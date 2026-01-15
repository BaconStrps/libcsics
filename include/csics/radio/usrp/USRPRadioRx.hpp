#pragma once
#include <csics/radio/RadioRx.hpp>
#include <uhd/usrp/multi_usrp.hpp>

namespace csics::radio {
class USRPRadioRx : public IRadioRx {
   public:
    explicit USRPRadioRx(const RadioDeviceArgs& device_args);
    ~USRPRadioRx() override;
    StartStatus start_stream(
        const StreamConfiguration& stream_config) noexcept override;

    void stop_stream() noexcept override;

    bool is_streaming() const noexcept override;

    double get_sample_rate() const noexcept override;
    Timestamp set_sample_rate(double rate) noexcept override;
    double get_max_sample_rate() const noexcept override;

    double get_center_frequency() const noexcept override;
    Timestamp set_center_frequency(double freq) noexcept override;

    double get_gain() const noexcept override;
    Timestamp set_gain(double gain) noexcept override;

    RadioConfiguration get_configuration() const noexcept override;
    Timestamp set_configuration(const RadioConfiguration& config) noexcept override;
    RadioDeviceInfo get_device_info() const noexcept override;
   private:
    queue::SPSCQueue* queue_;
    RxQueue* rx_queue_;
    RadioConfiguration current_config_;
    uhd::usrp::multi_usrp::sptr usrp_;
    uhd::rx_streamer::sptr rx_streamer_;
    std::thread rx_thread_;
    std::size_t block_len_;
    

    std::atomic<bool> streaming_;
    std::atomic<bool> stop_signal_{false};

    void rx_loop() noexcept;
};
};  // namespace csics::radio
