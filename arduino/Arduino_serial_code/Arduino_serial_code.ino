#include <Wire.h>
#include "config.h"
#include "sensor.h"
#include "movement.h"


// --- Global Variable Definitions ---

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN);
HUSKYLENS huskylens;
HUSKYLENSResult face;
Servo servo1, servo2;
Emotion emotion = NEUTRAL;
UserBehavior current_user_behavior = NONE;
bool face_detected = false;
int LED_BRIGHTNESS = 2;

// Adjust these if the head is tilted at the start
float servo1_center = 100.0; // Left/Right center 
float servo2_center = 90.0; // Up/Down center

// Adjust these if tracking direction is reversed
float tracking_x = -80.0; // Change to 50.0 to reverse Left/Right
float tracking_y = 70.0;  // Change to -50.0 to reverse Up/Down

// Jitter control
float tracking_deadzone = 5.0;  // Only move if face moves > 2 degrees
float smoothing_speed = 0.05;   // Lower = smoother but slower (Try 0.03 to 0.08)

// Servo val
float servo1_pos = servo1_center, servo2_pos = servo2_center;
float servo1_target = servo1_center, servo2_target = servo2_center;
float servo1_speed = 0, servo2_speed = 0;

// Behavior Analysis Variables
int prev_face_width = 0;
int prev_face_x = 0;
unsigned long behavior_check_timer = 0;

long timer;
long timer_comm;

// We store the different eyes as a byte array
// You can use https://sjoerd.tech/eyes/ to design more eye patterns

byte neutral[] = {
  B0000,
  B01110,
  B011110,
  B0111110,
  B011110,
  B01110,
  B0000
};

byte blink1[] = {
  B0000,
  B00000,
  B011110,
  B0111110,
  B011110,
  B00000,
  B0000
};

byte blink2[] = {
  B0000,
  B00000,
  B000000,
  B1111111,
  B000000,
  B00000,
  B0000
};

byte suprised[] = {
  B1111,
  B11111,
  B111111,
  B1111111,
  B111111,
  B11111,
  B1111
};

byte happy[] = {
  B1111,
  B11111,
  B111111,
  B1100011,
  B000000,
  B00000,
  B0000
};

byte angry[] = {
  B0000,
  B10000,
  B110000,
  B1111000,
  B111110,
  B11111,
  B1111
};

byte sad[] = {
  B0000,
  B00001,
  B000011,
  B0001111,
  B011111,
  B11111,
  B1111
};

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1);

  // Initialize the leds
  pixels.begin();

// Initialize HuskyLens (via I2C)
  Wire.begin();
  while (!huskylens.begin(Wire)) {
      Serial.println(F("HuskyLens failed to start! Please check the I2C connection."));
      delay(1000);
  }

  // Initialize Servos
  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);
  servo1.write(servo1_center);
  servo2.write(servo2_center);
}

void  loop() {

  // Every 20 milliseconds, update the hardware (Camera, Servos, Eyes)
  if (millis() - timer >= 20){
    timer = millis();
    husky_lens();    // Grab face position
    analyze_user_behavior();
    run_emotions();  // Calculate targets for eyes and servos
    move_servos();   // Smoothly move servos to the targets
  }

  // Every 10 milliseconds, check for PC communication (Python)
  if (millis() - timer_comm >= 10){
    timer_comm = millis();
    communication();
  }

}

// ============================================
// ----------------- Emotions -----------------
// ============================================

void run_emotions(){
  pixels.clear();  

  switch (emotion) {
    case NEUTRAL:
      if (millis() % 5000 < 150) display_eyes(blink1, 125);
      else if (millis() % 5000 < 300) display_eyes(blink2, 125);
      else if (millis() % 5000 < 450) display_eyes(blink1, 125);
      else display_eyes(neutral, 125);

      if (face_detected) {
        if (current_user_behavior == APPROACHING) {
          Serial.println("Someone is coming! Dancing!");
          emotion = HAPPY;
        } 
        else if (current_user_behavior == PASSING) {
          Serial.println("Passive: Someone just walking by.");
          apply_tracking(false);
        }
        else if (current_user_behavior == PASSING) {
          Serial.println("Boring");
        }
      }
      break;

    case HAPPY:
      display_eyes(happy, 80);
      
      if (face_detected) {
        // Tracking + Dancing
        apply_tracking(true);

        // Auto-revert when the person moves back
        if (face.width < 80) {
          Serial.println("AHH. Why leaving！");
          emotion = NEUTRAL;
        }
      } else {
        // Revert if face is lost
        emotion = NEUTRAL;
      }
      break;
    case SAD:
      display_eyes(sad, 150);  
      // servo1_target = 90 + 3.0 * sin(millis() / 400.00);
      // servo2_target = 120 + 20.0 * cos(millis() / 500.00);
      break;

    case ANGRY:
      display_eyes(angry, 0);
      // servo1_target = 90 + 10.0 * sin(millis() / 250.00);
      // servo2_target = 110 + 15.0 * cos(millis() / 175.00);
      break;

    case SUPRISED:
      display_eyes(suprised, 125);
      // servo1_target = 90;
      // servo2_target = 80 + 10.0 * cos(millis() / 500.00);
      break;
      
  }

  pixels.show();
}


// ============================================
// ----------- Python communication -----------
// ============================================

void communication() {
  char val = ' ';
  String data = "";
  if (Serial.available()) {
    do {
      val = Serial.read();
      if (val != -1) data = data + val;
    }
    while ( val != -1);
  }

  // Data is a string of what we received, we will split it into the different values
  // Each command is sent as "command_type", command; The final command in the data ends in "." to indicate the end of the command chain.
  // Example of what the data could look like: "SERVO,10;EYES,sad;."
  if (data.length() > 1 && data.charAt(data.length() - 1) == '.') {
    // Send the received data back for debugging
    // Serial.print("Received: " + data);

    String command_pair;
    String type;
    String value;
    for (int i = 0; data.length() > 1; i++){
      command_pair = data.substring(0, data.indexOf(';'));
      data = data.substring(data.indexOf(';') + 1, data.length());
      
      type = command_pair.substring(0,command_pair.indexOf(','));
      value = command_pair.substring(command_pair.indexOf(',') + 1,command_pair.indexOf(';'));

      if (type == "SERVO") Serial.print("Setting servo to: " + value + "\n");
      if (type == "EYES") Serial.print("Setting eyes to: " + value + "\n");
      if (type == "EMOTION") {
        Serial.print("Setting emotion to: " + value + "\n");
        if(value == "NEUTRAL") emotion = NEUTRAL;
        else if(value == "SURPRISED") emotion = SUPRISED;
        else if(value == "HAPPY") emotion = HAPPY;
        else if(value == "ANGRY") emotion = ANGRY;
        else emotion = SAD;
        }
      }
    }
}