// Pin locations
// The top servo should be in the closest slot to the USB, then both plugged into D6 on the shield
// The huskylens should be plugged into any I2C port.
#define SERVO_PIN_1   7  // left right
#define SERVO_PIN_2   6  // up down

#include <Servo.h>
#include <Wire.h>
#include "HUSKYLENS.h"

// Global variables

// --- Enums ---
//enum Emotion { NEUTRAL, SUPRISED, HAPPY, ANGRY, SAD };

//  Servos
extern Servo servo_horizontal, servo_vertical;
extern float servo_horizontal_pos, servo_vertical_pos, servo_horizontal_default_pos, servo_vertical_default_pos, servo_horizontal_target, servo_vertical_target, servo_horizontal_speed, servo_vertical_speed;
//  Anti-jitter
extern float smoothing_speed;

// Huskylens
extern HUSKYLENS huskylens;
extern HUSKYLENSResult face;
extern bool face_detected;