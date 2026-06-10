#ifndef COLORGAME_H
#define COLORGAME_H

#include "config.h"

// Track if the game is currently active to prevent multiple inputs
bool gameActive = false;

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
  }
  if (matrixNum == 1) {
    matrix1.fill(targetVal, 0, NUMPIXELS); matrix1.show();
  } else if (matrixNum == 2) {
    matrix2.fill(targetVal, 0, NUMPIXELS); matrix2.show();
  }
}

// ==========================================
// Set New Game Round (Called by Python)
// ==========================================
void setGameColors(char lcd_c, char m1_c, char m2_c) {
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
  lcd.print(F("Target: ")); printColorName(lcd_c);
  lcd.setCursor(0, 1);
  lcd.print(F("Btn0:   ")); printColorName(lcd_c);
  
  // Update hardware colors
  setLcdColor(lcd_c); 
  setMatrixColor(1, m1_c); 
  setMatrixColor(2, m2_c); 
  
  gameActive = true; // Unlock buttons for user input
}

// ==========================================
// Poll Button States (Active-HIGH for Grove)
// ==========================================
void checkButtons() {
  for (int i = 0; i < 3; i++) {
    bool reading = digitalRead(btnPins[i]);
    
    if (reading == HIGH && lastBtnState[i] == LOW) {
      delay(50); // Debounce
      if (digitalRead(btnPins[i]) == HIGH) { 
        
        // Only trigger if the game is currently active
        if (gameActive) {
          gameActive = false; // Lock immediately after one press
          Serial.print(F("BTN,")); 
          Serial.println(i);  // Send "BTN,0", "BTN,1", or "BTN,2" to Python
        }
        
      }
    }
    lastBtnState[i] = reading; 
  }
}

#endif