#ifndef SENSOR_H
#define SENSOR_H

#include "config.h"

void husky_lens() {
  if (!huskylens.request() || !huskylens.available()) {
    face_detected = false;
  } else {
    face_detected = false;
    int face_index = 0;
    while (huskylens.available()) {
      HUSKYLENSResult result = huskylens.read();
      if (result.command == COMMAND_RETURN_BLOCK) {
        if (face_index == 0 || result.ID == 1) face = result;
        face_index++;
        face_detected = true;
      }
    }
  }
}

void analyze_user_behavior() {
  if (!face_detected) {
    current_user_behavior = NONE;
    return;
  }
  // Check every 200ms to get a meaningful delta
  if (millis() - behavior_check_timer > 200) {
    int delta_width = face.width - prev_face_width;
    int delta_x = abs(face.xCenter - prev_face_x);
    // Approaching: Width increases significantly
    if (delta_width > 3 || face.width > 100) current_user_behavior = APPROACHING;
    // Passing By: High horizontal movement, but low width change
    else if (delta_x > 25 && abs(delta_width) < 4) 
      current_user_behavior = PASSING;
    else 
      current_user_behavior = NONE;
    // Store current values for next comparison
    prev_face_width = face.width;
    prev_face_x = face.xCenter;
    behavior_check_timer = millis();
  }
}

void check_buttons() {
  bool stateRed = digitalRead(BTN_RED_PIN);
  bool stateBlue = digitalRead(BTN_BLUE_PIN);
  bool stateGreen = digitalRead(BTN_GREEN_PIN);
  bool stateYellow = digitalRead(BTN_YELLOW_PIN);

  // High to Low --> pressed
  if (stateRed == LOW && lastStateRed == HIGH) { 
    Serial.println("BTN,RED"); 
    delay(30); 
  }
  if (stateBlue == LOW && lastStateBlue == HIGH) { 
    Serial.println("BTN,BLUE"); 
    delay(30); 
  }
  if (stateGreen == LOW && lastStateGreen == HIGH) { 
    Serial.println("BTN,GREEN"); 
    delay(30); 
  }
  if (stateYellow == LOW && lastStateYellow == HIGH) { 
    Serial.println("BTN,YELLOW"); 
    delay(30); 
  }

  lastStateRed = stateRed;
  lastStateBlue = stateBlue;
  lastStateGreen = stateGreen;
  lastStateYellow = stateYellow;
}

#endif