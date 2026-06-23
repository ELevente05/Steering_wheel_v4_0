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

int activeScreen = 1; // The dashboard starts on screen 1

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
// Safe Little Endian Parser
inline int16_t parseLE(const uint8_t* data, int offset) { 
  uint16_t raw_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
  return raw_val;
}

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize LEDs
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  // Play the boot animation once on startup
  playBootLedAnimation();

  // 2. Initialize Buttons
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);

  // 3. Initialize CAN Bus
  setupCAN();
}

void loop() {
  // 1. Listen for incoming data (like RPM)
  checkCANMessages();

  // 2. Scan for physical button presses (No Debounce)
  checkButtons();

  // 3. Update the visual display based on the latest data
  updateLEDs(currentRpm);
  
  // A tiny delay to yield resources 
  delay(10);
}

// ==========================================
// CAN BUS IMPLEMENTATION
// ==========================================

void setupCAN() {
  // Configure CAN (TWAI) for 500kbps - adjust if your network is different
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
  // Check if a message is available without blocking the whole loop
  if (twai_receive(&rx_msg, pdMS_TO_TICKS(1)) == ESP_OK) {
    
     if (rx_msg.identifier == 0x520) {
       currentRpm = parseLE(rx_msg.data, 0);
     }
  }
}

void sendButtonCANMessage(uint32_t messageId, uint8_t buttonData) {
  twai_message_t tx_msg;
  tx_msg.identifier = messageId;
  tx_msg.extd = 0;             // Standard ID (set to 1 for Extended ID)
  tx_msg.data_length_code = 1; // Sending 1 byte of data as an example
  tx_msg.data[0] = buttonData; 
  
  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
    Serial.println("Button message sent via CAN!");
  }
}

void sendActiveScreenCANMessage(uint8_t screenNum) {
  twai_message_t tx_msg;
  tx_msg.identifier = 0x524;   // The exact CAN ID your dashboard listens to for screens
  tx_msg.extd = 0;             // Standard ID format
  tx_msg.data_length_code = 1; // We only need to send 1 byte of data
  tx_msg.data[0] = screenNum;  // Load the current screen number into the data frame
  
  // Attempt to send the message
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
  // Read the current physical state of the pins
  bool btnDownPressed = (digitalRead(BTN_DOWN_PIN) == LOW);
  bool btnUpPressed = (digitalRead(BTN_UP_PIN) == LOW);

  if (btnUpPressed) {
    activeScreen++;
    if (activeScreen > 4) activeScreen = 4; // Cap at screen 4
    sendActiveScreenCANMessage(activeScreen);
  } 
  else if (btnDownPressed) {
    activeScreen--;
    if (activeScreen < 1) activeScreen = 1; // Limit to screen 1
    sendActiveScreenCANMessage(activeScreen);
  }
}

// ==========================================
// LED DISPLAY & ANIMATIONS
// ==========================================

void updateLEDs(int rpm) {
  // Your existing RPM math and strip mapping goes here
  // For now, we will just clear it to keep the frame compiling cleanly
  strip.clear();
  strip.show();
}

void playBootLedAnimation() {
  // Step 1: Start with a blank slate
  strip.clear();
  strip.show();

  // Step 2: Sweep inwards from the edges to the center
  for (int step = 0; step < (NUM_LEDS + 1) / 2; ++step) {
    int leftIndex = step;
    int rightIndex = NUM_LEDS - 1 - step;

    // Set the left LED to Red (Red: 255, Green: 0, Blue: 0)
    strip.setPixelColor(leftIndex, strip.Color(255, 0, 0));
    
    // Set the right LED to Red (only if it hasn't overlapped with the left index)
    if (rightIndex != leftIndex) {
      strip.setPixelColor(rightIndex, strip.Color(255, 0, 0));
    }

    // Send the updated colors to the hardware
    strip.show();
    
    // Pause briefly so our eyes can see the animation frame
    delay(130);
  }

  // Step 3: Hold the fully illuminated state for a little over a second
  delay(1100);
  
  // Step 4: Turn off all LEDs to reset
  strip.clear();
  strip.show();
}