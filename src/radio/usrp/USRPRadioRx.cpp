#include <csics/radio/usrp/USRPRadioRx.hpp>

namespace csics::radio {

const char* data_type_to_uhd(StreamDataType data_type) {
    switch (data_type) {
        case StreamDataType::SC8:
            return "sc8";
        case StreamDataType::SC16:
            return "sc16";
        case StreamDataType::FC32:
            return "fc32";
        default:
            return "sc16";  // default to sc16
    }
}

USRPRadioRx::~USRPRadioRx() {
    stop_stream();
    if (rx_queue_ != nullptr) delete rx_queue_;
    if (queue_ != nullptr) delete queue_;
};

USRPRadioRx::USRPRadioRx(const RadioDeviceArgs& device_args)
    : queue_(nullptr),
      rx_queue_(nullptr),
      usrp_(uhd::usrp::multi_usrp::make(
          std::get<UsrpArgs>(device_args.args).device_addr)) {}

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
    rx_streamer_ = usrp_->get_rx_stream(
        uhd::stream_args_t(data_type_to_uhd(stream_config.data_type), "sc16"));

    if (!rx_streamer_) {
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
    current_config_.gain = gain;
    usrp_->set_rx_gain(gain);
    return Timestamp::now();
}

Timestamp USRPRadioRx::set_sample_rate(double rate) noexcept {
    current_config_.sample_rate = rate;
    usrp_->set_rx_rate(rate);
    return Timestamp::now();
}

Timestamp USRPRadioRx::set_center_frequency(double freq) noexcept {
    current_config_.center_frequency = freq;
    usrp_->set_rx_freq(freq);
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

void USRPRadioRx::rx_loop() noexcept {
    RxQueue::AdaptedSlot slot {};
    IQSample* cursor = nullptr;
    while (!stop_signal_.load(std::memory_order_acquire)) {
        if (!rx_queue_->acquire_write(slot, block_len_)) {
            break;
        }
        cursor = slot.data;
        auto hdr = slot.header;
        hdr->timestamp_ns = Timestamp::now();
        hdr->num_samples = slot.size;
        hdr->reserved = 0;
        while (cursor < slot.data + slot.size) {
            uhd::rx_metadata_t md;
            size_t num_rx_samps = rx_streamer_->recv(
                cursor, slot.size - (cursor - slot.data), md);
            cursor += num_rx_samps;
        }
        rx_queue_->commit_write(slot);
    }
}

};  // namespace csics::radio
