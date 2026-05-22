#pragma once

#include <cmath>
#include <concepts>

#include "csics/linalg/Vec.hpp"

namespace csics::linalg {
template <std::floating_point T>
class Degrees;

template <std::floating_point T>
class Radians {
   public:
    using value_type = T;
    constexpr Radians() = default;
    constexpr Radians(T radians) : radians_(radians) {}

    constexpr const T radians() const noexcept { return radians_; }
    constexpr T& radians() noexcept { return radians_; }

    constexpr void radians(T r) noexcept { radians_ = r; }

    constexpr Radians(const Degrees<T>& d)
        : radians_(d.degrees() * static_cast<T>(M_PI) /
                   static_cast<T>(180.0f)) {}

    constexpr operator Degrees<T>() const { return Degrees<T>(*this); }
    constexpr operator T() const { return radians_; }

    constexpr Radians<T>& operator=(const T& r) {
        radians_ = r;
        return *this;
    }

    constexpr Radians<T>& operator=(const Degrees<T>& d) {
        radians_ = d.degrees() * static_cast<T>(M_PI) /
                   static_cast<T>(180.0f);
        return *this;
    }

    constexpr Radians<T> operator+( const Radians<T>& other) const {
        return Radians<T>(radians_ + other.radians_);
    }

    constexpr Radians<T> operator-( const Radians<T>& other) const {
        return Radians<T>(radians_ - other.radians_);
    }

    static constexpr Radians<T> from_degrees(const T& degrees) {
        return Radians<T>(degrees * static_cast<T>(M_PI) /
                         static_cast<T>(180.0f));
    }

     static constexpr Radians<T> from_degrees(const Degrees<T>& degrees) {
        return Radians<T>(degrees.degrees() * static_cast<T>(M_PI) /
                         static_cast<T>(180.0f));
    }

   private:
    T radians_;
};

template <std::floating_point T>
constexpr Radians<T> radians(T r) {
    return Radians<T>::from_degrees(r);
}

template <std::floating_point T>
class Degrees {
   public:
    using value_type = T;
    constexpr Degrees() = default;
    constexpr Degrees(T degrees) : degrees_(degrees) {}
    constexpr Degrees(const Radians<T>& r)
        : degrees_(r.radians() * static_cast<T>(180.0f) /
                   static_cast<T>(M_PI)) {}

    constexpr const T degrees() const noexcept { return degrees_; }
    constexpr T& degrees() noexcept { return degrees_; }

    constexpr Degrees<T>& operator=(const T& d) {
        degrees_ = d;
        return *this;
    }

    constexpr void degrees(T d) noexcept { degrees_ = d; }

    constexpr operator Radians<T>() const { return Radians<T>(*this); }
    constexpr operator T() const { return degrees_; }
   private:
    T degrees_;
};

template <std::floating_point T>
class EulerAngles {
   public:
    using value_type = T;
    constexpr EulerAngles() = default;
    constexpr EulerAngles(T yaw, T pitch, T roll)
        : roll_(roll), pitch_(pitch), yaw_(yaw) {}

    constexpr const T roll() const noexcept { return roll_; }
    constexpr const T pitch() const noexcept { return pitch_; }
    constexpr const T yaw() const noexcept { return yaw_; }

    constexpr const T psi() const noexcept { return yaw_; }
    constexpr const T theta() const noexcept { return pitch_; }
    constexpr const T phi() const noexcept { return roll_; }

    constexpr T& roll() noexcept { return roll_; }
    constexpr T& pitch() noexcept { return pitch_; }
    constexpr T& yaw() noexcept { return yaw_; }

    bool operator==(const EulerAngles& other) const {
        return roll_ == other.roll_ && pitch_ == other.pitch_ &&
               yaw_ == other.yaw_;
    }

    bool operator!=(const EulerAngles& other) const {
        return !(*this == other);
    }

   private:
    T roll_;
    T pitch_;
    T yaw_;
};

template <typename T>
class Cylindrical;

template <typename T>
class Spherical;

template <typename T, std::size_t D = 2>
class Cartesian;

template <typename T>
class Polar;

template <typename T>
Cartesian<T, 3> cylindrical_to_cartesian(const Cylindrical<T>& c);

template <typename T>
Cartesian<T, 3> spherical_to_cartesian(const Spherical<T>& s);

template <typename T>
Cylindrical<T> cartesian_to_cylindrical(const Cartesian<T, 3>& c);

template <typename T>
Spherical<T> cartesian_to_spherical(const Cartesian<T, 3>& c);

template <typename T>
Cartesian<T, 2> polar_to_cartesian(const Polar<T>& p);

template <typename T>
Polar<T> cartesian_to_polar(const Cartesian<T, 2>& c);

template <typename T, std::size_t D>
class Cartesian {
   public:
    using value_type = T;
    constexpr static size_t dim_v = D;

