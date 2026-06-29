#include "config.h"
#include "util.h"
#include "movement.h"
#include "vision.h"
#include "audio.h"
#include "buttons.h"
#include "led_matrices.h"
#include "animation.h"

#include <MemoryFree.h>

// Timing of loop
long timer;
long tick = 0;
long timer_comm;
uint16_t rainbow_timer;
long rainbow_start;

// Servos
float smoothing_speed = 0.02;
Servo servo_horizontal, servo_vertical;
float servo_horizontal_default_pos = 90, servo_vertical_default_pos = 30;
float servo_horizontal_pos = servo_horizontal_default_pos, servo_vertical_pos = servo_vertical_default_pos; 
float servo_horizontal_target = servo_horizontal_pos, servo_vertical_target = servo_vertical_pos;
float servo_horizontal_speed = 0, servo_vertical_speed = 0;

Animation* current_animation = nullptr;

// Huskylens
HUSKYLENS huskylens;
HUSKYLENSResult face;
struct FaceResult {
  int xCenter, yCenter, width, height;
};
FaceResult last_face;// To track if the face info changed from the last message we sent
bool face_detected = false;
bool huskylens_connected = true;
bool tracking_enabled = false;

// Audio player
SoftwareSerial mp3(AUDIO_PIN_1, AUDIO_PIN_2);

// Buttons (LOW = false, HIGH = true)
bool buttonStates[3] = {LOW, LOW, LOW}; 
int pressedButton = 3; // There are 3 buttons; 0, 1 and 2. If pressedButton = 3, no button has been pressed, as this is outside of the range

// All led matrices need to be in 1 object and change pins, otherwise it uses too much memory.
Adafruit_NeoPixel led_matrices(LED_COUNT, EYE_PIN);

// lcd screen
rgb_lcd lcd;

// Other eye stuff
int LED_BRIGHTNESS = 8;
int RAINBOW_BRIGHTNESS = 30;
Emotion eye_emotion = NEUTRAL;

// Hues of different eye emotions
char neutral_color = 'W';
char happy_color = 'G';
char angry_color = 'R';
char sad_color = 'B';

// Different eye byte arrays
const byte neutral[] PROGMEM = { B0000, B01110, B011110, B0111110, B011110, B01110, B0000 };
const byte blink1[]  PROGMEM = { B0000, B00000, B011110, B0111110, B011110, B00000, B0000 };
const byte blink2[]  PROGMEM = { B0000, B00000, B000000, B1111111, B000000, B00000, B0000 };
const byte surprised[] PROGMEM = { B1111, B11111, B111111, B1111111, B111111, B11111, B1111 };
const byte happy[]   PROGMEM = { B1111, B11111, B111111, B1100011, B000000, B00000, B0000 };
const byte angry[]   PROGMEM = { B0000, B10000, B110000, B1111000, B111110, B11111, B1111 };
const byte sad[]     PROGMEM = { B0000, B00001, B0000011, B0001111, B011111, B11111, B1111 };

// Different servo animations
const AnimationStep droop_steps[] PROGMEM = {
          {90, 0, 4000},
          {0, 0, 3000}
};
Animation droop(droop_steps, 2);

const AnimationStep idle_1_steps[] PROGMEM = {
          {120, 45, 2000},
          {60, 45, 2000},
          {90, 30, 2000},
          {120, 45, 2000},
          {90, 30, 2000},
          {60, 45, 2000},
          {90, 30, 1500},
          {90, 45, 3000}
};
Animation idle_1(idle_1_steps, 8);

const AnimationStep nod_steps[] PROGMEM = {
          {90, 0, 3000},
          {90, 90, 3000},
          {90, 0, 3000},
          {90, 90, 3000},
          {90, 0, 3000}
};
Animation nod(nod_steps, 5);

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);

  Serial.print(F("Free memory: "));
  Serial.print(freeMemory());
  Serial.println(F(" bytes"));

  // Wire is for communication with I2C ports, which is what huskylens should be plugged into
  Wire.begin();
  if (!huskylens.begin(Wire)) {
      Serial.println(F("HuskyLens failed to start! Please check the I2C connection."));
      delay(1000);
      if (!huskylens.begin(Wire)) {
        Serial.println(F("Failed to start HuskyLens twice, continuing without camera."));
        huskylens_connected = false;
        Wire.end();
      }
  }

  // Set the pin of the servos
  servo_horizontal.attach(SERVO_PIN_1);
  servo_vertical.attach(SERVO_PIN_2);
  // Set the servos to their default position on start
  servo_horizontal.write(servo_horizontal_default_pos);
  servo_vertical.write(servo_vertical_default_pos);

  // Initialise the led matrices
  led_matrices.begin();
  rainbow_timer = 0;

  led_matrices.setPin(BUTTON_MATRICES_PIN);
  led_matrices.clear();
  display_button_colors('O', 'O', 'O');
  led_matrices.show();
  led_matrices.setPin(EYE_PIN);

  // Initialise lcd with #rows and #cols.
  lcd.begin(16,2);
  lcd.clear();

  // Start all buttons
  for (int i =0; i < 3; i++){
    pinMode(BUTTON_PINS[i], INPUT);
  }

  // Start the connection with the audio module
  mp3.begin(9600);
  delay(100);                 // Required for booting up
  SelectPlayerDevice(0x02);   // Select SD card.
  SetVolume(0x1E);            // Set the volume to the max.
}

