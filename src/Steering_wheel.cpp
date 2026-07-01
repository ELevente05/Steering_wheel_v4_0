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
#define BTN_2_PIN     6
#define BTN_3_PIN     7
#define BTN_BCK_1_PIN 39
#define BTN_BCK_2_PIN 15
#define BTN_4_PIN     40
#define BTN_5_PIN     41
#define BTN_UP_PIN    42

// --- ROTARY DIP SWITCH PINS ---
// Assumes Common (C) is connected to Ground (Active LOW)
#define DIP_PIN_1     16 // Binary weight: 1
#define DIP_PIN_2     17 // Binary weight: 2
#define DIP_PIN_4     18 // Binary weight: 4
#define DIP_PIN_8     8  // Binary weight: 8

// --- STATE VARIABLES ---
int currentRpm = 0;
const int rpmStart = 3000;
const int rpmMax = 9500;

int activeScreen = 1; 

// Button & Switch Timers
unsigned long lastBtnPress = 0;
const unsigned long debounceDelay = 250; 
unsigned long lastErrorBroadcast = 0;
int lastDIPValue = -1;

// --- HARDWARE INITIALIZATION ---
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- FUNCTION DECLARATIONS ---
void setupCAN();
void checkCANMessages();
void checkButtons();
int readRotaryDIP();
void broadcastMockErrors(int mode);
void sendActiveScreenCANMessage(uint8_t screenNum);
void updateLEDs(int rpm);
void playBootLedAnimation();

// --- HELPERS ---
inline int16_t parseLE(const uint8_t* data, int offset) { 
  uint16_t raw_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
  return raw_val;
}

void setup() {
  delay(800); 

  Serial.begin(115200);
  Serial.println("Starting Steering Wheel CAN Controller...");
  
  // 1. Initialize LEDs
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  playBootLedAnimation();

  // 2. Initialize Buttons
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);

  // 3. Initialize Rotary DIP Switch
  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_4, INPUT_PULLUP);
  pinMode(DIP_PIN_8, INPUT_PULLUP);

  // 4. Initialize CAN Bus
  setupCAN();
}

void loop() {
  // 1. Listen for incoming RPM data
  checkCANMessages();

  // 2. Scan screen change buttons
  checkButtons();

  // 3. Check Rotary Switch and Broadcast Errors (every 100ms to compete with real PDU if connected)
  int currentDIP = readRotaryDIP();
  if (currentDIP != lastDIPValue || millis() - lastErrorBroadcast > 100) {
    if (currentDIP != lastDIPValue) {
      Serial.print("Rotary Switch Changed! New Mode: ");
      Serial.println(currentDIP);
      lastDIPValue = currentDIP;
    }
    
    broadcastMockErrors(currentDIP);
    lastErrorBroadcast = millis();
  }

  // 4. Update the LEDs
  updateLEDs(currentRpm);
  
  delay(10);
}

// ==========================================
// ROTARY DIP SWITCH LOGIC
// ==========================================

int readRotaryDIP() {
  int val = 0;
  // Because Common is Ground, a closed switch pulls the pin LOW. 
  // We invert the reading (LOW = true) to calculate the binary value.
  if (digitalRead(DIP_PIN_1) == LOW) val |= 1;
  if (digitalRead(DIP_PIN_2) == LOW) val |= 2;
  if (digitalRead(DIP_PIN_4) == LOW) val |= 4;
  if (digitalRead(DIP_PIN_8) == LOW) val |= 8;
  
  return val;
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
  twai_message_t tx_msg = {}; 
  tx_msg.identifier = 0x524;   
  tx_msg.extd = 0;             
  tx_msg.data_length_code = 1; 
  tx_msg.data[0] = screenNum;  
  
  twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
}

// Simulate the complex telemetry of the PDU based on the rotary dial position
void broadcastMockErrors(int mode) {
  // 1. Handle Global PDU Faults (ID 0x620)
  twai_message_t pdu_msg = {}; 
  pdu_msg.identifier = 0x620;   
  pdu_msg.extd = 0;             
  pdu_msg.data_length_code = 1; 
  
  if (mode == 9)       pdu_msg.data[0] = 0x01; // Low Volt Fault
  else if (mode == 10) pdu_msg.data[0] = 0x02; // Power Calc Fault
  else if (mode == 11) pdu_msg.data[0] = 0x04; // FET Fault
  else                 pdu_msg.data[0] = 0x00; // All Clear
  
  twai_transmit(&pdu_msg, pdMS_TO_TICKS(1));

  // 2. Handle Individual eFuse Faults (IDs 0x710 - 0x717)
  for (int i = 0; i < 8; i++) {
    twai_message_t efuse_msg = {}; 
    efuse_msg.identifier = 0x710 + i;   
    efuse_msg.extd = 0;             
    efuse_msg.data_length_code = 8; 

    // Mode 1 maps to eFuse 0 (0x710). Mode 8 maps to eFuse 7 (0x717).
    if (mode == (i + 1)) {
      // Simulate PMBus Status Word with FAULT_MASK active (0x703F)
      efuse_msg.data[6] = 0x3F; // Low byte
      efuse_msg.data[7] = 0x70; // High byte
    } else {
      // Clean status word
      efuse_msg.data[6] = 0x00; 
      efuse_msg.data[7] = 0x00; 
    }

    twai_transmit(&efuse_msg, pdMS_TO_TICKS(1));
  }
}

// ==========================================
// BUTTON HANDLING
// ==========================================

void checkButtons() {
  unsigned long currentMillis = millis();

  bool downPressed = (digitalRead(BTN_DOWN_PIN) == HIGH);
  bool upPressed = (digitalRead(BTN_UP_PIN) == HIGH);

  if ((downPressed || upPressed) && (currentMillis - lastBtnPress > debounceDelay)) {
    if (upPressed) {
      activeScreen++;
      if (activeScreen > 4) activeScreen = 4; 
    } 
    else if (downPressed) {
      activeScreen--;
      if (activeScreen < 1) activeScreen = 1; 
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