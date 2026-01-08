#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "math.h"
#include "our_gl.h"

class Camera {
public:
    Vec3f eye;
    Vec3f center;
    Vec3f up;


    Camera(Vec3f e, Vec3f c, Vec3f u,
           float fov_deg = 60.f,
           float near_plane = 1.f,
           float far_plane  = 1000.f)
        : eye(e), center(c), up(u)
    {}

    void applyView() const {
        lookat(eye, center, up);
    }

    void applyProjection(float viewport_width, float viewport_height) const {
        float k = -1.f / (eye - center).norm();
        projection(k);
    }
};

#endif // __CAMERA_H__
