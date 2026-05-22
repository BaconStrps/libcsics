#pragma once
#include <concepts>
#include <random>

#include "csics/Buffer.hpp"
#include "csics/dsp/Transfer.hpp"
#include "csics/executor/Concept.hpp"
#include "csics/geo/Concepts.hpp"
#include "csics/geo/Coordinates.hpp"
#include "csics/geo/Ellipsoids.hpp"
#include "csics/geo/Ops.hpp"
#include "csics/linalg/Complex.hpp"
namespace csics::em {

constexpr double kSpeedOfLight = 299792458.0;  // m/s

constexpr auto to_watts(auto dbm) -> double {
    return std::pow(10.0, dbm / 10.0) / 1000.0;
}

constexpr auto to_dbm(auto watts) -> double {
    if (watts <= 0.0) {
        std::cerr
            << "Warning: Power in watts must be positive to convert to dBm. "
            << "Received: " << watts
            << ". Returning -1000 dBm as a practical approximation of "
               "negative infinity."
            << std::endl;
        return -1000.0;  // effectively negative infinity for practical purposes
                         // without introducing NaN or -inf which can cause
                         // issues in calculations
    }
    return 10.0 * std::log10(watts * 1000.0);
}

class Link;

struct PropagationResult {
    double power;             // watts
    double center_frequency;  // Hz
    double bandwidth;         // Hz
};

struct Propagation {
   public:
    double distance = 0.0;
    dsp::TransferFunction tf;
    Propagation() : distance(0.0), tf({}) {}
    Propagation(double delay, dsp::TransferFunction tf)
        : distance(delay), tf(std::move(tf)) {}

    Propagation operator*(const Propagation& other) const {
        auto d = distance + other.distance;
        auto combined_tf = tf * other.tf;
        CSICS_RUNTIME_ASSERT(combined_tf.has_value(),
                             "Failed to combine transfer functions");
        auto tf = std::move(combined_tf.value());
        return {d, std::move(tf)};
    }

    Propagation operator*(const dsp::TransferFunction& other) const {
        auto combined_tf = tf * other;
        CSICS_RUNTIME_ASSERT(combined_tf.has_value(),
                             "Failed to combine transfer functions");
        auto tf = std::move(combined_tf.value());
        return {distance, std::move(tf)};
    }

    Propagation& operator*=(const Propagation& other) {
        auto combined_tf = tf * other.tf;
        CSICS_RUNTIME_ASSERT(combined_tf.has_value(),
                             "Failed to combine transfer functions");
        tf = std::move(combined_tf.value());
        distance += other.distance;
        return *this;
    }

    PropagationResult result() const {
        double bw = tf.bandwidth();
        Buffer<float> psd(tf.samples().size());
        tf.to_psd(psd);
        float received_power = std::accumulate(psd.begin(), psd.end(), 0.0f) /
                               psd.size() * tf.bin_width() / bw;
        double center_frequency = tf.center_frequency();
        return {received_power, center_frequency, bw};
    }
};

template <typename T>
concept EnvironmentalModel = requires(T t, dsp::SpectralGrid freq_range) {
    {
        t.attenuate(double{}, freq_range)
    } -> std::convertible_to<dsp::TransferFunction>;
};

template <typename T>
concept NoiseModel = requires(T t, Range<float> range, float bin_width) {
    { t.sample() } -> std::convertible_to<linalg::Complex<float>>;
    {
        t.sample(range, bin_width)
    } -> std::convertible_to<Buffer<linalg::Complex<float>>>;
};

template <typename T>
concept Transmitter = requires(T t) {
    {
        t.transmit_towards(std::declval<geo::Geodetic<double, geo::WGS84>>())
    } -> std::same_as<Propagation>;

    {
        t.transmit_towards(std::declval<geo::Geocentric<double, geo::WGS84>>())
    } -> std::same_as<Propagation>;

    { t.location() } -> geo::GeospatialCoordinate;
};

template <typename T>
concept Receiver = requires(T t) {
    {
        t.receive_from(std::declval<geo::Geodetic<double, geo::WGS84>>())
    } -> std::same_as<Propagation>;

    {
        t.receive_from(std::declval<geo::Geocentric<double, geo::WGS84>>())
    } -> std::same_as<Propagation>;

    { t.receive() } -> std::same_as<Propagation>;
    { t.location() } -> geo::GeospatialCoordinate;
};

class AWGNoiseModel {
   public:
    AWGNoiseModel(double watts_per_hz, std::mt19937& rng)
        : rng_(rng), dist_(0.0f, std::sqrt(watts_per_hz / 2.0f)) {}

