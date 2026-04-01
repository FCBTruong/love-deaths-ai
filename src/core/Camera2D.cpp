#include "core/Camera2D.h"

#include <cmath>

void Camera2D::SnapToTarget(float targetX, float targetY) {
    x = targetX - (width * 0.5f);
    y = targetY - (height * 0.5f);
}

void Camera2D::LerpToTarget(float targetX, float targetY, float smoothness, float dt) {
    const float goalX = targetX - (width * 0.5f);
    const float goalY = targetY - (height * 0.5f);

    const float blend = 1.0f - std::exp(-smoothness * dt);
    x += (goalX - x) * blend;
    y += (goalY - y) * blend;
}
