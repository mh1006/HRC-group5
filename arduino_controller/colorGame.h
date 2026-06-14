#ifndef COLORGAME_H
#define COLORGAME_H

#include "config.h"

// Track if the game is currently active to prevent multiple inputs
bool gameActive = false;

// Debounce timestamps per button (non-blocking)
unsigned long lastDebounceTime[3] = {0, 0, 0};
#define DEBOUNCE_MS 50

// ==========================================
// Utility: Convert char code to string (uses F() to save SRAM)
// ==========================================
void printColorName(char c) {
  switch(c) {
    case 'R': lcd.print(F("RED   ")); break;
    case 'G': lcd.print(F("GREEN ")); break;
    case 'B': lcd.print(F("BLUE  ")); break;
    case 'Y': lcd.print(F("YELLOW")); break;
    case 'C': lcd.print(F("CYAN  ")); break;
    case 'P': lcd.print(F("PURPLE")); break;
    default:  lcd.print(F("OFF   ")); break;
  }
}

// ==========================================
// Utility: Control RGB LCD Backlight
// ==========================================
void setLcdColor(char c) {
  switch(c) {
    case 'R': lcd.setRGB(255, 0, 0); break;
    case 'G': lcd.setRGB(0, 255, 0); break;
    case 'B': lcd.setRGB(0, 0, 255); break;
    case 'Y': lcd.setRGB(255, 255, 0); break;
    case 'C': lcd.setRGB(0, 255, 255); break;
    case 'P': lcd.setRGB(128, 0, 255); break;
    case 'W': lcd.setRGB(255, 255, 255); break; // White
    default:  lcd.setRGB(0, 0, 0); break;       // Off
  }
}

// ==========================================
// Utility: Control NeoPixel Matrices
// ==========================================
void setMatrixColor(int matrixNum, char c) {
  uint32_t targetVal = matrix1.Color(0, 0, 0); 
  switch(c) {
    case 'R': targetVal = matrix1.Color(255, 0, 0); break;
    case 'G': targetVal = matrix1.Color(0, 255, 0); break;
    case 'B': targetVal = matrix1.Color(0, 0, 255); break;
    case 'Y': targetVal = matrix1.Color(255, 255, 0); break;
    case 'C': targetVal = matrix1.Color(0, 255, 255); break;
    case 'P': targetVal = matrix1.Color(128, 0, 255); break;
    case 'W': targetVal = matrix1.Color(255, 255, 255); break; // White
    default:  targetVal = matrix1.Color(0, 0, 0); break;       // Off
  }
  if (matrixNum == 1) {
    matrix1.fill(targetVal, 0, NUMPIXELS); matrix1.show();
  } else if (matrixNum == 2) {
    delay(50);
    matrix2.fill(targetVal, 0, NUMPIXELS); matrix2.show();
  }
}

void setBtnColor(int btnNum, char color) {
  btnColors[btnNum] = color;
}

// ==========================================
// Set New Game Round (Called by Python)
// ==========================================
void setGameColors(char lcd_c, char m1_c, char m2_c, char target) {
  // Check input
  bool unique = (lcd_c != m1_c) && (lcd_c != m2_c) && (m1_c != m2_c);
  bool targetValid = (target == lcd_c) || (target == m1_c) || (target == m2_c);
  if (!unique || !targetValid) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("ERROR! Invalid"));
    return; 
  }

  targetColor = target;
  if (lcd_c == 'O') {
    // Turn off game
    lcd.clear();
    setLcdColor('W'); 
    setMatrixColor(1, 'O'); setMatrixColor(2, 'O');
    gameActive = false;
    return;
  }

  // Update LCD interface
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Target: ")); printColorName(target);
  lcd.setCursor(0, 1);
  lcd.print(F("Btn0:   ")); printColorName(lcd_c);
  
  // Update hardware colors
  setLcdColor(lcd_c); 
  setBtnColor(0, lcd_c);
  setMatrixColor(1, m1_c); 
  setBtnColor(1, m1_c);
  setMatrixColor(2, m2_c); 
  setBtnColor(2, m2_c);
  gameActive = true; // Unlock buttons for user input
}

// ==========================================
// Poll Button States (Active-HIGH for Grove)
// ==========================================
void checkButtons() {
  for (int i = 0; i < 3; i++) {
    bool reading = digitalRead(btnPins[i]);

    if (reading != lastBtnState[i]) {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) >= DEBOUNCE_MS) {
      if (reading != stableBtnState[i]) {
        stableBtnState[i] = reading; 

        if (stableBtnState[i] == HIGH) {
          
          if (btnColors[i] != 'O') {
            Serial.print(i);
            Serial.print(F(" BTN,"));
            Serial.print(btnColors[i]);
            if (btnColors[i] == targetColor) {
              Serial.println(F("->CORRECT"));
            } else {
              Serial.println(F("->INCORRECT"));
            }
          }
          
        } 
      }      
    }
    lastBtnState[i] = reading;
  }
}

#endif