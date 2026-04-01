#pragma once

struct Camera2D {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    void SnapToTarget(float targetX, float targetY);
    void LerpToTarget(float targetX, float targetY, float smoothness, float dt);
};
