#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// --- PIN AND HARDWARE CONFIGURATION ---
#define LED_PIN    4
#define NUM_LEDS   9

// Initialize the NeoPixel strip with your specific hardware details
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- FUNCTION DECLARATIONS (THE FIX) ---
void playBootLedAnimation(); 

void setup() {
  strip.begin();  
  strip.setBrightness(50);
  strip.clear();
  strip.show();
}

void loop() {
  playBootLedAnimation();
  delay(1000); 
}

// --- ANIMATION FUNCTION ---
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