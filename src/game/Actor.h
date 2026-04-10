#pragma once

class Actor {
public:
    Actor(float x, float y, float width, float height)
        : x_(x), y_(y), width_(width), height_(height), hasBegunPlay_(false), hasEndedPlay_(false), lifeTime_(0.0f) {}

    virtual ~Actor() = default;

    void BeginPlay() {
        if (hasBegunPlay_) {
            return;
        }
        hasBegunPlay_ = true;
        hasEndedPlay_ = false;
        lifeTime_ = 0.0f;
        OnBeginPlay();
    }

    virtual void Tick(float dt) {
        if (!hasBegunPlay_) {
            BeginPlay();
        }
        if (hasEndedPlay_) {
            return;
        }
        lifeTime_ += dt;
    }

    void EndPlay() {
        if (!hasBegunPlay_ || hasEndedPlay_) {
            return;
        }
        OnEndPlay();
        hasEndedPlay_ = true;
        hasBegunPlay_ = false;
    }

    void SetPosition(float x, float y) {
        x_ = x;
        y_ = y;
    }

    float X() const { return x_; }
    float Y() const { return y_; }
    float CenterX() const { return x_ + (width_ * 0.5f); }
    float CenterY() const { return y_ + (height_ * 0.5f); }
    float FeetY() const { return y_ + height_; }
    float Width() const { return width_; }
    float Height() const { return height_; }
    bool HasBegunPlay() const { return hasBegunPlay_; }
    bool HasEndedPlay() const { return hasEndedPlay_; }
    float LifeTime() const { return lifeTime_; }

protected:
    virtual void OnBeginPlay() {}
    virtual void OnEndPlay() {}

    float x_;
    float y_;
    float width_;
    float height_;
    bool hasBegunPlay_;
    bool hasEndedPlay_;
    float lifeTime_;
};
