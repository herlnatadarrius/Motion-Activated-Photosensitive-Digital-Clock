/*
  Motion-Activated Digital Clock with Auto-Brightness
  ----------------------------------------------------
  Hardware: Arduino Nano
    - DS1307 RTC   : SDA -> A4, SCL -> A5, VCC -> 5V, GND -> GND
    - SSD1306 OLED : SDA -> A4, SCL -> A5, VCC -> 5V, GND -> GND  (shared I2C bus)
    - PIR sensor   : OUT -> D2, VCC -> 5V, GND -> GND
    - LDR module   : DO  -> D3, VCC -> 5V, GND -> GND
                      (DO is digital only: bright/dark, not a graded reading,
                       so the display switches between two contrast levels
                       rather than smoothly ramping.)

  Libraries needed (install via Library Manager):
    - RTClib          by Adafruit
    - Adafruit GFX Library
    - Adafruit SSD1306

  Behavior:
    - Display is OFF until the PIR detects motion.
    - On motion, display turns ON and shows the time.
    - Any new motion while on resets the "stay on" timer.
    - After DISPLAY_TIMEOUT_MS with no motion, display turns off again.
    - While on, OLED contrast is continuously adjusted based on the LDR reading
      (brighter room -> higher contrast, darker room -> lower contrast).
*/

#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Pin assignments ----------
#define PIR_PIN      2
#define LDR_PIN      3

// ---------- Display settings ----------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS  0x3C   // common default; try 0x3D if display doesn't init

// ---------- Timing ----------
const unsigned long DISPLAY_TIMEOUT_MS = 30000UL; // stay on 30s after last motion

// ---------- Brightness mapping ----------
// LDR module's DO pin is digital only (HIGH/LOW), so only two contrast
// levels are possible. Most LDR modules output LOW when the room is bright
// and HIGH when dark (comparator trips as light drops) -- if it's the
// opposite on your board, flip INVERT_LDR to true.
const bool INVERT_LDR = false;
const int  DARK_CONTRAST   = 20;   // used when the room is dark
const int  BRIGHT_CONTRAST = 255;  // used when the room is bright

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS1307 rtc;

unsigned long lastMotionTime = 0;
bool displayIsOn = false;

void setup() {
  Serial.begin(9600);

  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  Wire.begin();
  Wire.setClock(400000); // faster I2C (400kHz) speeds up OLED redraws

  if (!rtc.begin()) {
    Serial.println(F("RTC not found. Check wiring."));
    while (1) delay(10);
  }

  if (!rtc.isrunning()) {
    Serial.println(F("RTC not running, setting to compile time."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // --- ONE-TIME FORCE RESYNC ---
  // Uncomment the line below, upload once, confirm the time is correct,
  // then re-comment it and upload again so it doesn't keep resetting
  // to compile time on every restart.
  
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("OLED not found. Check wiring/address."));
    while (1) delay(10);
  }

  display.clearDisplay();
  display.display();
  turnDisplayOff();
}

void loop() {
  bool motionDetected = digitalRead(PIR_PIN) == HIGH;

  if (motionDetected) {
    lastMotionTime = millis();
    if (!displayIsOn) {
      turnDisplayOn();
    }
  }

  if (displayIsOn) {
    // Timeout check
    if (millis() - lastMotionTime > DISPLAY_TIMEOUT_MS) {
      turnDisplayOff();
    } else {
      updateBrightness();
      showTime();
    }
  }

  delay(20); // small debounce/loop pacing (kept short so the seconds display stays smooth)
}

void turnDisplayOn() {
  displayIsOn = true;
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void turnDisplayOff() {
  displayIsOn = false;
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void updateBrightness() {
  bool isDark = digitalRead(LDR_PIN) == HIGH;

  if (INVERT_LDR) {
    isDark = !isDark;
  }

  int contrast = isDark ? DARK_CONTRAST : BRIGHT_CONTRAST;

  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void showTime() {
  DateTime now = rtc.now();

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(10, 15);
  printTwoDigits(now.hour());
  display.print(":");
  printTwoDigits(now.minute());
  display.print(":");
  printTwoDigits(now.second());

  display.setTextSize(1);
  display.setCursor(10, 45);
  display.print(now.year());
  display.print("-");
  printTwoDigits(now.month());
  display.print("-");
  printTwoDigits(now.day());

  display.display();
}

void printTwoDigits(int value) {
  if (value < 10) display.print("0");
  display.print(value);
}
