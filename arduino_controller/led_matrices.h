#ifndef LED_MATRICES_H
#define LED_MATRICES_H

#include "config.h"

void display_matrix(byte arr[], Color matrix_color, bool left) {
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
      Color brightness_adjusted_color = matrix_color * brightness; 
      led_matrices.setPixelColor(index, brightness_adjusted_color.toNeoPixel(led_matrices));
      index++;
    }
  }
}

void display_matrices(byte arr[], char matrix_color, char other_color = '\0'){
  display_matrix(arr, char_to_color(matrix_color), true);
  if (other_color == '\0') {display_matrix(arr, char_to_color(matrix_color), false);}
  else{ display_matrix(arr, char_to_color(other_color), false);}
}

#endif