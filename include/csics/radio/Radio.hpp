#pragma once

#include <chrono>
#include <variant>
#ifdef CSICS_USE_UHD
#include <uhd/types/device_addr.hpp>
#endif
namespace csics::radio {
/** @brief Configuration parameters for the radio receiver. */
struct RadioConfiguration {
    // Sample rate in Hz.
    double sample_rate = 1e6;
    // Center frequency in Hz.
    double center_frequency = 2.437e9;
    // Gain in dB.
    double gain = 0.0;
    // Channel bandwidth in Hz.
    double channel_bandwidth = 1e6;
};

struct RadioDeviceInfo {
    struct {
        double min;
        double max;
    } frequency_range;
    struct {
        double min;
        double max;
    } sample_rate_range;
    double max_gain;
};

enum class DeviceType {
    DEFAULT,
#ifdef CSICS_USE_UHD
    USRP,
#endif
};

struct RadioDeviceArgs;

#ifdef CSICS_USE_UHD
struct UsrpArgs {
    uhd::device_addr_t device_addr;
    UsrpArgs() = default;
    UsrpArgs(const uhd::device_addr_t& addr) : device_addr(addr) {}
    operator RadioDeviceArgs() const;
};
#endif

struct RadioDeviceArgs {
    DeviceType device_type;
    std::variant<
#ifdef CSICS_USE_UHD
        UsrpArgs,
#endif
        std::monostate>
        args;
    RadioDeviceArgs();
};

/**
 * @brief Defines the host-side data type for IQ samples.
 */
enum class StreamDataType {
    SC16,  // 16-bit signed integer complex, IQ interleaved
};

struct SampleLength {
    enum class Type {
        NUM_SAMPLES,
        DURATION,
    } type;
    union {
        std::size_t num_samples = 0;
        std::chrono::nanoseconds duration;
    };

    std::size_t get_num_samples(double sample_rate) const {
        if (type == Type::NUM_SAMPLES) {
            return num_samples;
        } else {
            return static_cast<std::size_t>((duration.count() / 1e9) *
                                            sample_rate);
        }
    }
    std::size_t get_num_bytes(double sample_rate,
                              StreamDataType data_type) const {
        std::size_t samples = get_num_samples(sample_rate);
        std::size_t bytes_per_sample = 0;
        switch (data_type) {
            case StreamDataType::SC16:
                bytes_per_sample = 4;  // I + Q each 2 bytes
                break;
        }
        return samples * bytes_per_sample;
    }
};

struct Timestamp {
    uint64_t nanoseconds_since_epoch = 0;

    Timestamp(uint64_t t) : nanoseconds_since_epoch(t) {}

    template <typename Duration>
    Timestamp(std::chrono::time_point<std::chrono::system_clock, Duration> tp)
        : nanoseconds_since_epoch(
              static_cast<uint64_t>(tp.time_since_epoch().count())) {}

    static Timestamp now() {
        return Timestamp{static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count())};
    }

    operator uint64_t() const { return nanoseconds_since_epoch; }
    template<typename Clock>

    operator std::chrono::time_point<Clock, std::chrono::nanoseconds>() const {
        return std::chrono::time_point<Clock, std::chrono::nanoseconds>(
            std::chrono::nanoseconds(nanoseconds_since_epoch));
    }
};

struct StreamConfiguration {
    StreamDataType data_type = StreamDataType::SC16;
    SampleLength sample_length = {SampleLength::Type::NUM_SAMPLES, {1024}};
};
template <typename T>
concept RadioDeviceArgsConvertible =
    std::same_as<T, RadioDeviceArgs> || std::convertible_to<T, RadioDeviceArgs>;
};  // namespace csics::radio

using IQSample = std::complex<int16_t>;
