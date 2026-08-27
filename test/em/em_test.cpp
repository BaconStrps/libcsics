// EM module tests — exercises the RF link calculation API in csics::em.
//
// Test suite setup: 1 GHz center frequency, 1 MHz bandwidth, 10 W transmitter,
// 1024-point FFT grid with 4 MHz sample rate. Positions are WGS84 geodetic
// (latitude, longitude, altitude_m).
//
// PowerConversions
//   Verifies to_dbm / to_watts utilities: 1 W = 30 dBm, 1 mW = 0 dBm,
//   round-trip lossless, and that zero/negative input returns the -1000 dBm
//   sentinel instead of NaN or -inf.
//
// IsometricLinkReceivesPower
//   Basic end-to-end smoke test. TX and RX ~10 km apart. Asserts received power
//   is positive, less than TX power (attenuation occurred), and center frequency
//   is preserved through the link.
//
// PowerDecreasesWithDistance
//   Three receivers at increasing ranges (~1.1, ~5.6, ~11.1 km east). Asserts
//   received power is strictly monotonically decreasing with distance.
//
// DoublingDistanceReducesPowerSixteenFold
//   FreeSpaceEnvironmentalModel uses path_loss = (4pi*d*c/f)^2. Because
//   to_psd calls linalg::norm (magnitude-squared), received power is
//   proportional to 1/d^4. Doubling distance must produce 16x less power.
//
// SincTransmitterOnAxisStrongerThanOffAxis
//   30 deg HPBW beam pointing East (90 deg azimuth). Receiver due east is
//   on-axis (0 deg off boresight); receiver due north is 90 deg off boresight.
//   Asserts on-axis received power exceeds off-axis received power.

#include <gtest/gtest.h>

#include <csics/dsp/Transfer.hpp>
#include <csics/em/ElectroMagnetics.hpp>
#include <csics/geo/Coordinates.hpp>
#include <csics/linalg/Coordinates.hpp>

using namespace csics;
using namespace csics::em;
using namespace csics::geo;

static constexpr double   kFreqHz      = 1.0e9;     // 1 GHz
static constexpr double   kBandwidthHz = 1.0e6;     // 1 MHz
static constexpr uint64_t kSampleRate  = 4000000;   // 4 MHz (4x Nyquist)
static constexpr uint32_t kFftSize     = 1024;
static constexpr float    kTxPowerW    = 10.0f;     // 10 W

static dsp::SpectralGrid make_grid() {
    return dsp::SpectralGrid::centered(kFftSize, kFreqHz, kSampleRate);
}

TEST(EMTests, PowerConversions) {
    EXPECT_NEAR(to_dbm(1.0), 30.0, 1e-6);
    EXPECT_NEAR(to_dbm(0.001), 0.0, 1e-6);
    EXPECT_NEAR(to_watts(30.0), 1.0, 1e-9);
    EXPECT_NEAR(to_watts(0.0), 0.001, 1e-9);
    EXPECT_DOUBLE_EQ(to_dbm(0.0), -1000.0);
    EXPECT_DOUBLE_EQ(to_dbm(-5.0), -1000.0);
}

TEST(EMTests, IsometricLinkReceivesPower) {
    auto grid = make_grid();
    Geodetic<double, WGS84> tx_pos(0.0, 0.0, 0.0);
    Geodetic<double, WGS84> rx_pos(0.0, 0.09, 0.0);  // ~10 km east

    IsometricTransmitter tx(kTxPowerW, kFreqHz, kBandwidthHz, grid, tx_pos);
    IsometricReceiver rx(kFreqHz, kBandwidthHz, grid, rx_pos);
    FreeSpaceEnvironmentalModel channel;

    auto result = Link::apply(channel, tx, rx);

    EXPECT_GT(result.power, 0.0);
    EXPECT_LT(result.power, static_cast<double>(kTxPowerW));
    EXPECT_NEAR(result.center_frequency, kFreqHz, 1.0);
}

TEST(EMTests, PowerDecreasesWithDistance) {
    auto grid = make_grid();
    Geodetic<double, WGS84> tx_pos(0.0, 0.0, 0.0);
    Geodetic<double, WGS84> rx_near(0.0, 0.01, 0.0);  // ~1.1 km east
    Geodetic<double, WGS84> rx_mid (0.0, 0.05, 0.0);  // ~5.6 km east
    Geodetic<double, WGS84> rx_far (0.0, 0.10, 0.0);  // ~11.1 km east

    FreeSpaceEnvironmentalModel channel;
    IsometricTransmitter tx(kTxPowerW, kFreqHz, kBandwidthHz, grid, tx_pos);

    auto r_near = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_near));
    auto r_mid  = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_mid));
    auto r_far  = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_far));

    EXPECT_GT(r_near.power, r_mid.power);
    EXPECT_GT(r_mid.power, r_far.power);
}

TEST(EMTests, DoublingDistanceReducesPowerSixteenFold) {
    // path_loss = (4π*d*c/f)²; after norm(), received power ∝ 1/d⁴.
    // Doubling distance → 16× power reduction.
    auto grid = make_grid();
    Geodetic<double, WGS84> tx_pos(0.0, 0.0, 0.0);
    Geodetic<double, WGS84> rx_1x(0.0, 0.01, 0.0);  // ~1.11 km east
    Geodetic<double, WGS84> rx_2x(0.0, 0.02, 0.0);  // ~2.22 km east

    FreeSpaceEnvironmentalModel channel;
    IsometricTransmitter tx(kTxPowerW, kFreqHz, kBandwidthHz, grid, tx_pos);

    auto r1 = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_1x));
    auto r2 = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_2x));

    double ratio = r1.power / r2.power;
    EXPECT_NEAR(ratio, 16.0, 1.0);  // 10% tolerance for coordinate non-linearity
}

TEST(EMTests, SincTransmitterOnAxisStrongerThanOffAxis) {
    // 30° HPBW beam pointing East (90°). Equal-distance receivers:
    //   rx_east  is on-axis  (azimuth ≈ 90° from TX → 0° off boresight)
    //   rx_north is off-axis (azimuth ≈ 0°  from TX → 90° off boresight)
    auto grid = make_grid();
    Geodetic<double, WGS84> tx_pos(0.0, 0.0, 0.0);
    Geodetic<double, WGS84> rx_east (0.0,  0.01, 0.0);
    Geodetic<double, WGS84> rx_north(0.01, 0.0,  0.0);

    FreeSpaceEnvironmentalModel channel;
    SincTransmitter tx(kTxPowerW, kFreqHz, kBandwidthHz, grid,
                       30.0f, linalg::Degrees<float>{90.0f}, tx_pos);

    auto r_on  = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_east));
    auto r_off = Link::apply(channel, tx, IsometricReceiver(kFreqHz, kBandwidthHz, grid, rx_north));

    EXPECT_GT(r_on.power, r_off.power);
}
