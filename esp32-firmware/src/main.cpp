#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 17;
const int LOADCELL_SCK_PIN = 16;

// Caliper wiring
const int CALIPER_CLK_PIN = 18;
const int CALIPER_DATA_PIN = 19;

HX711 scale;

// Variables to accumulate the bits
volatile uint32_t word1_temp = 0;
volatile uint8_t bit_count = 0;
volatile unsigned long last_bit_time = 0;

// Variables to store the full words for processing
volatile uint32_t word1 = 0;
volatile bool data_ready = false;

// Current caliper value
float scale_val = 0;
float scale_val_max = -1000;
float caliper_val = 0;
float caliper_val_max = -1000;

const float CALIPER_FACTOR = 57300 / 0.546;

hw_timer_t *my_timer = NULL;
volatile bool read_value = false;
const uint64_t step_us = 100000;
uint64_t cur_us = 0;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// LED wiring
const int LED_GRN_PIN = 23;
const int LED_RED_PIN = 5;

// Scale tare
const int TARE_PIN = 4;
volatile bool tare_needed = false;

// BLE Server, Service, and Characteristic
BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pCharacteristic = NULL;

// UUIDs for BLE service and characteristic
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

void IRAM_ATTR onTimer() {
  cur_us += step_us;
  read_value = true;
}

// Interrupt service routine to read CALIPER_DATA_PIN
void IRAM_ATTR readCaliperData() {
  unsigned long current_time = millis();
  if (digitalRead(CALIPER_CLK_PIN)) return;
  if (current_time - last_bit_time > 10) {
    word1_temp = 0;
    bit_count = 0;
  }
  last_bit_time = current_time;
  uint8_t bit = digitalRead(CALIPER_DATA_PIN) ? 0 : 1;
  word1_temp |= (bit << bit_count);
  bit_count++;
  if (bit_count >= 24) {
    word1 = word1_temp;
    word1_temp = 0;
    bit_count = 0;
    data_ready = true;
  }
}

void IRAM_ATTR tareScale() {
  tare_needed = true;
}

void setup() {
  Serial.begin(115200);

  digitalWrite(LED_GRN_PIN, 1);
  digitalWrite(LED_RED_PIN, 0);

  pinMode(LED_GRN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIPER_FACTOR);
  scale.tare();

  pinMode(CALIPER_CLK_PIN, INPUT);
  pinMode(CALIPER_DATA_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(CALIPER_CLK_PIN), readCaliperData, FALLING);
  pinMode(TARE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TARE_PIN), tareScale, FALLING);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();

  my_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(my_timer, &onTimer, true);
  timerAlarmWrite(my_timer, step_us, true);
  timerAlarmEnable(my_timer);

  // Initialize BLE
  BLEDevice::init("ESP32_Tamper");
  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Functions that help with iPhone connections
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE service started");
}

void printValue(float value, int decimalPlaces) {
  float threshold = 0.5 / pow(10, decimalPlaces);
  if (value > -threshold && value < threshold) {
    value = 0.0;
  }
  display.print(value, decimalPlaces);
}

void loop() {
  if (read_value) {
    scale_val = scale.get_units();

    if (scale_val > scale_val_max) {
      scale_val_max = scale_val;
    }

    if (caliper_val > caliper_val_max) {
      caliper_val_max = caliper_val;
    }

    // Prepare data string
    String data = String(cur_us / 1000000., 1) + "," +
                  String(scale_val, 3) + "," +
                  String(caliper_val, 2);

    // Send data via BLE
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();

    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setRotation(1);

    display.println("Force (kg)");
    display.println("");
    display.setTextSize(2);
    printValue(scale_val, 2);
    display.println("");
    printValue(scale_val_max, 2);
    display.println("");
    display.setTextSize(1);
    display.println("");

    display.println("Pos'n (mm)");
    display.println("");
    display.setTextSize(2);
    printValue(caliper_val, 2);
    display.println("");
    printValue(caliper_val_max, 2);
    display.println("");
    display.setTextSize(1);
    display.println("");

    display.display();

    // Update LEDs
    if (scale_val > 30 / 2.2) {
      digitalWrite(LED_GRN_PIN, 0);
      digitalWrite(LED_RED_PIN, 1);
    } else {
      digitalWrite(LED_GRN_PIN, 1);
      digitalWrite(LED_RED_PIN, 0);
    }

    read_value = false;
  }

  if (data_ready) {
    caliper_val = (word1 & ~(15 << 20)) / 100.0;
    if (word1 & (1 << 20)) caliper_val = -caliper_val;
    data_ready = false;
  }

  if (tare_needed) {
    scale.tare();
    scale_val_max = -1000;
    caliper_val_max = -1000;
    tare_needed = false;
  }
}
