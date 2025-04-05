#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

#include <WiFi.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <TetrisMatrixDraw.h>
#include <RTClib.h>
#include <ezTime.h>
#include <time.h>
#include <Preferences.h>

// Wifi network station credentials

RTC_DS3231 rtc;
Preferences prefs;

// Hours in 24H format (i.e. for 3:45 PM, timeHour = 15, timeMinute = 45)
int timeHour = 00;
int timeMinute = 00;

// Function to set time over serial
void setTimeFromSerial(String dateTime, int tzOffset);
void setRTCFromSerial(String dateTime, String tzName);


const int panelResX = 64;   // Number of pixels wide of each INDIVIDUAL panel module.
const int panelResY = 32;   // Number of pixels tall of each INDIVIDUAL panel module.
const int panel_chain = 1;  // Total number of panels chained one to another.


// Sets whether the clock should be 12 hour format or not.
bool twelveHourFormat = true;


bool forceRefresh = false;

MatrixPanel_I2S_DMA *dma_display = nullptr;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * animationTimer = NULL;

unsigned long animationDue = 0;
unsigned long animationDelay = 100; // Smaller number == faster

uint16_t myBLACK = dma_display->color565(0, 0, 0);

TetrisMatrixDraw tetris(*dma_display); // Main clock
TetrisMatrixDraw tetris2(*dma_display); // The "M" of AM/PM
TetrisMatrixDraw tetris3(*dma_display); // The "P" or "A" of AM/PM

Timezone myTZ;
unsigned long oneSecondLoopDue = 0;

bool showColon = true;
volatile bool finishedAnimating = false;
bool displayIntro = true;

String lastDisplayedTime = "";
String lastDisplayedAmPm = "";

const int y_offset = panelResY / 2;

// This method is for controlling the tetris library draw calls
void animationHandler()
{
  if (!finishedAnimating) {
    dma_display->fillScreen(myBLACK);
    if (displayIntro) {
      finishedAnimating = tetris.drawText(1, 5 + y_offset);
    } else {
      if (twelveHourFormat) {
        bool tetris1Done = false;
        bool tetris2Done = false;
        bool tetris3Done = false;

        tetris1Done = tetris.drawNumbers(-6, 10 + y_offset, showColon);
        tetris2Done = tetris2.drawText(56, 9 + y_offset);

        // Only draw the top letter once the bottom letter is finished.
        if (tetris2Done) {
          tetris3Done = tetris3.drawText(56, -1 + y_offset);
        }

        finishedAnimating = tetris1Done && tetris2Done && tetris3Done;

      } else {
        finishedAnimating = tetris.drawNumbers(2, 10 + y_offset, showColon);
      }
    }
    dma_display->flipDMABuffer();
  }

}

void drawIntro(int x = 0, int y = 0)
{
  dma_display->fillScreen(myBLACK);
  tetris.drawChar("L", x, y, tetris.tetrisGREEN);
  tetris.drawChar("o", x+5, y, tetris.tetrisRED);
  tetris.drawChar("a", x+11, y, tetris.tetrisYELLOW);
  tetris.drawChar("d", x+17, y, tetris.tetrisCYAN);
  tetris.drawChar("i", x+22, y, tetris.tetrisMAGENTA);
  tetris.drawChar("n", x+27, y, tetris.tetrisBLUE);
  tetris.drawChar("g", x+32, y, tetris.tetrisORANGE);
  tetris.drawChar(".", x+37, y, tetris.tetrisGREEN);
  tetris.drawChar(".", x+42, y, tetris.tetrisRED);
  tetris.drawChar(".", x+47, y, tetris.tetrisYELLOW);
  
  dma_display->flipDMABuffer();
}

void drawConnecting(int x = 0, int y = 0, const char* text = "...")
{
  dma_display->fillScreen(myBLACK);

  // Define an array of colors to cycle through
  uint16_t colors[] = {
    tetris.tetrisCYAN, tetris.tetrisMAGENTA, tetris.tetrisYELLOW,
    tetris.tetrisGREEN, tetris.tetrisBLUE, tetris.tetrisRED, 
    tetris.tetrisWHITE
  };

  int numColors = sizeof(colors) / sizeof(colors[0]);
  // Loop through each character in the C-string
  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];  // Get the current character
    int colorIndex = i % numColors;  // Cycle through colors
    tetris.drawChar(String(c), x + (i * 6), y, colors[colorIndex]); // Adjust spacing
  }  

  dma_display->flipDMABuffer();
}

void setup() {
  Serial.begin(115200);
  if (!rtc.begin()) {
    Serial.println("RTC module is NOT found");
    while (1);
  }

  // Initializing preferences
  prefs.begin("ps3dclock", false);
  
  String savedTZ = prefs.getString("tz", "");
  if(savedTZ != "") {
    myTZ.setLocation(savedTZ);
  } else {
    prefs.putString("tz", "UTC");
  }

  if(rtc.lostPower()) {
    rtc.adjust(DateTime(2025, 3, 31, timeHour, timeMinute, 0));
  } else {
    Serial.println("Getting time from RTC.");
  }

  HUB75_I2S_CFG mxconfig(
    panelResX,   // Module width
    panelResY,   // Module height
    panel_chain  // Chain length
  );

  mxconfig.double_buff = true;
  mxconfig.gpio.e = 18;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  //mxconfig.clkphase = false;

  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->fillScreen(myBLACK);

  tetris.display = dma_display; // Main clock
  tetris2.display = dma_display; // The "M" of AM/PM
  tetris3.display = dma_display; // The "P" or "A" of AM/PM


  
  // "Powered By"
  drawIntro(6, -4 + y_offset);
  delay(2000);

  // Start the Animation Timer
  tetris.setText("PITSTOP3D");

  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(2000);
  finishedAnimating = false;
  displayIntro = false;
  tetris.scale = 2;
  //tetris.setTime("00:00", true);
}

