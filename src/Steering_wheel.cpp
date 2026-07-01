#include <Arduino.h>
#include "driver/twai.h"
#include <Adafruit_NeoPixel.h>

// --- PIN CONFIGURATION ---
#define LED_PIN       4
#define NUM_LEDS      9
#define CAN_TX_PIN    1
#define CAN_RX_PIN    2

// --- BUTTON PINS ---
#define BTN_DOWN_PIN  5 
#define BTN_UP_PIN    42
#define BTN_TEST_PIN  7 // PDU Error Test Button

// --- STATE VARIABLES ---
int currentRpm = 0;
const int rpmStart = 3000;
const int rpmMax = 9500;

int activeScreen = 1; 

// Toggle state for our simulated PDU error
bool testErrorActive = false; 

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
void sendPDUErrorTestCANMessage(bool hasError);
void updateLEDs(int rpm);
void playBootLedAnimation();

// --- HELPERS ---
inline int16_t parseLE(const uint8_t* data, int offset) { 
  uint16_t raw_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
  return raw_val;
}

void setup() {
  // FIX: Increased from 500 to 800 to give the Dashboard time to draw its boot logos!
  // Tweak this number up or down by 50ms to get the LED timing synced perfectly.
  delay(800); 

  Serial.begin(115200);
  Serial.println("Starting Steering Wheel CAN Controller...");
  
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  playBootLedAnimation();

  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_TEST_PIN, INPUT_PULLUP);

  setupCAN();
}

void loop() {
  checkCANMessages();
  checkButtons();
  updateLEDs(currentRpm);
  
  delay(10);
}

// ==========================================
// CAN BUS IMPLEMENTATION
// ==========================================

void setupCAN() {
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
  if (twai_receive(&rx_msg, pdMS_TO_TICKS(1)) == ESP_OK) {
     if (rx_msg.identifier == 0x520) {
       currentRpm = parseLE(rx_msg.data, 0);
     }
  }
}

void sendActiveScreenCANMessage(uint8_t screenNum) {
  // FIX: Added {} to zero-initialize the struct and prevent garbage memory from breaking the flags
  twai_message_t tx_msg = {}; 
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

void sendPDUErrorTestCANMessage(bool hasError) {
  // FIX: Added {} to zero-initialize the struct
  twai_message_t tx_msg = {}; 
  tx_msg.identifier = 0x620;   
  tx_msg.extd = 0;             
  tx_msg.data_length_code = 1; 
  tx_msg.data[0] = hasError ? 0x07 : 0x00;  
  
  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
    Serial.print("PDU Test Error sent! State: ");
    Serial.println(hasError ? "ACTIVE" : "CLEARED");
  } else {
    Serial.println("Warning: Failed to send PDU test message on CAN bus.");
  }
}

// ==========================================
// BUTTON HANDLING
// ==========================================

void checkButtons() {
  unsigned long currentMillis = millis();

  bool downPressed = (digitalRead(BTN_DOWN_PIN) == HIGH);
  bool upPressed = (digitalRead(BTN_UP_PIN) == HIGH);
  bool testPressed = (digitalRead(BTN_TEST_PIN) == HIGH); 

  if ((downPressed || upPressed || testPressed) && (currentMillis - lastBtnPress > debounceDelay)) {
    
    if (upPressed) {
      activeScreen++;
      if (activeScreen > 4) activeScreen = 4; 
      sendActiveScreenCANMessage(activeScreen);
    } 
    else if (downPressed) {
      activeScreen--;
      if (activeScreen < 1) activeScreen = 1; 
      sendActiveScreenCANMessage(activeScreen);
    }
    else if (testPressed) {
      testErrorActive = !testErrorActive; 
      sendPDUErrorTestCANMessage(testErrorActive);
    }

    lastBtnPress = currentMillis;
  }
}

// ==========================================
// LED DISPLAY
// ==========================================

void updateLEDs(int rpm) {
  int numLedsToLight = 0;
  bool redline = false;

  if (rpm >= rpmMax) {
    redline = true;
  } else if (rpmMax > rpmStart && rpm >= rpmStart) { 
    numLedsToLight = static_cast<int>((rpm - rpmStart) * NUM_LEDS / static_cast<float>(rpmMax - rpmStart)) + 1;
    if (numLedsToLight > NUM_LEDS) numLedsToLight = NUM_LEDS;
  }

  strip.clear(); 

  if (redline) {
    if ((millis() / 50) % 2 == 0) {
      for(int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255));
      }
    }
  } else {
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