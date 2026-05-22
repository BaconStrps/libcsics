

#pragma once
#include "csics/geo/Concepts.hpp"
#include "csics/geo/Coordinates.hpp"
#include "csics/linalg/Coordinates.hpp"
#include "csics/linalg/Ops.hpp"

namespace csics::geo {
class ToGeodetic {
   public:
    template <GeospatialCoordinate C>
    constexpr Geodetic<typename C::value_type, typename C::ellipsoid_v>
    operator()(const C& geodetic) const {
        return apply(geodetic);
    }
    template <GeocentricLike C>
    static auto apply(C const& geocentric) {
        double latitude, longitude, altitude;
        using E = typename C::ellipsoid_v;
        E e{};
        geocentric_to_geodetic(geocentric.x(), geocentric.y(), geocentric.z(),
                               e.a, e.f, latitude, longitude, altitude);

        return Geodetic<typename C::value_type, typename C::ellipsoid_v>{
            latitude, longitude, altitude};
    }

    static auto apply(GeodeticLike auto const& geodetic) { return geodetic; }

   private:
    static void geocentric_to_geodetic(const double x, const double y,
                                       const double z, const double a,
                                       const double f, double& latitude,
                                       double& longitude, double& altitude);
};

constexpr ToGeodetic to_geodetic{};

class ToGeocentric {
   public:
    template <GeospatialCoordinate C>
    constexpr Geocentric<typename C::value_type, typename C::ellipsoid_v>
    operator()(C const& val) const {
        return apply(val);
    }

    template <GeodeticLike C>
    static auto apply(C const& geodetic) {
        double x, y, z;
        using E = typename C::ellipsoid_v;
        E e{};
        geodetic_to_geocentric(geodetic.latitude(), geodetic.longitude(),
                               geodetic.altitude(), e.a, e.f, x, y, z);
        return Geocentric<typename C::value_type, typename C::ellipsoid_v>{x, y,
                                                                           z};
    }

    static auto apply(GeocentricLike auto const& geocentric) {
        return geocentric;
    }

   private:
    static void geodetic_to_geocentric(const double latitude,
                                       const double longitude,
                                       const double altitude, const double a,
                                       const double f, double& x, double& y,
                                       double& z);
};

constexpr ToGeocentric to_geocentric{};

class ToENU {
   public:
    template <GeospatialCoordinate C>
    constexpr ENU<typename C::value_type, typename C::ellipsoid_v> operator()(
        C const& val) const {
        return apply(val);
    }

    template <GeodeticLike C>
    static auto apply(C const& geodetic) {
        auto geocentric = to_geocentric(geodetic);
        return to_enu(geocentric);
    }

    template <GeocentricLike C>
    static auto apply(C const& geocentric) {
        return to_enu(geocentric);
    }

   private:
    template <GeodeticLike C>
    static auto to_enu(C const& geodetic) {
        constexpr linalg::Matrix<typename C::value_type, 3, 3>
            ecef_to_enu_rotation = [&]() {
                double lat_rad = geodetic.latitude() * M_PI / 180.0;
                double lon_rad = geodetic.longitude() * M_PI / 180.0;
                double sin_lat = std::sin(lat_rad);
                double cos_lat = std::cos(lat_rad);
                double sin_lon = std::sin(lon_rad);
                double cos_lon = std::cos(lon_rad);
                return linalg::Matrix<typename C::value_type, 3, 3>{
                    {-sin_lon, cos_lon, 0},
                    {-sin_lat * cos_lon, -sin_lat * sin_lon, cos_lat},
                    {cos_lat * cos_lon, cos_lat * sin_lon, sin_lat}};
            }();

        auto vec = ecef_to_enu_rotation * to_geocentric(geodetic);

        return ENU<typename C::value_type, typename C::ellipsoid_v>(
            vec.x(), vec.y(), vec.z());
    }
};

template <typename T>
struct AzEl {
    linalg::Radians<T> azimuth;
    linalg::Radians<T> elevation;
};

class AzElOp {
   public:
    template <typename C1, typename C2>
        requires geo::GeospatialCoordinate<C1> &&
                 geo::GeospatialCoordinate<C2> &&
                 std::same_as<typename C1::ellipsoid_v,
                              typename C2::ellipsoid_v>
    auto operator()(C1 const& c1, C2 const& c2) const {
        return apply(c1, c2);
    }

    template <typename C1, typename C2>
        requires geo::GeospatialCoordinate<C1> &&
                 geo::GeospatialCoordinate<C2> &&
                 std::same_as<typename C1::ellipsoid_v,
                              typename C2::ellipsoid_v>
    static auto apply(C1 const& c1, C2 const& c2) {
        linalg::Matrix<typename C1::value_type, 3, 3>
            ecef_to_enu_rotation = [&]() {
                auto geodetic = to_geodetic(c1);
                double lat_rad = linalg::Radians<double>(geodetic.latitude());
                double lon_rad = linalg::Radians<double>(geodetic.longitude());
                double sin_lat = std::sin(lat_rad);
                double cos_lat = std::cos(lat_rad);
                double sin_lon = std::sin(lon_rad);
                double cos_lon = std::cos(lon_rad);
                return linalg::Matrix<typename C1::value_type, 3, 3>{
                    {-sin_lon, cos_lon, 0},
                    {-sin_lat * cos_lon, -sin_lat * sin_lon, cos_lat},
                    {cos_lat * cos_lon, cos_lat * sin_lon, sin_lat}};
            }();

        auto ecef_1 = to_geocentric(c1);
        auto ecef_2 = to_geocentric(c2);

        auto vec = ecef_to_enu_rotation * (ecef_2 - ecef_1);

        return AzEl<typename C1::value_type>{
            std::atan2(std::get<0>(vec), std::get<1>(vec)),
            std::atan2(std::get<2>(vec), std::hypot(std::get<0>(vec), std::get<1>(vec)))};
    }
};

constexpr AzElOp az_el{};

class Euclidean {
   public:
    template <typename C1, typename C2>
        requires geo::GeospatialCoordinate<C1> &&
                 geo::GeospatialCoordinate<C2> &&
                 std::same_as<typename C1::ellipsoid_v,
                              typename C2::ellipsoid_v>
    static auto distance(const C1& c1, const C2& c2) {
        return linalg::mag(vector(c1, c2));
    }

    template <typename C1, typename C2>
        requires geo::GeospatialCoordinate<C1> &&
                 geo::GeospatialCoordinate<C2> &&
                 std::same_as<typename C1::ellipsoid_v,
                              typename C2::ellipsoid_v>
    static auto vector(C1 const& c1, C2 const& c2) {
        auto geocentric_c1 = geo::to_geocentric(c1);
        auto geocentric_c2 = geo::to_geocentric(c2);
        return geocentric_c2 - geocentric_c1;
    }

    template <typename C1, typename C2>
        requires geo::GeospatialCoordinate<C1> &&
                 geo::GeospatialCoordinate<C2> &&
                 std::same_as<typename C1::ellipsoid_v,
                              typename C2::ellipsoid_v>
    static auto distance_and_vector(C1 const& c1, C2 const& c2) {
        auto v = vector(c1, c2);
        return std::make_pair(linalg::mag(v), v);
    }
};

constexpr Euclidean euclidean{};
};  // namespace csics::geo
