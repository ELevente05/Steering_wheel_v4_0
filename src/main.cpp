#include <Arduino.h>
#include "driver/twai.h"
#include <Adafruit_NeoPixel.h>

// --- PIN CONFIGURATION ---
#define LED_PIN       4
#define NUM_LEDS      9
#define CAN_TX_PIN    39
#define CAN_RX_PIN    38

// --- BUTTON PINS ---
#define BTN_DOWN_PIN  5 
#define BTN_UP_PIN    6 

// --- STATE VARIABLES ---
int currentRpm = 0;
const int rpmStart = 3000;
const int rpmMax = 9500;

int activeScreen = 1; 

// Button Debounce Variables
unsigned long lastBtnPress = 0;
const unsigned long debounceDelay = 250; 

// --- HARDWARE INITIALIZATION ---
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- FUNCTION DECLARATIONS ---
void setupCAN();
void checkCANMessages();
void checkButtons();
void sendActiveScreenCANMessage(uint8_t screenNum);
void updateLEDs(int rpm);
void playBootLedAnimation();

// --- HELPERS ---
// Matches the dashboard's exact Little Endian parsing logic
inline int16_t parseLE(const uint8_t* data, int offset) { 
  uint16_t raw_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
  return raw_val;
}

void setup() {
  delay(500); 

  Serial.begin(115200);
  Serial.println("Starting Steering Wheel CAN Controller...");
  
  // 1. Initialize LEDs
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  playBootLedAnimation();

  // 2. Initialize Buttons (INPUT_PULLUP handles the NC hardware)
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);

  // 3. Initialize CAN Bus
  setupCAN();
}

void loop() {
  // 1. Listen for incoming RPM data from the vehicle network
  checkCANMessages();

  // 2. Scan buttons to change screens
  checkButtons();

  // 3. Update the LEDs to match the dashboard exactly
  updateLEDs(currentRpm);
  
  delay(10);
}

// ==========================================
// CAN BUS IMPLEMENTATION
// ==========================================

void setupCAN() {
  // NOTE: Set to TWAI_MODE_NORMAL if connected to the dashboard!
  // If testing on your desk alone, change this back to TWAI_MODE_NO_ACK
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    if (twai_start() == ESP_OK) {
      Serial.println("CAN Bus initialized successfully.");
    }
  } else {
    Serial.println("Failed to initialize CAN Bus.");
  }
}

void checkCANMessages() {
  twai_message_t rx_msg;
  // Non-blocking check for CAN messages
  if (twai_receive(&rx_msg, pdMS_TO_TICKS(1)) == ESP_OK) {
     // ID 0x520 contains the RPM data in the first 2 bytes
     if (rx_msg.identifier == 0x520) {
       currentRpm = parseLE(rx_msg.data, 0);
     }
  }
}

void sendActiveScreenCANMessage(uint8_t screenNum) {
  twai_message_t tx_msg;
  tx_msg.identifier = 0x524;   
  tx_msg.extd = 0;             
  tx_msg.data_length_code = 1; 
  tx_msg.data[0] = screenNum;  
  
  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
    Serial.print("Screen change sent via CAN! Active Screen: ");
    Serial.println(screenNum);
  } else {
    Serial.println("Warning: Failed to send screen change on CAN bus.");
  }
}

// ==========================================
// BUTTON HANDLING
// ==========================================

void checkButtons() {
  unsigned long currentMillis = millis();

  // Reading HIGH because your buttons are Normally Closed (NC)
  bool downPressed = (digitalRead(BTN_DOWN_PIN) == HIGH);
  bool upPressed = (digitalRead(BTN_UP_PIN) == HIGH);

  if ((downPressed || upPressed) && (currentMillis - lastBtnPress > debounceDelay)) {
    
    if (upPressed) {
      activeScreen++;
      if (activeScreen > 4) activeScreen = 4; // Dashboard max screen is 4
    } 
    else if (downPressed) {
      activeScreen--;
      if (activeScreen < 1) activeScreen = 1; // Dashboard min screen is 1
    }

    sendActiveScreenCANMessage(activeScreen);
    lastBtnPress = currentMillis;
  }
}

// ==========================================
// LED DISPLAY
// ==========================================

void updateLEDs(int rpm) {
  int numLedsToLight = 0;
  bool redline = false;

  // Calculate RPM state exactly as the Dashboard does
  if (rpm >= rpmMax) {
    redline = true;
  } else if (rpmMax > rpmStart && rpm >= rpmStart) { 
    numLedsToLight = static_cast<int>((rpm - rpmStart) * NUM_LEDS / static_cast<float>(rpmMax - rpmStart)) + 1;
    if (numLedsToLight > NUM_LEDS) numLedsToLight = NUM_LEDS;
  }

  strip.clear(); 

  if (redline) {
    // Flash blue rapidly when hitting the limiter
    if ((millis() / 50) % 2 == 0) {
      for(int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255));
      }
    }
  } else {
    // Standard RPM mapping colors
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i >= NUM_LEDS - numLedsToLight) {
        if (i >= 6) strip.setPixelColor(i, strip.Color(0, 255, 0));       
        else if (i >= 3) strip.setPixelColor(i, strip.Color(255, 255, 0)); 
        else strip.setPixelColor(i, strip.Color(255, 0, 0));               
      }
    }
  }
  strip.show(); 
}

void playBootLedAnimation() {
  strip.clear();
  strip.show();

  for (int step = 0; step < (NUM_LEDS + 1) / 2; ++step) {
    const int leftIndex = step;
    const int rightIndex = NUM_LEDS - 1 - step;

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