void loop() {
  if (millis() - timer >= 20){
    timer = millis();
    tick ++;

    // The neopixels library does not allow interrupts, causing glitches in the movement, so these need to be done on alternate loops.
    if (tick % 2 == 0){
      run_emotions();
    } else {
      if (current_animation != nullptr && current_animation->has_update()){
        AnimationStep s = current_animation->currentStep();
        servo_horizontal_target = s.x;
        servo_vertical_target   = s.y;
      }
      move_servos();
    }
    if (huskylens_connected) husky_lens();
    if (tracking_enabled) track_face();
  }
  read_buttons();
  if (Serial.available()) receive_communication();
  send_communication();
}

// ============================================
// ----------------- Emotions -----------------
// ============================================
void run_emotions(){
  led_matrices.clear();
  switch (eye_emotion) {
    // Neutral means the robot is idle
    case NEUTRAL:
      if (millis() % 5000 < 150) display_matrices(blink1, neutral_color);
      else if (millis() % 5000 < 300) display_matrices(blink2, neutral_color);
      else if (millis() % 5000 < 450) display_matrices(blink1, neutral_color);
      // else if (millis() % 20000 < 100) {
      //   eye_emotion = RAINBOW;
      //   rainbow_start = millis();
      // }
//       if (millis() % 30010 < 100) {
//         eye_emotion = RAINBOW;
//         rainbow_start = millis();
//       }
      if (millis() % 20010 < 100) {
        set_current_animation(&idle_1);
      }
      else display_matrices(neutral, neutral_color);
      break;

    case HAPPY:
      display_matrices(happy, happy_color);
      break;

    case SAD:
      display_matrices(sad, sad_color);  
      break;

    case ANGRY:
      display_matrices(angry, angry_color);
      break;

    case SURPRISED:
      display_matrices(surprised, neutral_color);
      break;
    case RAINBOW:
      if((millis() - rainbow_start) > 7000) eye_emotion = NEUTRAL;
      for (int i = 0; i < LED_COUNT; i++) {
        led_matrices.setPixelColor(i, led_matrices.gamma32(led_matrices.ColorHSV(rainbow_timer + (i * 65536L / LED_COUNT), 255, RAINBOW_BRIGHTNESS)));
      }
      rainbow_timer = int((rainbow_timer + 256) % 65536);
      break;
  }
  led_matrices.show();
}

// Set the servo animation
void set_current_animation(Animation* anim) {
  current_animation = anim;
  current_animation->play();
}

// ============================================
// --------------- Face tracking --------------
// ============================================

// Proportional gain: how a
// ggressively the head chases the face.
// 0.1 means a 160px error (half the screen) moves the servo 16°.
#define TRACKING_GAIN 0.5f
// Pixel deadband: ignore face offsets smaller than this to suppress sensor noise.
// Raise if the head still jitters when the person is still; lower if tracking feels sluggish.
// IF HEAD TWITCHES INCREASE TO 18 or 20. IF TOO SLOW TO REACT WHEN SOMEONE ACTUALLY MOVES DROP TO 8.
#define TRACKING_DEADBAND 8

void track_face() {
  if (face_detected) {
    // HuskyLens resolution: 320×240, so screen centre is (160, 120).
    // Horizontal: face left of centre → servo turns left (target decreases).
    // Vertical:   face above centre  → servo tilts up  (target increases).
    int dx = face.xCenter - 160;
    int dy = face.yCenter - 120;
    if (abs(dx) > TRACKING_DEADBAND)
      servo_horizontal_target = constrain(
        servo_horizontal_default_pos - dx * TRACKING_GAIN, 0, 180);
    if (abs(dy) > TRACKING_DEADBAND)
      servo_vertical_target = constrain(
        servo_vertical_default_pos - dy * TRACKING_GAIN, 0, 180);
  } else {
    // No face visible — drift back to default so the robot looks forward.
    servo_horizontal_target = servo_horizontal_default_pos;
    servo_vertical_target   = servo_vertical_default_pos;
  }
}

// ============================================
// ----------- Python communication -----------
// ============================================
// test command: BUTTONS,RGB;.


