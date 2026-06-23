#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// --- PIN CONFIGURATION ---
#define LED_PIN       4
#define NUM_LEDS      9

// --- BUTTON PINS ---
#define BTN_COLOR_PIN 6  // NC Button to cycle colors

// --- ROTARY DIP SWITCH PINS ---
// Assumes Common (C) is connected to Ground
#define DIP_PIN_1     16 // Binary weight: 1
#define DIP_PIN_2     17 // Binary weight: 2
#define DIP_PIN_4     18 // Binary weight: 4
#define DIP_PIN_8     8  // Binary weight: 8

// --- STATE VARIABLES ---
int currentColorIndex = 0;
int lastNumLedsToLight = -1; 
int lastColorIndex = -1;     

// Button Debounce Variables
unsigned long lastColorBtnPress = 0;
const unsigned long debounceDelay = 300; // 300 milliseconds debounce delay

// --- HARDWARE INITIALIZATION ---
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

const char* colorNames[] = {"Red", "Green", "Blue", "Cyan", "Magenta", "Yellow"};

void checkButtons();
void updateLEDs();
void playBootLedAnimation();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting LED Steering Wheel Controller...");
  
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  playBootLedAnimation();

  // Initialize Buttons & DIP Switch
  pinMode(BTN_COLOR_PIN, INPUT_PULLUP);

  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_4, INPUT_PULLUP);
  pinMode(DIP_PIN_8, INPUT_PULLUP);

  Serial.println("Hardware initialized. Ready for input.");
}

void loop() {
  checkButtons();
  updateLEDs();
  delay(10);
}

// ==========================================
// BUTTON HANDLING
// ==========================================

void checkButtons() {
  unsigned long currentMillis = millis();

  // CHANGED: Reading HIGH because the button is Normally Closed (NC)
  if (digitalRead(BTN_COLOR_PIN) == HIGH) {
    if (currentMillis - lastColorBtnPress > debounceDelay) {
      
      currentColorIndex++;
      if (currentColorIndex > 5) {
        currentColorIndex = 0; 
      }
      
      Serial.print("Button Pressed! Color changed to: ");
      Serial.println(colorNames[currentColorIndex]);
      
      lastColorBtnPress = currentMillis;
    }
  }
}

// ==========================================
// LED DISPLAY
// ==========================================

void updateLEDs() {
  // Read the BCD value. Switch pulls LOW when dot is present in the table.
  uint8_t val1 = (digitalRead(DIP_PIN_1) == LOW) ? 1 : 0;
  uint8_t val2 = (digitalRead(DIP_PIN_2) == LOW) ? 2 : 0;
  uint8_t val4 = (digitalRead(DIP_PIN_4) == LOW) ? 4 : 0;
  uint8_t val8 = (digitalRead(DIP_PIN_8) == LOW) ? 8 : 0;

  // Sum the active weights
  int numLedsToLight = val1 + val2 + val4 + val8;

  if (numLedsToLight > NUM_LEDS) numLedsToLight = NUM_LEDS;

  // Exit early if nothing changed
  if (numLedsToLight == lastNumLedsToLight && currentColorIndex == lastColorIndex) {
    return; 
  }

  if (numLedsToLight != lastNumLedsToLight) {
    Serial.print("Dial Turned! LEDs active: ");
    Serial.println(numLedsToLight);
  }
  
  lastNumLedsToLight = numLedsToLight;
  lastColorIndex = currentColorIndex;

  uint32_t colors[] = {
    strip.Color(255, 0, 0),     // 0: Red
    strip.Color(0, 255, 0),     // 1: Green
    strip.Color(0, 0, 255),     // 2: Blue
    strip.Color(0, 255, 255),   // 3: Cyan
    strip.Color(255, 0, 255),   // 4: Magenta
    strip.Color(255, 255, 0)    // 5: Yellow
  };

  strip.clear();
  for (int i = 0; i < numLedsToLight; i++) {
    strip.setPixelColor(i, colors[currentColorIndex]);
  }
  strip.show();
}

void playBootLedAnimation() {
  strip.clear();
  strip.show();

  for (int step = 0; step < (NUM_LEDS + 1) / 2; ++step) {
    int leftIndex = step;
    int rightIndex = NUM_LEDS - 1 - step;

    strip.setPixelColor(leftIndex, strip.Color(255, 0, 0));
    
    if (rightIndex != leftIndex) {
      strip.setPixelColor(rightIndex, strip.Color(255, 0, 0));
    }

    strip.show();
    delay(130);
  }

  delay(1100);
  strip.clear();
  strip.show();
}