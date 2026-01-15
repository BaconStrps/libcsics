#pragma once
#include <csics/radio/Radio.hpp>
#include <csics/queue/SPSCQueue.hpp>
#include <memory>

#ifdef CSICS_USE_UHD
#include <uhd/types/device_addr.hpp>
#endif

namespace csics::radio {


/** @brief Abstract base class for a radio receiver.
 * Abstracts over different radio hardware implementations.
 * Provides a common interface for opening the receiver.
 * Represents a single channel.
 * The returned queue from start_stream() will provide raw IQ samples.
 * The queue is owned by the RadioRx implementation and will be valid until the
 * radio is destroyed or start_stream() is called again.
 */
class IRadioRx {
   public:

    struct StartStatus;
    struct BlockHeader;
    using RxQueue = csics::queue::SPSCQueueBlockAdapter<BlockHeader, IQSample>;


    virtual ~IRadioRx() = default;

    /**
     * @brief Starts the radio stream.
     * @param stream_config Configuration for the stream.
     * @return StartStatus indicating success or failure, and the queue for
     * receiving samples. Will invalidate any previously returned queue.
     */
    virtual StartStatus start_stream(
        const StreamConfiguration& stream_config) noexcept = 0;

    /**
     * @brief Stops the radio stream.
     *
     * Stops the radio stream if it is currently streaming.
     * If the stream is not active, this function has no effect.
     * If the stream is active, no more samples will be produced after this
     * call. The queue still remains valid until the RadioRx object is
     * destroyed.
     */
    virtual void stop_stream() noexcept = 0;

    virtual bool is_streaming() const noexcept = 0;

    virtual double get_sample_rate() const noexcept = 0;
    virtual Timestamp set_sample_rate(double rate) noexcept = 0;
    virtual double get_max_sample_rate() const noexcept = 0;

    virtual double get_center_frequency() const noexcept = 0;
    virtual Timestamp set_center_frequency(double freq) noexcept = 0;

    virtual double get_gain() const noexcept = 0;
    virtual Timestamp set_gain(double gain) noexcept = 0;

    virtual RadioConfiguration get_configuration() const noexcept = 0;
    virtual Timestamp set_configuration(
        const RadioConfiguration& config) noexcept = 0;
    virtual RadioDeviceInfo get_device_info() const noexcept = 0;

    template <RadioDeviceArgsConvertible T>
    static std::unique_ptr<IRadioRx> create_radio_rx(
        T device_args, const RadioConfiguration& config);

    struct StartStatus {
        enum class Code {
            SUCCESS,
            HARDWARE_FAILURE,
            CONFIGURATION_ERROR,
        } code;
        RxQueue* queue = nullptr;

        operator bool() const noexcept {
            return code == Code::SUCCESS && queue != nullptr;
        }
    };

    struct BlockHeader {
        Timestamp timestamp_ns;  // Timestamp in nanoseconds since epoch. Derived
                                // from system clock.
        uint32_t num_samples;
        uint32_t reserved;
    };
};

};  // namespace csics::radio
