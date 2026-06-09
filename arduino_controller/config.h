#ifndef CONFIG // If the config file has not been defined yet do it now
#define CONFIG

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <MP3Player_KT403A.h>
#include "HUSKYLENS.h"

// Pin locations
// The top servo should be in the closest slot to the USB, then both plugged into D6 on the grove shield
// The huskylens should be plugged into any I2C port.
// The LED matrices should be plugged into D3 on the shield. From the arduino it should go into "in" on one matrix, then from that out to the in of the next.
#define SERVO_PIN_1   7  // left right
#define SERVO_PIN_2   6  // up down
#define AUDIO_PIN_1 2 // Receive pin 
#define AUDIO_PIN_2 3 // Transmit pin
#define LED_PIN 4
#define LED_COUNT 74 // 37 per matrix

// GLOBAL VARIABLES

// --- Enums ---
enum Emotion { NEUTRAL, SURPRISED, HAPPY, ANGRY, SAD };

// --- Servos ---
extern Servo servo_horizontal, servo_vertical;
extern float servo_horizontal_pos, servo_vertical_pos, servo_horizontal_default_pos, servo_vertical_default_pos, servo_horizontal_target, servo_vertical_target, servo_horizontal_speed, servo_vertical_speed;
// > Anti-jitter
extern float smoothing_speed;

// --- Huskylens ---
extern HUSKYLENS huskylens;
extern HUSKYLENSResult face;
extern bool face_detected;
extern bool tracking_enabled;

// --- Led matrices ---
extern Adafruit_NeoPixel pixels;
extern int LED_BRIGHTNESS;
extern Emotion eye_emotion;

extern int neutral_hue, happy_hue, angry_hue, sad_hue;
// Eye patterns
extern byte neutral[], blink1[], blink2[], surprised[], happy[], angry[], sad[];

#endif