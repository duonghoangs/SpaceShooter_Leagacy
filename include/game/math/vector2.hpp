#pragma once

#include <cmath>

namespace game {

struct Vector2 {
    float x = 0.0F;
    float y = 0.0F;

    constexpr Vector2& operator+=(const Vector2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vector2& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr Vector2& operator-=(const Vector2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    [[nodiscard]] constexpr float length_squared() const {
        return x * x + y * y;
    }

    [[nodiscard]] Vector2 normalized() const {
        const float length = std::sqrt(length_squared());
        return length > 0.0F ? Vector2{x / length, y / length} : Vector2{};
    }

    [[nodiscard]] Vector2 limited(float maximum) const {
        if (length_squared() <= maximum * maximum) {
            return *this;
        }
        Vector2 result = normalized();
        result *= maximum;
        return result;
    }

    [[nodiscard]] Vector2 rotated(float degrees) const {
        constexpr float pi = 3.14159265358979323846F;
        const float radians = degrees * pi / 180.0F;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return {x * cosine - y * sine, x * sine + y * cosine};
    }
};

[[nodiscard]] constexpr Vector2 operator+(Vector2 left, const Vector2& right) {
    left += right;
    return left;
}

[[nodiscard]] constexpr Vector2 operator-(Vector2 left, const Vector2& right) {
    left -= right;
    return left;
}

[[nodiscard]] constexpr Vector2 operator*(Vector2 vector, float scalar) {
    vector *= scalar;
    return vector;
}

[[nodiscard]] constexpr Vector2 operator*(float scalar, Vector2 vector) {
    return vector * scalar;
}

[[nodiscard]] constexpr float dot(const Vector2& left, const Vector2& right) {
    return left.x * right.x + left.y * right.y;
}

[[nodiscard]] constexpr Vector2 lerp(
    const Vector2& start, const Vector2& end, float alpha) {
    return start + (end - start) * alpha;
}

}  // namespace game