    constexpr Cartesian() = default;
    template <typename... Args, std::enable_if_t<sizeof...(Args) == D, int> = 0>
    constexpr Cartesian(Args&&... args)
        : coords_{std::forward<Args>(args)...} {}

    constexpr Cartesian(const Cartesian& other) = default;
    constexpr Cartesian(Cartesian&& other) = default;

    constexpr Cartesian(const Cylindrical<T>& c) {
        if constexpr (D == 3) {
            *this = cylindrical_to_cartesian(c);
        } else {
            static_assert(D == 3,
                          "Cylindrical coordinates can only be converted to 3D "
                          "Cartesian coordinates");
        }
    }
    constexpr Cartesian(const Spherical<T>& s) {
        if constexpr (D == 3) {
            *this = spherical_to_cartesian(s);
        } else {
            static_assert(D == 3,
                          "Spherical coordinates can only be converted to 3D "
                          "Cartesian coordinates");
        }
    }

    constexpr Cartesian(const Polar<T>& p) {
        if constexpr (D == 2) {
            *this = polar_to_cartesian(p);
        } else {
            static_assert(D == 2,
                          "Polar coordinates can only be converted to 2D "
                          "Cartesian coordinates");
        }
    }

    constexpr const T& operator[](std::size_t index) const noexcept {
        return coords_[index];
    }

    template <std::size_t I>
    constexpr const T& get() const noexcept {
        static_assert(I < D, "Index out of bounds");
        return coords_[I];
    }

    template <std::size_t I>
    constexpr T& get() noexcept {
        static_assert(I < D, "Index out of bounds");
        return coords_[I];
    }

   private:
    T coords_[D];
};

template <typename T>
class Polar {
   public:
    using value_type = T;
    constexpr Polar() = default;
    constexpr Polar(T radius, Radians<T> angle)
        : radius_(radius), angle_(angle) {}

    constexpr const T radius() const noexcept { return radius_; }
    constexpr const Radians<T>& angle() const noexcept { return angle_; }
    constexpr T& radius() noexcept { return radius_; }
    constexpr T& magnitude() noexcept { return radius_; }
    constexpr Radians<T>& angle() noexcept { return angle_; }

    constexpr void radius(T r) noexcept { radius_ = r; }
    constexpr void angle(const Radians<T>& a) noexcept { angle_ = a; }

    constexpr Polar(const Cartesian<T, 2>& c) { *this = cartesian_to_polar(c); }

    template <std::size_t I>
    constexpr const T& get() const noexcept {
        if constexpr (I == 0)
            return radius_;
        else if constexpr (I == 1)
            return angle_.radians();
        else
            static_assert(I < 2, "Index out of bounds");
    }

    template <std::size_t I>
    constexpr T& get() noexcept {
        if constexpr (I == 0)
            return radius_;
        else if constexpr (I == 1)
            return angle_.radians();
        else
            static_assert(I < 2, "Index out of bounds");
    }

   private:
    T radius_;
    Radians<T> angle_;
};

template <typename T>
class Cylindrical {
   public:
    using value_type = T;
    constexpr Cylindrical() = default;
    constexpr Cylindrical(T radius, Radians<T> angle, T height)
        : radius_(radius), angle_(angle), height_(height) {}

    constexpr const T radius() const noexcept { return radius_; }
    constexpr const Radians<T>& angle() const noexcept { return angle_; }
    constexpr const T height() const noexcept { return height_; }

    constexpr T& radius() noexcept { return radius_; }
    constexpr Radians<T>& angle() noexcept { return angle_; }
    constexpr T& height() noexcept { return height_; }

    constexpr Cylindrical(const Cartesian<T, 3>& c) {
        *this = cartesian_to_cylindrical(c);
    }

    template <std::size_t I>
    constexpr const T& get() const noexcept {
        if constexpr (I == 0)
            return radius_;
        else if constexpr (I == 1)
            return angle_.radians();
        else if constexpr (I == 2)
            return height_;
        else
            static_assert(I < 3, "Index out of bounds");
    }

