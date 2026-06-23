#include <Arduino.h>
#include "driver/twai.h"
#include <Adafruit_NeoPixel.h>

// --- PIN CONFIGURATION ---
#define LED_PIN       4
#define NUM_LEDS      9
#define CAN_TX_PIN    39
#define CAN_RX_PIN    38

// --- BUTTON PINS ---
#define BTN_DOWN_PIN  6
#define BTN_UP_PIN    41

// --- STATE VARIABLES ---
int currentRpm = 0;
const int rpmStart = 3000;
const int rpmMax = 9500;

int activeScreen = 1; 
unsigned long lastButtonPress = 0; 
const unsigned long debounceDelay = 100;

// --- HARDWARE INITIALIZATION ---
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- FUNCTION DECLARATIONS ---
void setupCAN();
void checkCANMessages();
void checkButtons();
void sendButtonCANMessage(uint32_t messageId, uint8_t buttonData);
void sendActiveScreenCANMessage(uint8_t screenNum);
void updateLEDs(int rpm);
void playBootLedAnimation();

// --- HELPERS ---
inline int16_t parseLE(const uint8_t* data, int offset) { 
  uint16_t raw_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
  return raw_val;
}

void setup() {
  Serial.begin(115200);
  
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  playBootLedAnimation();

  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);

  // Force the RX pin HIGH so the ESP32 thinks the CAN bus is quiet
  pinMode(CAN_RX_PIN, INPUT_PULLUP);

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
  // CHANGED: Using TWAI_MODE_NO_ACK for desk testing without a network.
  // Change back to TWAI_MODE_NORMAL when plugging into the actual car/dashboard!
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NO_ACK);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    if (twai_start() == ESP_OK) {
      Serial.println("CAN Bus initialized successfully (NO ACK MODE).");
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
  bool btnDownPressed = (digitalRead(BTN_DOWN_PIN) == LOW);
  bool btnUpPressed = (digitalRead(BTN_UP_PIN) == LOW);

  // CHANGED: Debounce is back to protect against noise and hyper-fast loops
  if ((btnDownPressed || btnUpPressed) && (millis() - lastButtonPress > debounceDelay)) {
    
    if (btnUpPressed) {
      activeScreen++;
      if (activeScreen > 4) activeScreen = 4;
      sendActiveScreenCANMessage(activeScreen);
    } 
    else if (btnDownPressed) {
      activeScreen--;
      if (activeScreen < 1) activeScreen = 1;
      sendActiveScreenCANMessage(activeScreen);
    }
    
    lastButtonPress = millis();
  }
}

// ==========================================
// LED DISPLAY & ANIMATIONS
// ==========================================

void updateLEDs(int rpm) {
  strip.clear();
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