    linalg::Complex<float> sample() {
        return linalg::Complex<float>(dist_(rng_), dist_(rng_));
    }

    Buffer<linalg::Complex<float>> sample(Range<float> frequency_range,
                                          float bin_width) {
        std::size_t num_samples =
            static_cast<std::size_t>(frequency_range.size() / bin_width);
        Buffer<linalg::Complex<float>> samples(num_samples);
        for (std::size_t i = 0; i < num_samples; ++i) {
            samples[i] = sample();
        }
        return samples;
    }

   private:
    std::mt19937& rng_;
    std::normal_distribution<float> dist_;
};

// TODO: Implement
class ThermalNoiseModel;

class FreeSpaceEnvironmentalModel {
   public:
    dsp::TransferFunction attenuate(double distance,
                                    dsp::SpectralGrid grid) const {
        // Free space path loss: (4 * pi * d * f / c)^2
        // where d is distance, f is frequency, c is speed of light
        // We can represent this as a transfer function with a single band
        // that covers all frequencies and has the appropriate attenuation

        Buffer<linalg::Complex<float>> samples(grid.fft_size);
        for (std::size_t i = 0; i < grid.fft_size; ++i) {
            double freq =
                (static_cast<double>(i) / grid.fft_size) * grid.sample_rate_hz +
                grid.frequency_range().bottom();
            double path_loss =
                std::pow((4.0 * M_PI * distance * kSpeedOfLight/freq), 2);
            double phase_shift = -2.0 * M_PI * distance * freq / kSpeedOfLight;
            samples[i] =
                linalg::Polar<float>{1 / static_cast<float>(path_loss),
                                     static_cast<float>(phase_shift)};
        }
        return dsp::TransferFunction(grid, std::move(samples));
    }
};

class IsometricTransmitter {
   public:
    IsometricTransmitter(float power, float center_frequency, float bandwidth,
                         dsp::SpectralGrid grid,
                         geo::GeospatialCoordinate auto location)
        : power_(power),
          center_frequency_(center_frequency),
          bandwidth_(bandwidth),
          location_(geo::to_geodetic(location)),
          grid_(grid) {}

    auto transmit_towards(geo::GeospatialCoordinate auto coord) const {
        auto dist = geo::euclidean.distance(location_, coord);
        // auto geodetic = geo::to_geodetic(coord);

        return Propagation(dist, dsp::TransferFunction::ideal_bandpass(
                                     center_frequency_, bandwidth_, grid_) *
                                     linalg::Complex<float>{power_, 0.0f});
    }

    auto location() const { return location_; }

   private:
    float power_;             // watts
    float center_frequency_;  // Hz
    float bandwidth_;         // Hz
    geo::LatLongAlt<double> location_;
    dsp::SpectralGrid grid_;
};

class SincTransmitter {
   public:
    SincTransmitter(float power, float center_frequency, float bandwidth,
                    dsp::SpectralGrid grid, float hpbw, linalg::Degrees<float> beam_direction,
                    geo::GeospatialCoordinate auto location)
        : power_(power),
          center_frequency_(center_frequency),
          bandwidth_(bandwidth),
          hpbw_(hpbw),
          beam_direction_(beam_direction),
          location_(geo::to_geodetic(location)),
          grid_(grid) {}

    auto transmit_towards(geo::GeospatialCoordinate auto coord) const {
        auto azel = geo::az_el(location_, coord);
        auto dist = geo::euclidean.distance(location_, coord);
        double azimuth_diff = std::abs(beam_direction_ - azel.azimuth);
        return Propagation(dist, dsp::TransferFunction::ideal_bandpass(
                                     center_frequency_, bandwidth_, grid_) *
                                     sinc_pattern(azimuth_diff));
    }

    linalg::Complex<float> sinc_pattern(float angle_diff) const {
        auto sinc_arg = 1.39156f * std::sin(linalg::radians(angle_diff)) / (std::sin(linalg::Radians(hpbw_ / 2.0f)));
        auto sinc = std::abs(std::sin(sinc_arg) / sinc_arg);
        return linalg::Complex<float>{power_ * sinc, 0.0f};
    }

   private:
    float power_;             // watts
    float center_frequency_;  // Hz
    float bandwidth_;         // Hz
    float hpbw_;              // degrees
    linalg::Degrees<float> beam_direction_;    // degrees
    geo::LatLongAlt<double> location_;
    dsp::SpectralGrid grid_;
};

class IsometricReceiver {
   public:
    IsometricReceiver(double center_frequency, double bandwidth,
                      dsp::SpectralGrid grid,
                      geo::GeospatialCoordinate auto loc)
        : location_(loc),
          center_frequency_(center_frequency),
          bandwidth_(bandwidth),
          grid_(grid) {}