    template <std::size_t I>
    constexpr T& get() noexcept {
        if constexpr (I == 0)
            return radius_;
        else if constexpr (I == 1)
            return angle_.radians();
        else if constexpr (I == 2)
            return height_;
        else
            static_assert(I < 3, "Index out of bounds");
    }

   private:
    T radius_;
    Radians<T> angle_;
    T height_;
};

template <typename T>
class Spherical {
   public:
    using value_type = T;
    constexpr Spherical() = default;
    constexpr Spherical(T radius, Radians<T> polar_angle,
                        Radians<T> azimuthal_angle)
        : radius_(radius),
          polar_angle_(polar_angle),
          azimuthal_angle_(azimuthal_angle) {}

    constexpr Spherical(const Cylindrical<T>& c) {
        *this = cartesian_to_spherical(cylindrical_to_cartesian(c));
    }

    constexpr const T radius() const noexcept { return radius_; }
    constexpr const Radians<T>& polar_angle() const noexcept {
        return polar_angle_;
    }
    constexpr const Radians<T>& azimuthal_angle() const noexcept {
        return azimuthal_angle_;
    }

    constexpr T& radius() noexcept { return radius_; }
    constexpr Radians<T>& polar_angle() noexcept { return polar_angle_; }
    constexpr Radians<T>& azimuthal_angle() noexcept {
        return azimuthal_angle_;
    }

    constexpr Spherical(const Cartesian<T, 3>& c) {
        *this = cartesian_to_spherical(c);
    }

    template <std::size_t I>
    constexpr const T& get() const noexcept {
        if constexpr (I == 0)
            return radius_;
        else if constexpr (I == 1)
            return polar_angle_.radians();
        else if constexpr (I == 2)
            return azimuthal_angle_.radians();
        else
            static_assert(I < 3, "Index out of bounds");
    }

    template <std::size_t I>
    constexpr T& get() noexcept {
        if constexpr (I == 0)
            return radius_;
        else if constexpr (I == 1)
            return polar_angle_.radians();
        else if constexpr (I == 2)
            return azimuthal_angle_.radians();
        else
            static_assert(I < 3, "Index out of bounds");
    }

   private:
    T radius_;
    Radians<T> polar_angle_;
    Radians<T> azimuthal_angle_;
};

// generic coordinate type, can be used for 2D/3D/ND coordinates, or even other
// types of coordinates (e.g. polar coordinates) usage: Coordinate<Polar<T>>,
// Coordinate<Cartesian<T>>, etc. Default to 2D coordinates, but can be
// specialized for higher dimensions if needed
template <typename Rep, std::size_t D = 2>
class Coordinate {
   public:
    Coordinate() = default;
    Coordinate(Rep internal) : internal_(internal) {}
    Coordinate(const Rep& internal) : internal_(internal) {}
    Coordinate(Rep&& internal) : internal_(std::move(internal)) {}

    Coordinate(const Coordinate& other) = default;
    Coordinate(Coordinate&& other) = default;

    template <typename U,
              std::enable_if_t<std::is_convertible_v<U, Rep>, int> = 0>
    Coordinate(const Coordinate<U, D>& other) : internal_(other.internal()) {}

    template <typename U,
              std::enable_if_t<std::is_convertible_v<U, Rep>, int> = 0>
    Coordinate(Coordinate<U, D>&& other)
        : internal_(std::move(other.internal())) {}

    template <typename... Args, std::enable_if_t<sizeof...(Args) == D, int> = 0>
    Coordinate(Args&&... args) : internal_(std::forward<Args>(args)...) {}

    constexpr const Rep& internal() const noexcept { return internal_; }
    constexpr Rep& internal() noexcept { return internal_; }

    template <typename T>
    constexpr Polar<T> polar() const noexcept {
        if constexpr (std::is_same_v<Rep, Polar<T>>) {
            return internal_;
        } else if constexpr (std::is_same_v<Rep, Cartesian<T, 2>>) {
            return cartesian_to_polar(internal_);
        } else {
            static_assert(std::is_same_v<Rep, Polar<T>>,
                          "Coordinate is not of type Polar");
        }
        return internal_;
    }

