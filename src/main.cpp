//#include <driver/gpio.h>
#include <RTClib.h>
//#include <PxMatrix.h>
//#include <TetrisMatrixDraw.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ------------------------
#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 4 // Generic ESP32
// -----------------------

#define ANIMATION_TIME 50000
MatrixPanel_I2S_DMA matrix;

uint16_t colorRed = matrix.color565(255, 0, 0);

//PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E); // Initialize display
//TetrisMatrixDraw tetris(display); // Initialize Tetris library

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  HUB75_I2S_CFG mxconfig(64, 32, 1);
  matrix.begin(mxconfig);
  matrix.setBrightness(64);
  
  // Initialize the display
  //display.begin(8);
  //display.clearDisplay();
  //display.setTextColor(display.color565(255, 0, 0)); // Red
  matrix.setCursor(0, 0);
  matrix.print("HELLO");
  //display.display(); // Display "HELLO"

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC module is NOT found");
    while (1);
  }

  // Set RTC to current date & time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
  DateTime now = rtc.now();

  // Prepare time string
  char time_string[6];
  snprintf(time_string, sizeof(time_string), "%02d:%02d", now.hour(), now.minute());

  // Debug: Print the time string to the serial monitor
  Serial.print("Formatted Time: ");
  Serial.println(time_string);
  matrix.print(time_string);

  // Clear previous display content and update the time
  //tetris.setTime(time_string, false);
  //tetris.drawNumbers(0, 0, true);

  // Refresh display with updated content
  //display.display();

  delay(1000); // Delay 1 second before updating
  matrix.clearScreen();
  matrix.setCursor(0,0);
}
