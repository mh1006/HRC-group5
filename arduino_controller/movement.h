#include "config.h"

// ============================================
// --------------- SERVO MOTORS ---------------
// ============================================

void move_servos(){
  // Smooth movement logic: prevents aggressive jerks when tracking
  auto smooth = [](float &pos, float &target, float &speed, Servo &s) {
    if (abs(target - pos) < 1) {
      s.write(target);
      pos = target;
      speed = 0;
    } else {
      speed = constrain(constrain(target - pos, speed - smoothing_speed, speed + smoothing_speed), -1.0, 1.0);
      pos += speed;
      s.write(pos);
    }
  };
  smooth(servo_horizontal_pos, servo_horizontal_target, servo_horizontal_speed, servo_horizontal);
  smooth(servo_vertical_pos, servo_vertical_target, servo_vertical_speed, servo_vertical);
}