    template <typename T>
    constexpr Cylindrical<T> cylindrical() const noexcept {
        if constexpr (std::is_same_v<Rep, Cylindrical<T>>) {
            return internal_;
        } else if constexpr (std::is_same_v<Rep, Cartesian<T, 3>>) {
            return cartesian_to_cylindrical(internal_);
        } else if constexpr (std::is_same_v<Rep, Spherical<T>>) {
            return cartesian_to_cylindrical(spherical_to_cartesian(internal_));
        } else {
            static_assert(std::is_same_v<Rep, Cylindrical<T>>,
                          "Coordinate is not of type Cylindrical");
        }
        return internal_;
    }

    template <typename T>
    constexpr Spherical<T> spherical() const noexcept {
        if constexpr (std::is_same_v<Rep, Spherical<T>>) {
            return internal_;
        } else if constexpr (std::is_same_v<Rep, Cartesian<T, 3>>) {
            return cartesian_to_spherical(internal_);
        } else if constexpr (std::is_same_v<Rep, Cylindrical<T>>) {
            return cartesian_to_spherical(cylindrical_to_cartesian(internal_));
        } else {
            static_assert(std::is_same_v<Rep, Spherical<T>>,
                          "Coordinate is not of type Spherical");
        }
        return internal_;
    }

    template <typename T>
    Cartesian<T, D> cartesian() const noexcept {
        if constexpr (std::is_same_v<Rep, Cartesian<T, D>>) {
            return internal_;
        } else if constexpr (std::is_same_v<Rep, Polar<T>>) {
            return polar_to_cartesian(internal_);
        } else if constexpr (std::is_same_v<Rep, Cylindrical<T>> && D == 3) {
            return cylindrical_to_cartesian(internal_);
        } else if constexpr (std::is_same_v<Rep, Spherical<T>> && D == 3) {
            return spherical_to_cartesian(internal_);
        } else {
            static_assert(std::is_same_v<Rep, Cartesian<T, D>>,
                          "Coordinate is not of type Cartesian");
        }
    }

    template <std::size_t I>
    constexpr auto get() const noexcept {
        internal_.template get<I>();
    }

    template <std::size_t I>
    constexpr auto& get() noexcept {
        return internal_.template get<I>();
    }

   private:
    Rep internal_;
};

};  // namespace csics::linalg

