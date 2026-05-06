#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "config.h"

// ============================================
// ------------------- Eyes -------------------
// ============================================

void display_eye(byte arr[], int hue, bool left) {
  // We will draw a circle on the display
  // It is a hexagonal matrix, which means we have to do some math to know where each pixel is on the screen

  int rows[] = {4, 5, 6, 7, 6, 5, 4};      // The matrix has 4, 5, 6, 7, 6, 5, 4 rows.
  int NUM_COLUMNS = 7;                     // There are 7 columns
  int index = (left) ? 0 : 37;             // If we draw the left eye, we have to add an offset of 37 (4+5+6+7+6=5+4)
  for (int i = 0; i < NUM_COLUMNS; i++) {
    for (int j = 0; j < rows[i]; j++) {
      int brightness = LED_BRIGHTNESS * bitRead(arr[i], (left) ? rows[i] - 1 - j : j);
      pixels.setPixelColor(index, pixels.ColorHSV(hue * 256, 255, brightness));
      index ++;
    }
  }
}

void display_eyes(byte arr[], int hue) {
  display_eye(arr, hue, true);
  display_eye(arr, hue, false);
}

// ============================================
// -------------- Tracking Faces --------------
// ============================================

void apply_tracking(bool isExcited){
  if (!face_detected) return;
    // 1. Calculate basic tracking coordinates
    float pot_x = servo1_center + float(face.xCenter - 160) / 320.00 * tracking_x;
    float pot_y = servo2_center + float(face.yCenter - 120) / 240.00 * tracking_y;
    // 2. Add "Dance/Wiggle" offset if excited (Approaching)
    if (isExcited) {
      // Add a small oscillation
      pot_x += 10.0 * sin(millis() / 200.0); 
      pot_y += 5.0 * cos(millis() / 150.0);
    }
    // 3. Apply deadzone and update targets
    if (abs(pot_x - servo1_target) > tracking_deadzone) servo1_target = pot_x;
    if (abs(pot_y - servo2_target) > tracking_deadzone) servo2_target = pot_y;
}

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
  smooth(servo1_pos, servo1_target, servo1_speed, servo1);
  smooth(servo2_pos, servo2_target, servo2_speed, servo2);
}

#endif