#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Servo.h>
#include "HUSKYLENS.h"

// --- Pins ---
#define LED_PIN       4
#define NUMPIXELS     74
#define SERVO_PIN_1   6  // Pan
#define SERVO_PIN_2   7  // Tilt

// === Color Game Pins ===
#define BTN_RED_PIN    2
#define BTN_BLUE_PIN   3
#define BTN_GREEN_PIN  4
#define BTN_YELLOW_PIN 5

// --- Enums ---
enum Emotion { NEUTRAL, SUPRISED, HAPPY, ANGRY, SAD };
enum UserBehavior { NONE, PASSING, APPROACHING };

// --- Calibration Settings ---
extern float servo1_center, servo2_center;
extern float tracking_x, tracking_y;
extern float tracking_deadzone, smoothing_speed;

// --- Global Variables ---
extern Adafruit_NeoPixel pixels;
extern int LED_BRIGHTNESS;
extern Emotion emotion;
extern HUSKYLENS huskylens;
extern HUSKYLENSResult face;
extern Servo servo1, servo2;
extern bool face_detected;
extern float servo1_pos, servo2_pos, servo1_target, servo2_target, servo1_speed, servo2_speed;

extern int prev_face_width, prev_face_x;
extern unsigned long behavior_check_timer;
extern UserBehavior current_user_behavior;

extern long timer, timer_comm;

// --- Color Game State ---
extern bool color_game_active;
extern bool lastStateRed;
extern bool lastStateBlue;
extern bool lastStateGreen;
extern bool lastStateYellow;

// --- Eye Patterns ---
extern byte neutral[], blink1[], blink2[], suprised[], happy[], angry[], sad[];

#endif