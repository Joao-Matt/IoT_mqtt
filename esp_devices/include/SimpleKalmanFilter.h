#pragma once

class SimpleKalman {
public:
  SimpleKalman(float alpha = 0.98f) : alpha_(alpha), angle_(0.0f), initialized_(false) {}

  float update(float gyroRateDps, float accelAngleDeg, float dtSeconds) {
    if (!initialized_) {
      angle_ = accelAngleDeg;
      initialized_ = true;
      return angle_;
    }

    float gyroAngle = angle_ + (gyroRateDps * dtSeconds);
    angle_ = (alpha_ * gyroAngle) + ((1.0f - alpha_) * accelAngleDeg);
    return angle_;
  }

private:
  float alpha_;
  float angle_;
  bool initialized_;
};
