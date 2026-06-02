#include "config.h"
#include "movement.h"
#include "vision.h"

// Timing of loop
long timer;
long timer_comm;

// Servos
float smoothing_speed = 0.05;
Servo servo_horizontal, servo_vertical;
float servo_horizontal_default_pos = 90, servo_vertical_default_pos = 30;
float servo_horizontal_pos = servo_horizontal_default_pos, servo_vertical_pos = servo_vertical_default_pos; 
float servo_horizontal_target = servo_horizontal_pos, servo_vertical_target = servo_vertical_pos;
float servo_horizontal_speed = 0, servo_vertical_speed = 0;

// Emotions
//Emotion emotion = NEUTRAL;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1);

  servo_horizontal.attach(SERVO_PIN_1);
  servo_vertical.attach(SERVO_PIN_2);
  servo_horizontal.write(servo_horizontal_default_pos);
  servo_vertical.write(servo_vertical_default_pos);
}

void loop() {
  if (millis() - timer >= 20){
    timer = millis();
    move_servos();
  }
  if (millis() - timer_comm >= 10){
    timer_comm = millis();
    communication();
  }
}

// ============================================
// ----------------- Emotions -----------------
// ============================================

// Emotion string_to_emotion(String emotion_string){
//     if(emotion_string == "SAD") return SAD;
//     else if(emotion_string == "SURPRISED") return SUPRISED;
//     else if(emotion_string == "HAPPY") return HAPPY;
//     else if(emotion_string == "ANGRY") return ANGRY;
//     return NEUTRAL;
// }

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
  // Example of what the data could look like: "SERVO,(10,10);EYES,sad;."
  if (data.length() > 1 && data.charAt(data.length() - 1) == '.') {
    // Send the received data back for debugging
    Serial.print("Received: " + data);

    String command_pair;
    String type;
    String value;
    for (int i = 0; data.length() > 1; i++){
      command_pair = data.substring(0, data.indexOf(';'));
      data = data.substring(data.indexOf(';') + 1, data.length());
      
      type = command_pair.substring(0,command_pair.indexOf(','));
      value = command_pair.substring(command_pair.indexOf(',') + 1,command_pair.indexOf(';'));

      if (type == "SERVO") {
        int first_value = value.substring(value.indexOf('(') + 1, value.indexOf(',')).toInt();
        int second_value = value.substring(value.indexOf(',') + 1, value.indexOf(')')).toInt();
        servo_horizontal_target = first_value;
        servo_vertical_target = second_value;
        Serial.print("Setting servo 1 to: " + String(servo_horizontal_target) + "" + "Setting servo 2 to: " + String(servo_vertical_target) + "\n");
      }
      if (type == "EYES") Serial.print("Setting eyes to: " + value + "\n");
      // if (type == "EMOTION") {
      //   Serial.print("Setting emotion to: " + value + "\n");
      //   emotion = string_to_emotion(value);
      //   }
      }
    }
}