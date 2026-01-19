#include "USRPRadioRx.hpp"
#include <uhd/usrp/usrp.h>

namespace csics::radio {

USRPRadioRx::~USRPRadioRx() {
    stop_stream();
    if (rx_queue_ != nullptr) delete rx_queue_;
    if (queue_ != nullptr) delete queue_;
};

USRPRadioRx::USRPRadioRx(const RadioDeviceArgs& device_args)
    : queue_(nullptr),
      rx_queue_(nullptr),
      block_len_(0),
      usrp_(nullptr) {

    auto err = uhd_usrp_make(&usrp_, std::get<UsrpArgs>(device_args.args).device_args);
    if (err != UHD_ERROR_NONE) {
        // Do error logging here eventually 
        char err_str[256];
        uhd_get_last_error(err_str, 256);
        throw std::runtime_error(err_str);
    }
    err = uhd_rx_streamer_make(&rx_streamer_);
    if (err != UHD_ERROR_NONE) {
        // Do error logging here eventually 
        char err_str[256];
        uhd_get_last_error(err_str, 256);
        throw std::runtime_error(err_str);
    }
}

USRPRadioRx::StartStatus USRPRadioRx::start_stream(
    const StreamConfiguration& stream_config) noexcept {
    if (is_streaming()) {
        stop_stream();
        delete rx_queue_;
        delete queue_;
        queue_ = nullptr;
        rx_queue_ = nullptr;
    }

    block_len_ = stream_config.sample_length.get_num_samples(
        current_config_.sample_rate);
    queue_ = new csics::queue::SPSCQueue(block_len_ * 4 *
                                         sizeof(std::complex<int16_t>));
    uhd_stream_args_t stream_args {};
    stream_args.otw_format = const_cast<char*>("sc16");
    stream_args.cpu_format = const_cast<char*>("sc16");
    stream_args.n_channels = 1;
    stream_args.channel_list = {0};
    auto err = uhd_usrp_get_rx_stream(usrp_, &stream_args, rx_streamer_);
    if (err != UHD_ERROR_NONE) {
        delete queue_;
        queue_ = nullptr;
        return {StartStatus::Code::HARDWARE_FAILURE, nullptr};
    }

    streaming_.store(true, std::memory_order_release);
    rx_thread_ = std::thread(&USRPRadioRx::rx_loop, this);
    rx_queue_ = new RxQueue(*queue_);
    return {StartStatus::Code::SUCCESS, rx_queue_};
}

bool USRPRadioRx::is_streaming() const noexcept {
    return streaming_.load(std::memory_order_acquire);
}

double USRPRadioRx::get_sample_rate() const noexcept {
    return current_config_.sample_rate;
}

double USRPRadioRx::get_max_sample_rate() const noexcept { return 0; }

double USRPRadioRx::get_center_frequency() const noexcept {
    return current_config_.center_frequency;
}

double USRPRadioRx::get_gain() const noexcept { return current_config_.gain; }

// When setting certain parameters,
// we might need to stop and restart the stream.
// or maybe just dump a certain amount of samples.
// idk yet.

Timestamp USRPRadioRx::set_gain(double gain) noexcept {
    uhd_usrp_set_rx_gain(usrp_, gain, 0, nullptr);
    uhd_usrp_get_rx_gain(usrp_, 0, nullptr, &gain);
    current_config_.gain = gain;
    return Timestamp::now();
}

Timestamp USRPRadioRx::set_sample_rate(double rate) noexcept {
    uhd_usrp_set_rx_rate(usrp_, rate, 0);
    uhd_usrp_get_rx_rate(usrp_, 0, &rate);
    current_config_.sample_rate = rate;
    return Timestamp::now();
}

Timestamp USRPRadioRx::set_center_frequency(double freq) noexcept {
    uhd_tune_request_t tune_req{};
    tune_req.target_freq = freq;
    tune_req.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
    tune_req.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
    uhd_tune_result_t tune_res{};
    uhd_usrp_set_rx_freq(usrp_, &tune_req, 0, &tune_res);
    current_config_.center_frequency = tune_res.actual_rf_freq;

    return Timestamp::now();
}

void USRPRadioRx::stop_stream() noexcept {
    if (is_streaming()) {
        stop_signal_.store(true, std::memory_order_release);
        if (rx_thread_.joinable()) {
            rx_thread_.join();
        }
        streaming_.store(false, std::memory_order_release);
        stop_signal_.store(false, std::memory_order_release);
    }
}

// may need to optimize later just in case we need to update multiple params
Timestamp USRPRadioRx::set_configuration(const RadioConfiguration& config) noexcept {
    if (config.sample_rate != current_config_.sample_rate)
        set_sample_rate(config.sample_rate);
    if (config.center_frequency != current_config_.center_frequency)
        set_center_frequency(config.center_frequency);
    if (config.gain != current_config_.gain) set_gain(config.gain);
    current_config_ = config;
    return Timestamp::now();
}

RadioConfiguration USRPRadioRx::get_configuration() const noexcept {
    return current_config_;
}

RadioDeviceInfo USRPRadioRx::get_device_info() const noexcept {
    RadioDeviceInfo info {};
    uhd_meta_range_handle range_handle;
    uhd_meta_range_make(&range_handle);
    uhd_usrp_get_rx_freq_range(usrp_, 0, range_handle);
    uhd_meta_range_start(range_handle, &info.frequency_range.min);
    uhd_meta_range_stop(range_handle, &info.frequency_range.max);
    uhd_meta_range_free(&range_handle);
    uhd_meta_range_make(&range_handle);
    uhd_usrp_get_rx_rates(usrp_, 0, range_handle);
    uhd_meta_range_start(range_handle, &info.sample_rate_range.min);
    uhd_meta_range_stop(range_handle, &info.sample_rate_range.max);
    uhd_meta_range_free(&range_handle);
    uhd_meta_range_make(&range_handle);
    uhd_usrp_get_rx_gain_range(usrp_, nullptr, 0, range_handle);
    uhd_meta_range_stop(range_handle, &info.max_gain);
    uhd_meta_range_free(&range_handle);
    return info;
}

void USRPRadioRx::rx_loop() noexcept {
    RxQueue::AdaptedSlot slot {};
    IQSample* cursor = nullptr;
	uhd_rx_metadata_handle md;
    while (!stop_signal_.load(std::memory_order_acquire)) {
        if (!rx_queue_->acquire_write(slot, block_len_)) {
            break;
        }
        cursor = slot.data;
        auto hdr = slot.header;
        if (!hdr) {
            break;
        }
        hdr->timestamp_ns = Timestamp::now();
        hdr->num_samples = slot.size;
        uhd_rx_metadata_make(&md);
        size_t num_rx_samps = 0;
        while (cursor < slot.data + slot.size) {
            uhd_rx_streamer_recv(rx_streamer_, reinterpret_cast<void**>(&slot.data),
                                 slot.size - (cursor - slot.data), &md, 0.1, false, &num_rx_samps);
            cursor += num_rx_samps;
        }
        rx_queue_->commit_write(slot);
    }
    uhd_rx_metadata_free(&md);
}

};  // namespace csics::radio
