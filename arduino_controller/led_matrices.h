#include "config.h"

void display_eye(byte arr[], int hue, bool left) {
  // We will draw a circle on the display
  // It is a hexagonal matrix, which means we have to do some math to know where each pixel is on the screen

  int rows[] = {4, 5, 6, 7, 6, 5, 4};      // The matrix has 4, 5, 6, 7, 6, 5, 4 rows.
  int NUM_COLUMNS = 7;                     // There are 7 columns
  int index = (left) ? 0 : 37;             // If we draw the left eye, we have to add an offset of 37 (4+5+6+7+6=5+4)
  for (int i = 0; i < NUM_COLUMNS; i++) {
   byte row_data = pgm_read_byte(&arr[i]);
    for (int j = 0; j < rows[i]; j++) {
      int bit_to_read = (left) ? rows[i] - 1 - j : j;
      int brightness = LED_BRIGHTNESS * bitRead(row_data, bit_to_read);
      pixels.setPixelColor(index, pixels.ColorHSV(hue * 256, 255, brightness));
      index++;
    }
  }
}

void display_eyes(byte arr[], int hue){
   display_eye(arr, hue, true);
   display_eye(arr, hue, false);
}