void receive_communication() {
  static char buf[64];
  int len = Serial.readBytesUntil('.', buf, sizeof(buf) - 1);
  if (len == 0) return;
  buf[len] = '\0'; // String ending char

  // Serial.print(F("Received: ["));
  // Serial.print(buf);
  // Serial.println(F("]"));

  // Pointers to the data and to the command seperator ;
  char *data = buf;
  char *semicolon;

  // Each command ends in ; so this loops while there are still commands in the data
  while ((semicolon = strchr(data, ';')) != NULL) {
    // This splits the data into 2 strings in place, as '\0' indicates the end of a string
    *semicolon = '\0';

    // All commands are split by commas
    char *comma = strchr(data, ',');
    // If there is no comma, this command is invalid and we continue
    if (!comma) { data = semicolon + 1; continue; }
    *comma = '\0';

    // Again using pointers to extract different parts of the data using '\0' to separate strings
    char *cmd   = data;
    char *value = comma + 1;

    // String comparing function which returns 0 if strings are the same.
    if (strcmp(cmd, "SERVO") == 0) {
      char *c = strchr(value, ',');
      // Atoi interprets string as int, it skips one for the parenthesis, as the servo commands are structured (180,180)
      servo_horizontal_target = atoi(value + 1);
      servo_vertical_target   = c ? atoi(c + 1) : 0;
      // Serial.print(F("Setting servo 1 to: "));
      // Serial.print(servo_horizontal_target);
      // Serial.print(F(", Setting servo 2 to: "));
      // Serial.println(servo_vertical_target);
    } else if (strcmp(cmd, "EYES") == 0) {
      Emotion new_emotion = string_to_emotion(value);
      if (new_emotion == RAINBOW) rainbow_start = millis();  // add this line
      eye_emotion = new_emotion;
      run_emotions();
    } else if (strcmp(cmd, "AUDIO") == 0) {
      // Serial.print(F("Playing audio file: "));
      // Serial.println(value);
      play_audio(atoi(value));
    } else if (strcmp(cmd, "TRACKING") == 0) {
      tracking_enabled = (strcmp(value, "ON") == 0);
      if (!tracking_enabled) {
        servo_horizontal_target = servo_horizontal_default_pos;
        servo_vertical_target   = servo_vertical_default_pos;
      }
    } else if (strcmp(cmd, "BUTTONS") == 0) {
      // ex: BUTTONS,RGB;.
      // The available colors can be found in util.h, use 'X' for a button to keep the same colour
      if (strlen(value) >= 3){
        display_button_colors(value[0], value[1], value[2]);
      }
    } else if(strcmp(cmd, "M") == 0) {
        Serial.print(F("Free memory: "));
        Serial.print(freeMemory());
        Serial.println(F(" bytes"));
    } else if(strcmp(cmd, "ANIMATE") == 0) {
        Serial.print(F("Setting animation to:"));
        Serial.println(value);
        if (strcmp(value, "DROOP") == 0) set_current_animation(&droop); 
        else if (strcmp(value, "IDLE1") == 0) set_current_animation(&idle_1);
        else if (strcmp(value, "NOD") == 0) set_current_animation(&nod);
    }
    data = semicolon + 1;
  }
}

bool same_face(HUSKYLENSResult new_face){
  if (new_face.xCenter != last_face.xCenter) return false;
  else if (new_face.yCenter != last_face.yCenter) return false;
  else if (new_face.width != last_face.width) return false;
  else if (new_face.height != last_face.height) return false;
  else return true;
}

// Sends the communication to python in json format for easy extraction
void send_communication(){
  if ((face_detected && !same_face(face)) || pressedButton != 3){
      Serial.print(F("{"));
      if (face_detected && !same_face(face)){
        // Serial.println(String() + F("Block:xCenter=") + result.xCenter + F(",yCenter=") + result.yCenter + F(",width=") + result.width + F(",height=") + result.height + F(",ID=") + result.ID);
        last_face = {face.xCenter, face.yCenter, face.width, face.height};
        // As string concatenation is very bad for performance and made the memory run out, this is sadly the best way I've found to do this :(
        Serial.print(F("\"detected_face\": {\"xCenter\": "));
        Serial.print(face.xCenter);
        Serial.print(F(", \"yCenter\": "));
        Serial.print(face.yCenter);
        Serial.print(F(", \"width\": "));
        Serial.print(face.width);
        Serial.print(F(", \"height\": "));
        Serial.print(face.height);
        Serial.print(F("}"));
      }
      if (pressedButton != 3){
        Serial.print(F("\"pressed_button\": "));
        Serial.print(pressedButton);
        pressedButton = 3;
      }
      Serial.println(F("}"));
  }
}