    Propagation receive_from(geo::GeospatialCoordinate auto) const {
        return receive();
    }

    Propagation receive() const {
        Propagation result =
            Propagation(0.0f, dsp::TransferFunction::ideal_bandpass(
                                  center_frequency_, bandwidth_, grid_));
        return result;
    }

    auto location() const { return location_; }

   private:
    geo::LatLongAlt<double> location_;
    float center_frequency_;  // Hz
    float bandwidth_;         // Hz
    dsp::SpectralGrid grid_;
};

struct LinkBudget {
    double received_power;      // dBm
    double noise_floor;         // dBm
    double interference_power;  // dBm
    double sinr;                // dB
};

struct LinkBudgetBatch {
    using ValueBuffer =
        Buffer<double, kCacheLineSize, CapacityPolicy::CacheAligned()>;
    ValueBuffer received_powers;
    ValueBuffer noise_floors;
    ValueBuffer interference_powers;
    ValueBuffer sinrs;

    LinkBudgetBatch(std::size_t size)
        : received_powers(size),
          noise_floors(size),
          interference_powers(size),
          sinrs(size) {}

    void push_back(const LinkBudget& budget) {
        received_powers.push_back(budget.received_power);
        noise_floors.push_back(budget.noise_floor);
        interference_powers.push_back(budget.interference_power);
        sinrs.push_back(budget.sinr);
    }
};

class Link {
   public:
    static PropagationResult apply(EnvironmentalModel auto channel,
                                   Transmitter auto tx, Receiver auto rx) {
        auto tx_propagation = tx.transmit_towards(rx.location());
        auto channel_propagation = channel.attenuate(tx_propagation.distance,
                                                     tx_propagation.tf.grid());
        auto rx_propagation = rx.receive();
        return (tx_propagation * channel_propagation * rx_propagation).result();
    }

    static LinkBudget budget(EnvironmentalModel auto channel,
                             BasicBufferView<PropagationResult> desireds,
                             BasicBufferView<PropagationResult> interferers) {
        CSICS_RUNTIME_ASSERT(desireds.size() > 0,
                             "At least one desired signal is required");

        double total_interference_power = 0.0;
        double total_desired_power = 0.0;
        double avg_center_frequency = 0.0;
        double avg_bandwidth = 0.0;
        for (const auto& interferer : interferers) {
            total_interference_power += interferer.power;
            avg_center_frequency += interferer.center_frequency;
            avg_bandwidth += interferer.bandwidth;
        }

        for (const auto& desired : desireds) {
            total_desired_power += desired.power;
            avg_center_frequency += desired.center_frequency;
            avg_bandwidth += desired.bandwidth;
        }

        avg_center_frequency /= (desireds.size() + interferers.size());
        avg_bandwidth /= (desireds.size() + interferers.size());

        double noise_floor =
            channel.noise_floor(avg_center_frequency - avg_bandwidth / 2.0,
                                avg_center_frequency + avg_bandwidth / 2.0);

        return LinkBudget{
            .received_power = to_dbm(total_desired_power),
            .noise_floor = noise_floor,
            .interference_power = to_dbm(total_interference_power),
            .sinr = to_dbm(total_desired_power /
                           (to_watts(noise_floor) + total_interference_power))};
    }

    template <Transmitter Tx, Receiver Rx>
    static LinkBudgetBatch apply_and_budget(EnvironmentalModel auto channel,
                                            Buffer<Tx>& priority_txs,
                                            Buffer<Tx>& interferer_txs,
                                            Buffer<Rx>& rxs,
                                            executor::Executor auto&) {
        LinkBudgetBatch batch(rxs.size());
        // This is always true for now as the batch is in development
        // TODO: implement a heuristic to determine when to batch and when to do
        // sequentially
        if (rxs.size() * (priority_txs.size() + interferer_txs.size()) < 0) {
            Buffer<PropagationResult> desireds(priority_txs.size());
            Buffer<PropagationResult> interferers(interferer_txs.size());

            for (std::size_t i = 0; i < rxs.size(); ++i) {
                auto& rx = rxs[i];

                for (std::size_t j = 0; j < priority_txs.size(); ++j) {
                    desireds[j] = apply(channel, priority_txs[j], rx);
                }

                for (std::size_t k = 0; k < interferer_txs.size(); ++k) {
                    interferers[k] = apply(channel, interferer_txs[k], rx);
                }

                LinkBudget budget = budget(channel, desireds, interferers);
                batch.push_back(budget);
            }
            return batch;
        }

        // executor batch processing tbd
        return batch;
    }
};
};  // namespace csics::em
