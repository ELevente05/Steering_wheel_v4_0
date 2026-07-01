#include <Arduino.h>
#include "driver/twai.h"

// --- PIN CONFIGURATION ---
#define CAN_TX_PIN    1
#define CAN_RX_PIN    2

// --- BUTTON PINS ---
// Front Buttons (Normally Closed to Ground)
#define BTN_1_PIN     5
#define BTN_2_PIN     6
#define BTN_3_PIN     7
#define BTN_4_PIN     40
#define BTN_5_PIN     41
#define BTN_6_PIN     42

// Back Buttons (Normally Open to Ground)
#define BTN_BCK_1_PIN 39
#define BTN_BCK_2_PIN 15

// --- ROTARY DIP SWITCH PINS ---
// Common to Ground (Active LOW)
#define DIP_PIN_1     16 // Binary weight: 1
#define DIP_PIN_2     17 // Binary weight: 2
#define DIP_PIN_4     18 // Binary weight: 4
#define DIP_PIN_8     8  // Binary weight: 8

// --- TIMING ---
unsigned long lastTransmission = 0;
const unsigned long transmissionInterval = 20; // 50Hz update rate for responsive gaming

void setupCAN() {
  // Initialize TWAI (CAN) at 500kbps
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    if (twai_start() == ESP_OK) {
      Serial.println("Steering CAN Bus initialized successfully.");
    }
  } else {
    Serial.println("Failed to initialize CAN Bus.");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Front Buttons (Normally Closed - Reads HIGH when pressed)
  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_2_PIN, INPUT_PULLUP);
  pinMode(BTN_3_PIN, INPUT_PULLUP);
  pinMode(BTN_4_PIN, INPUT_PULLUP);
  pinMode(BTN_5_PIN, INPUT_PULLUP);
  pinMode(BTN_6_PIN, INPUT_PULLUP);

  // Initialize Back Buttons (Normally Open - Reads LOW when pressed)
  pinMode(BTN_BCK_1_PIN, INPUT_PULLUP);
  pinMode(BTN_BCK_2_PIN, INPUT_PULLUP);

  // Initialize Rotary DIP Switch
  pinMode(DIP_PIN_1, INPUT_PULLUP);
  pinMode(DIP_PIN_2, INPUT_PULLUP);
  pinMode(DIP_PIN_4, INPUT_PULLUP);
  pinMode(DIP_PIN_8, INPUT_PULLUP);

  setupCAN();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastTransmission >= transmissionInterval) {
    lastTransmission = currentMillis;

    // 1. Read Button States into a single Byte (Bitmask)
    uint8_t buttonState = 0;
    
    // Front Buttons (NC: HIGH is pressed)
    if (digitalRead(BTN_1_PIN) == HIGH) buttonState |= (1 << 0);
    if (digitalRead(BTN_2_PIN) == HIGH) buttonState |= (1 << 1);
    if (digitalRead(BTN_3_PIN) == HIGH) buttonState |= (1 << 2);
    if (digitalRead(BTN_4_PIN) == HIGH) buttonState |= (1 << 3);
    if (digitalRead(BTN_5_PIN) == HIGH) buttonState |= (1 << 4);
    if (digitalRead(BTN_6_PIN) == HIGH) buttonState |= (1 << 5);

    // Back Buttons (NO: LOW is pressed)
    if (digitalRead(BTN_BCK_1_PIN) == LOW) buttonState |= (1 << 6);
    if (digitalRead(BTN_BCK_2_PIN) == LOW) buttonState |= (1 << 7);

    // 2. Read Rotary DIP Switch
    uint8_t dipValue = 0;
    if (digitalRead(DIP_PIN_1) == LOW) dipValue |= 1;
    if (digitalRead(DIP_PIN_2) == LOW) dipValue |= 2;
    if (digitalRead(DIP_PIN_4) == LOW) dipValue |= 4;
    if (digitalRead(DIP_PIN_8) == LOW) dipValue |= 8;

    // 3. Transmit DOOM Controller Frame
    twai_message_t tx_msg = {}; 
    tx_msg.identifier = 0x666;   // DOOM specific CAN ID!
    tx_msg.extd = 0;             
    tx_msg.data_length_code = 2; 
    tx_msg.data[0] = buttonState;  
    tx_msg.data[1] = dipValue;
    
    twai_transmit(&tx_msg, pdMS_TO_TICKS(5));
  }
}