// Operator implementations
namespace csics::linalg {

template <typename T>
Cartesian<T, 3> cylindrical_to_cartesian(const Cylindrical<T>& c) {
    T x = c.radius() * std::cos(c.angle().radians());
    T y = c.radius() * std::sin(c.angle().radians());
    T z = c.height();
    return Cartesian<T, 3>(x, y, z);
}

template <typename T>
Cartesian<T, 3> spherical_to_cartesian(const Spherical<T>& s) {
    T x = s.radius() * std::sin(s.polar_angle().radians()) *
          std::cos(s.azimuthal_angle().radians());
    T y = s.radius() * std::sin(s.polar_angle().radians()) *
          std::sin(s.azimuthal_angle().radians());
    T z = s.radius() * std::cos(s.polar_angle().radians());
    return Cartesian<T, 3>(x, y, z);
}

template <typename T>
Cartesian<T, 2> polar_to_cartesian(const Polar<T>& p) {
    T x = p.radius() * std::cos(p.angle().radians());
    T y = p.radius() * std::sin(p.angle().radians());
    return Cartesian<T, 2>(x, y);
}

template <typename T, std::size_t D>
ColumnVec<T, D> operator-(const Cartesian<T, D>& a, const Cartesian<T, D>& b) {
    ColumnVec<T, D> result;
    []<std::size_t... I>(const Cartesian<T, D>& a, const Cartesian<T, D>& b,
                         Cartesian<T, D>& result, std::index_sequence<I...>) {
        ((result.template get<I>() = a.template get<I>() - b.template get<I>()),
         ...);
    }(a, b, result, std::make_index_sequence<D>{});
    return result;
}

template <typename T, std::size_t D>
Cartesian<T, D> operator+(const Cartesian<T, D>& a, const Cartesian<T, D>& b) {
    Cartesian<T, D> result;
    []<std::size_t... I>(const Cartesian<T, D>& a, const Cartesian<T, D>& b,
                         Cartesian<T, D>& result, std::index_sequence<I...>) {
        ((result.template get<I>() = a.template get<I>() + b.template get<I>()),
         ...);
    }(a, b, result, std::make_index_sequence<D>{});
    return result;
}

// If needed, these operators can be rewritten to be optimized for specific
// coordinate types, but for now we can just convert to cartesian coordinates
template <typename T>
ColumnVec<T, 3> operator-(const Cylindrical<T>& a, const Cylindrical<T>& b) {
    return cylindrical_to_cartesian(a) - cylindrical_to_cartesian(b);
}

template <typename T>
ColumnVec<T, 3> operator-(const Spherical<T>& a, const Spherical<T>& b) {
    return spherical_to_cartesian(a) - spherical_to_cartesian(b);
}

template <typename T>
ColumnVec<T, 2> operator-(const Polar<T>& a, const Polar<T>& b) {
    return polar_to_cartesian(a) - polar_to_cartesian(b);
}

template <typename T, std::size_t D>
Cartesian<T, D> operator+(const Cartesian<T, D> a, const ColumnVec<T, D>& b) {
    Cartesian<T, D> result;
    []<std::size_t... I>(const Cartesian<T, D>& a, const ColumnVec<T, D>& b,
                         Cartesian<T, D>& result, std::index_sequence<I...>) {
        ((result.template get<I>() = a.template get<I>() + b.template get<I>()),
         ...);
    }(a, b, result, std::make_index_sequence<D>{});
    return result;
}

template <typename T, std::size_t D>
Cartesian<T, D> operator+(const ColumnVec<T, D>& a, const Cartesian<T, D>& b) {
    return b + a;  // commutative
}

template <typename T, std::size_t D>
Cartesian<T, D> operator-(const Cartesian<T, D>& a, const ColumnVec<T, D>& b) {
    return a + (-1 * b);
}

template <typename T>
Cylindrical<T> operator+(const Cylindrical<T>& a, const ColumnVec<T, 3>& b) {
    return cartesian_to_cylindrical(cylindrical_to_cartesian(a) + b);
}

template <typename T>
Cylindrical<T> operator+(const ColumnVec<T, 3>& a, const Cylindrical<T>& b) {
    return b + a;  // commutative
}

template <typename T>
Cylindrical<T> operator-(const Cylindrical<T>& a, const ColumnVec<T, 3>& b) {
    return cartesian_to_cylindrical(cylindrical_to_cartesian(a) - b);
}

template <typename T>
Spherical<T> operator+(const Spherical<T>& a, const ColumnVec<T, 3>& b) {
    return cartesian_to_spherical(spherical_to_cartesian(a) + b);
}

template <typename T>
Spherical<T> operator+(const ColumnVec<T, 3>& a, const Spherical<T>& b) {
    return b + a;  // commutative
}

template <typename T>
Spherical<T> operator-(const Spherical<T>& a, const ColumnVec<T, 3>& b) {
    return cartesian_to_spherical(spherical_to_cartesian(a) - b);
}

template <typename T>
Polar<T> operator+(const Polar<T>& a, const ColumnVec<T, 2>& b) {
    return cartesian_to_polar(polar_to_cartesian(a) + b);
}

template <typename T>
Polar<T> operator+(const ColumnVec<T, 2>& a, const Polar<T>& b) {
    return b + a;  // commutative
}

template <typename T>
Polar<T> operator-(const Polar<T>& a, const ColumnVec<T, 2>& b) {
    return cartesian_to_polar(polar_to_cartesian(a) - b);
}

template <typename Rep1, typename Rep2, typename RepRet, std::size_t D>
ColumnVec<RepRet, D> operator-(const Coordinate<Rep1, D>& a,
                               const Coordinate<Rep2, D>& b) {
    return a.internal() - b.internal();
}

template <typename Rep1, typename Rep2, typename RepRet, std::size_t D>
Coordinate<RepRet, D> operator+(const ColumnVec<Rep1, D>& a,
                                const Coordinate<Rep2, D>& b) {
    return Coordinate<RepRet, D>(a + b.internal());
}

template <typename Rep1, typename Rep2, typename RepRet, std::size_t D>
Coordinate<RepRet, D> operator+(const Coordinate<Rep1, D>& a,
                                const ColumnVec<Rep2, D>& b) {
    return Coordinate<RepRet, D>(a.internal() + b);
}

};  // namespace csics::linalg

namespace std {
    template <typename Rep, std::size_t D>
    struct tuple_size<csics::linalg::Coordinate<Rep, D>>
        : std::integral_constant<std::size_t, D> {};

    template <typename T>
        T cos(const csics::linalg::Radians<T>& angle) {
            return std::cos(angle.radians());
        }

    template <typename T>
        T sin(const csics::linalg::Radians<T>& angle) {
            return std::sin(angle.radians());
        }
}
