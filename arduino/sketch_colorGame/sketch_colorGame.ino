#include <Wire.h>
#include "rgb_lcd.h"
#include <Adafruit_NeoPixel.h>

const int btnPins[3] = {A0, A1, A2}; // Button 0 (LCD), Button 1 (D7), Button 2 (D8)

rgb_lcd lcd; 
#define MATRIX_PIN_1 8  // Matrix 1 connected to D7
#define MATRIX_PIN_2 9  // Matrix 2 connected to D8
#define NUMPIXELS    37 // Number of LEDs per matrix

// Initialize Objects
Adafruit_NeoPixel matrix1(NUMPIXELS, MATRIX_PIN_1);
Adafruit_NeoPixel matrix2(NUMPIXELS, MATRIX_PIN_2);

int LED_BRIGHTNESS = 5; 


String btnColors[3] = {"OFF", "OFF", "OFF"}; // Colors assigned to [LCD, D7_Matrix, D8_Matrix]
String allColors[6] = {"RED", "BLUE", "GREEN", "YELLOW", "CYAN", "PURPLE"};
bool lastBtnState[3] = {HIGH, HIGH, HIGH}; 

String targetColor = "OFF"; 

void setup() {
  Serial.begin(115200); 
  randomSeed(analogRead(A5)); 
  
  lcd.begin(16, 2);lcd.begin(16, 2);
  
  // Initialize Buttons
  for (int i = 0; i < 3; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }

  // Initialize I2C LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  setLcdColor("WHITE"); 

  // Initialize NeoPixel Matrices
  matrix1.begin(); matrix1.setBrightness(LED_BRIGHTNESS); matrix1.show();
  matrix2.begin(); matrix2.setBrightness(LED_BRIGHTNESS); matrix2.show();

  Serial.println("======================================");
  Serial.println("Type command: EYES_COLOR,RED;.");
  Serial.println("======================================");
}

void loop() {
  communication(); // Handle incoming serial commands
  checkButtons();  // Poll button states
}

// ==========================================
// 1. Parse Serial Commands
// ==========================================
void communication() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('.'); 
    input.trim();
    
    int startIndex = 0;
    int endIndex = input.indexOf(';');
    
    while (endIndex != -1) {
      String commandPair = input.substring(startIndex, endIndex);
      int commaIndex = commandPair.indexOf(',');
      
      if (commaIndex != -1) {
        String type = commandPair.substring(0, commaIndex);
        String value = commandPair.substring(commaIndex + 1);

        if (type == "EYES_COLOR") {
          targetColor = value;
          assignRandomColors(value);
          
          Serial.print("\n>>> NEW ROUND! Target Color is: ");
          Serial.println(targetColor);
        }
      }
      startIndex = endIndex + 1;
      endIndex = input.indexOf(';', startIndex);
    }
  }
}

// ==========================================
// 2. Randomly Assign 3 Colors to LCD, D6, and D8
// ==========================================
void assignRandomColors(String targetColor) {
  if (targetColor == "OFF") {
    for(int i = 0; i < 3; i++) btnColors[i] = "OFF";
    lcd.clear();
    setLcdColor("WHITE"); // White while the game end
    setMatrixColor(1, "OFF"); setMatrixColor(2, "OFF");
    return;
  }

  // (0=LCD, 1=D7 Matrix, 2=D8 Matrix)
  int targetIndex = random(0, 3);
  btnColors[targetIndex] = targetColor;

  int colorPoolIdx = random(0, 6);
  for (int i = 0; i < 3; i++) {
    if (i == targetIndex) continue; 
    
    while (allColors[colorPoolIdx] == targetColor) {
      colorPoolIdx = (colorPoolIdx + 1) % 6; 
    }
    btnColors[i] = allColors[colorPoolIdx];
    colorPoolIdx = (colorPoolIdx + 1) % 6; 
  }

  // Position 0: Update LCD Text AND Backlight Color
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target: " + targetColor);
  lcd.setCursor(0, 1);
  lcd.print("Btn0: " + btnColors[0]);
  setLcdColor(btnColors[0]); // Set color on LCD

  // Position 1 & 2: Display Solid Colors on NeoPixel Matrices
  setMatrixColor(1, btnColors[1]); // Matrix 1 (D6) matches Button 1 (A1)
  setMatrixColor(2, btnColors[2]); // Matrix 2 (D8) matches Button 2 (A2)
}

