#include <cmath>

struct Vec4i {
    int x, y, z, w;
};

struct Vec4 {
    float x, y, z, w;

    Vec4 operator + (const Vec4& other) const {
        return Vec4(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    Vec4 operator - (const Vec4& other) const {
        return Vec4(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    Vec4 operator * (float s) const {
        return Vec4(x * s, y * s, z * s, w * s);
    }
};

struct Vec3 {
    float x, y, z;

    Vec3 operator - (const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator + (const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator ^ (const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    float operator[](unsigned index) const
    {
        switch (index)
        {
        case 0: return x;
        case 1: return y; 
        case 2: return z;
        default: return 0;
        }
    }

    Vec3 operator * (const float& other) const {
        return Vec3(x * other, y * other, z * other);
    }

    Vec3 operator * (const Vec3& other) const {
        return Vec3(x * other.x, y * other.y, z * other.z);
    }

    Vec3 normalize() {
        float len = sqrt(x * x + y * y + z * z);
        if (len > 0) {
            return Vec3(x / len, y / len, z / len);
        }
        return Vec3(x, y, z);
    }
};

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}

    Vec2 operator - (const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }

    Vec2 operator + (const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }

    Vec2 operator * (const float& other) const {
        return Vec2(x * other, y * other);
    }
};

struct Vec3i {
    float x, y, z;
};

struct Vec2i {
    int x, y;

    Vec2i operator + (const Vec2i& other) const {
        return Vec2i{ x + other.x, y + other.y };
    }

    Vec2i operator - (const Vec2i& other) const {
        return Vec2i{ x - other.x, y - other.y };
    }

    Vec2i operator * (const float& other) const {
        int x = x * other;
        int y = y * other;
        return Vec2i{ x, y };
    }
};

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct Matrix4 {
private:
    float m[16];

public:
    Matrix4() {
        for (int i = 0; i < 16; i++) m[i] = 0;
        m[0] = m[5] = m[10] = m[15] = 1.0f; 
    }

    static Matrix4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Matrix4 result;

        Vec3 f = (target - eye).normalize();
        Vec3 s = cross(f, up).normalize();
        Vec3 u = cross(s, f);

        result.m[0] = s.x;
        result.m[1] = s.y;
        result.m[2] = s.z;

        result.m[4] = u.x;
        result.m[5] = u.y;
        result.m[6] = u.z;

        result.m[8] = -f.x;
        result.m[9] = -f.y;
        result.m[10] = -f.z;

        result.m[12] = -dot(s, eye);
        result.m[13] = -dot(u, eye);
        result.m[14] = dot(f, eye);

        return result;
    }

    static Matrix4 perspective(float fov, float aspect, float near, float far) {
        Matrix4 result;

        float tanHalfFov = tan(fov / 2.0f);
        float range = far - near;

        result.m[0] = 1.0f / (aspect * tanHalfFov);
        result.m[5] = 1.0f / tanHalfFov;
        result.m[10] = -(far + near) / range;
        result.m[11] = -1.0f;
        result.m[14] = -(2.0f * far * near) / range;
        result.m[15] = 0.0f;

        return result;
    }

    Vec3 transformPoint(const Vec3& point) const {
        float x = m[0] * point.x + m[4] * point.y + m[8] * point.z + m[12];
        float y = m[1] * point.x + m[5] * point.y + m[9] * point.z + m[13];
        float z = m[2] * point.x + m[6] * point.y + m[10] * point.z + m[14];
        float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];

        if (w != 0.0f) {
            x /= w;
            y /= w;
            z /= w;
        }

        return Vec3(x, y, z);
    }
};

Vec3 barycentric(Vec3* pts, Vec2 P) {
    Vec3 u = cross(Vec3(pts[2].x - pts[0].x, pts[1].x - pts[0].x, pts[0].x - P.x),
        Vec3(pts[2].y - pts[0].y, pts[1].y - pts[0].y, pts[0].y - P.y));

    // Если вырожденный
    if (std::abs(u.z) < 1e-2) {
        return Vec3(-1, 1, 1);
    }

    return Vec3(1.0f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
}