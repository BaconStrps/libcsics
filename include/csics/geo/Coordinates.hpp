#pragma once
#include "csics/geo/Concepts.hpp"
#include "csics/geo/Ellipsoids.hpp"
#include "csics/linalg/Coordinates.hpp"

namespace csics::geo {

template <typename T, Ellipsoid E = WGS84>
class Geodetic {
   public:
    using value_type = T;
    using ellipsoid_v = E;
    constexpr Geodetic(T latitude, T longitude, T altitude)
        : latitude_(latitude), longitude_(longitude), altitude_(altitude) {}
    constexpr Geodetic() : latitude_(0), longitude_(0), altitude_(0) {}

    constexpr const linalg::Degrees<T>& latitude() const & noexcept { return latitude_; }
    constexpr const linalg::Degrees<T>& longitude() const & noexcept { return longitude_; }
    constexpr const T& altitude() const & noexcept { return altitude_; }

    constexpr linalg::Degrees<T>& latitude() & noexcept { return latitude_; }
    constexpr linalg::Degrees<T>& longitude() & noexcept { return longitude_; }
    constexpr T& altitude() & noexcept { return altitude_; }

    constexpr linalg::Degrees<T>&& latitude() && noexcept { return latitude_; }
    constexpr linalg::Degrees<T>&& longitude() && noexcept { return longitude_; }
    constexpr T&& altitude() && noexcept { return altitude_; }
    
    bool operator==(const Geodetic& other) const {
        return latitude_ == other.latitude_ && longitude_ == other.longitude_ &&
               altitude_ == other.altitude_;
    }

    bool operator!=(const Geodetic& other) const { return !(*this == other); }

   private:
    linalg::Degrees<T> latitude_;
    linalg::Degrees<T> longitude_;
    T altitude_;
};

static_assert(GeodeticCoordinate<Geodetic<double, WGS84>, WGS84>,
              "Geodetic does not satisfy GeodeticCoordinate concept");
static_assert(GeodeticLike<Geodetic<double, WGS84>>,
              "Geodetic does not satisfy GeodeticLike concept");
static_assert(GeospatialCoordinate<Geodetic<double, WGS84>>,
              "Geodetic does not satisfy GeospatialCoordinate concept");
static_assert(!GeocentricCoordinate<Geodetic<double, WGS84>, WGS84>,
              "Geodetic should not satisfy GeocentricCoordinate concept");
static_assert(!GeocentricLike<Geodetic<double, WGS84>>,
              "Geodetic should not satisfy GeocentricLike concept");

template <typename T, Ellipsoid E = WGS84>
using LatLongAlt = Geodetic<T, E>;

template <typename T, Ellipsoid E = WGS84>
class Geocentric {
   public:
    using value_type = T;
    using ellipsoid_v = E;
    static constexpr auto cols_v = linalg::Vec3<T>::cols_v;
    static constexpr auto rows_v = linalg::Vec3<T>::rows_v;
    constexpr Geocentric(T x, T y, T z) : x_(x), y_(y), z_(z) {}
    constexpr Geocentric() : x_(0), y_(0), z_(0) {}
    constexpr Geocentric(const linalg::Vec3<T>& vec)
        : x_(std::get<0>(vec)), y_(std::get<1>(vec)), z_(std::get<2>(vec)) {}

    constexpr const T x() const { return x_; }
    constexpr const T y() const { return y_; }
    constexpr const T z() const { return z_; }

    constexpr T x() { return x_; }
    constexpr T y() { return y_; }
    constexpr T z() { return z_; }

    bool operator==(const Geocentric& other) const {
        return x_ == other.x_ && y_ == other.y_ && z_ == other.z_;
    }

    bool operator!=(const Geocentric& other) const { return !(*this == other); }

    operator linalg::Vec3<T>() const { return linalg::Vec3<T>(x_, y_, z_); }

    template <std::size_t I>
    constexpr const T& get() const noexcept {
        if constexpr (I == 0)
            return x_;
        else if constexpr (I == 1)
            return y_;
        else if constexpr (I == 2)
            return z_;
        else
            static_assert(I < 3, "Index out of bounds");
    }

    const T& operator[](std::size_t i) const& {
        if (i == 0)
            return x_;
        else if (i == 1)
            return y_;
        else
            return z_;
    }

    T&& operator[](std::size_t i) && {
        if (i == 0)
            return std::move(x_);
        else if (i == 1)
            return std::move(y_);
        else
            return std::move(z_);
    }

    T& operator[](std::size_t i) & {
        if (i == 0)
            return x_;
        else if (i == 1)
            return y_;
        else
            return z_;
    }

   private:
    T x_;
    T y_;
    T z_;
};

template <typename T, Ellipsoid E = WGS84>
using UTM = Geocentric<T, E>;

static_assert(GeocentricCoordinate<Geocentric<double, WGS84>, WGS84>,
              "Geocentric does not satisfy GeocentricCoordinate concept");
static_assert(GeocentricLike<Geocentric<double, WGS84>>,
              "Geocentric does not satisfy GeocentricLike concept");
static_assert(GeospatialCoordinate<Geocentric<double, WGS84>>,
                "Geocentric does not satisfy GeospatialCoordinate concept");
static_assert(!GeodeticCoordinate<Geocentric<double, WGS84>, WGS84>,
              "Geocentric should not satisfy GeodeticCoordinate concept");
static_assert(!GeodeticLike<Geocentric<double, WGS84>>,
                "Geocentric should not satisfy GeodeticLike concept");

static_assert(GeodeticCoordinate<LatLongAlt<double, WGS84>, WGS84>,
              "LatLongAlt does not satisfy GeodeticCoordinate concept");


template <typename T, Ellipsoid E = WGS84>
class ENU {
    public:
    using value_type = T;
    using ellipsoid_v = E;
    static constexpr auto cols_v = linalg::Vec3<T>::cols_v;
    static constexpr auto rows_v = linalg::Vec3<T>::rows_v;

    constexpr ENU(T east, T north, T up) : east_(east), north_(north), up_(up) {}
    constexpr ENU() : east_(0), north_(0), up_(0) {}

    constexpr const T east() const { return east_; }
    constexpr const T north() const { return north_; }
    constexpr const T up() const { return up_; }

    private:
    T east_;
    T north_;
    T up_;
};



};  // namespace csics::geo

namespace csics::geo {
    // oh boy operators

template <typename T, Ellipsoid E>
linalg::Vec3<T> operator-(const Geocentric<T, E>& a, const Geocentric<T, E>& b) {
    return linalg::Vec3<T>(a) - linalg::Vec3<T>(b);
};

};