// ==========================================
// 3. Check Button Presses & Handle Feedback
// ==========================================
void checkButtons() {
  for (int i = 0; i < 3; i++) {
    bool reading = digitalRead(btnPins[i]);
    
    if (reading == LOW && lastBtnState[i] == HIGH) {
      delay(50); // Debounce
      
      if (digitalRead(btnPins[i]) == LOW) { 
        if (btnColors[i] != "OFF") {
          
          // Print output format for Python protocol
          Serial.print("BTN,");
          Serial.print(btnColors[i]); 
          
          // Print verification for manual testing
          if (btnColors[i] == targetColor) {
            Serial.print(btnPins[i]);
            Serial.println("-> CORRECT");
            playFeedback(true);
          } else {
            Serial.println("-> INCORRECT");
            playFeedback(false);
          }
          break;
        }
      }
    }
    lastBtnState[i] = reading; 
  }
}

// ==========================================
// 4. Feedback Effects (LCD + Matrix Flashing)
// ==========================================
void playFeedback(bool isCorrect) {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  uint32_t flashColor;
  if (isCorrect) {
    lcd.print(">>> CORRECT! <<<");
    flashColor = matrix1.Color(0, 255, 0); // NeoPixel Green
  } else {
    lcd.print(">>> WRONG!!! <<<");
    flashColor = matrix1.Color(255, 0, 0); // NeoPixel Red
  }

  // ★ 同步閃爍 LCD 背光與矩陣 3 次
  for(int k = 0; k < 3; k++) {
    matrix1.fill(flashColor, 0, NUMPIXELS); matrix2.fill(flashColor, 0, NUMPIXELS);
    matrix1.show(); matrix2.show();
    if (isCorrect) setLcdColor("GREEN"); else setLcdColor("RED");
    delay(150);
    
    matrix1.clear(); matrix2.clear();
    matrix1.show(); matrix2.show();
    setLcdColor("OFF"); 
    delay(150);
  }

  // 特效結束，回到待機狀態
  lcd.clear();
  lcd.print("Waiting...");
  setLcdColor("WHITE"); 
  
  btnColors[0] = "OFF"; btnColors[1] = "OFF"; btnColors[2] = "OFF";
  targetColor = "OFF";
}

// ==========================================
// Utility: Set RGB LCD Backlight Color
// ==========================================
void setLcdColor(String colorName) {
  if (colorName == "RED")         lcd.setRGB(255, 0, 0);
  else if (colorName == "GREEN")  lcd.setRGB(0, 255, 0);
  else if (colorName == "BLUE")   lcd.setRGB(0, 0, 255);
  else if (colorName == "YELLOW") lcd.setRGB(255, 255, 0);
  else if (colorName == "CYAN")   lcd.setRGB(0, 255, 255);
  else if (colorName == "PURPLE") lcd.setRGB(128, 0, 255); // 或是 255, 0, 255
  else if (colorName == "WHITE")  lcd.setRGB(255, 255, 255);
  else if (colorName == "OFF")    lcd.setRGB(0, 0, 0);
}

// ==========================================
// Utility: Fill Matrix with Color
// ==========================================
void setMatrixColor(int matrixNum, String colorName) {
  uint32_t targetColor = matrix1.Color(0, 0, 0); 
  
  if (colorName == "RED")         targetColor = matrix1.Color(255, 0, 0);
  else if (colorName == "GREEN")  targetColor = matrix1.Color(0, 255, 0);
  else if (colorName == "BLUE")   targetColor = matrix1.Color(0, 0, 255);
  else if (colorName == "YELLOW") targetColor = matrix1.Color(255, 255, 0);
  else if (colorName == "CYAN")   targetColor = matrix1.Color(0, 255, 255);
  else if (colorName == "PURPLE") targetColor = matrix1.Color(128, 0, 255);

  if (matrixNum == 1) {
    matrix1.fill(targetColor, 0, NUMPIXELS);
    matrix1.show();
  } else if (matrixNum == 2) {
    matrix2.fill(targetColor, 0, NUMPIXELS);
    matrix2.show();
  }
}