void setMatrixTime() {
  DateTime now = rtc.now();
  timeHour = now.hour();
  timeMinute = now.minute();

  Serial.println("----------");
  Serial.println(myTZ.getOffset());
  Serial.println(myTZ.getTimezoneName());
  Serial.println(myTZ.isDST());
  Serial.println(myTZ.dateTime(now.unixtime()));

  // Print the current time in a readable format
  Serial.print("Current time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  Serial.println("----------");

  char time_string[6];
  snprintf(time_string, sizeof(time_string), "%02d:%02d", timeHour, timeMinute);
  String timeStr = String(time_string);
  String timeString = "";
  String AmPmString = "";
  if (twelveHourFormat) {
    
    String hourStr = timeStr.substring(0, 2);
    
    //Get if its "AM" or "PM"
    int doubleHour = hourStr.toInt();
  
    if (doubleHour < 12){
      AmPmString = "AM";
    }
    else{
      AmPmString = "PM";
    }
    if (lastDisplayedAmPm != AmPmString) {
      Serial.println(AmPmString);
      lastDisplayedAmPm = AmPmString;
      // Second character is always "M"
      // so need to parse it out
      tetris2.setText("M", forceRefresh);

      // Parse out first letter of String
      tetris3.setText(AmPmString.substring(0, 1), forceRefresh);
    }

    int currentHour = hourStr.toInt() % 12;
    if (currentHour == 0) {
      currentHour = 12;
    }
    hourStr = String(currentHour);
    timeStr = hourStr + timeStr.substring(2);
    // Get the time in format "1:15" or 11:15 (12 hour, no leading 0)
    // Check the EZTime Github page for info on
    // time formatting
    timeString = timeStr;

    //If the length is only 4, pad it with
    // a space at the beginning
    if (timeStr.length() == 4) {
      timeStr = " " + timeStr;
    }

  } else {
    // Get time in format "01:15" or "22:15"(24 hour with leading 0)
    timeString = timeStr;
  }

  // Only update Time if its different
  if (lastDisplayedTime != timeStr) {
    Serial.println(timeStr);
    lastDisplayedTime = timeStr;
    tetris.setTime(timeStr, forceRefresh);

    // Must set this to false so animation knows
    // to start again
    finishedAnimating = false;
  }
}

void handleColonAfterAnimation() {

  // It will draw the colon every time, but when the colour is black it
  // should look like its clearing it.
  uint16_t colour =  showColon ? tetris.tetrisWHITE : tetris.tetrisBLACK;
  // The x position that you draw the tetris animation object
  int x = twelveHourFormat ? -6 : 2;
  // The y position adjusted for where the blocks will fall from
  // (this could be better!)
  int y = 10 + y_offset - (TETRIS_Y_DROP_DEFAULT * tetris.scale);
  tetris.drawColon(x, y, colour);
  dma_display->flipDMABuffer();
}


void loop() {

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    String tzName;
    String dateTime;
    Serial.print("Got: ");
    Serial.println(input);

    // Expecting format: "YYYY-MM-DDTHH:MM:SS TZ_ID"
    // ** NOTE **: Time will alwas be in UTC Format"
    int separator = input.lastIndexOf(' ');
    if (separator != -1) {
        dateTime = input.substring(0, separator);
        tzName = input.substring(separator + 1);
        //setTimeFromSerial(dateTime, tzOffset);
        myTZ.setLocation(tzName);
        setRTCFromSerial(dateTime, tzName);
    }
  }

  unsigned long now = millis();
  if (now > oneSecondLoopDue) {
    // We can call this often, but it will only
    // update when it needs to
    setMatrixTime();
    showColon = !showColon;

    // To reduce flicker on the screen we stop clearing the screen
    // when the animation is finished, but we still need the colon to
    // to blink
    if (finishedAnimating) {
      handleColonAfterAnimation();
    }
    oneSecondLoopDue = now + 1000;
  }
  now = millis();
  if (now > animationDue) {
    animationHandler();
    animationDue = now + animationDelay;
  }
}

void setTimeFromSerial(String dateTime, int tzOffset) {
  struct tm timeinfo;

  sscanf(dateTime.c_str(), "%d-%d-%dT%d:%d:%d", 
          &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday, 
          &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
    
    timeinfo.tm_year -= 1900; // Adjust for struct tm
    timeinfo.tm_mon -= 1;

    time_t t = mktime(&timeinfo);
    t += tzOffset * 3600; // Apply timezone offset
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    Serial.println("Time updated successfully!");
}

void setRTCFromSerial(String dateTime, String tzName) {
  int year, month, day, hour, minute, second;
    sscanf(dateTime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // Time is always received in UTC Format
    
    // Correct overflow/underflow
    if (hour < 0) {
        hour += 24;
        day -= 1;
    } else if (hour >= 24) {
        hour -= 24;
        day += 1;
    }

    DateTime newTime(year, month, day, hour, minute, second);
    rtc.adjust(newTime);  // Update RTC module
    time_t utc = rtc.now().unixtime();
    setTime(utc); // Sets ezTime's internal UTC clock

    Serial.println(tzName);
    myTZ.setLocation(tzName);
  
    Serial.println("RTC Time Set Successfully!");
    prefs.putString("tz", tzName);
}