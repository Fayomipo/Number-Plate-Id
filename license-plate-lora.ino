#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ====================== CONFIGURATION ======================
// LoRa module pins
#define SS_PIN 10
#define RST_PIN 9
#define DIO0_PIN 2

// LCD I2C address
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// System parameters
#define SERIAL_BAUD_RATE 9600
#define LORA_FREQUENCY 915E6  // 915MHz - adjust for your region
#define LOG_INTERVAL 60000    // Log stats every minute (60000ms)
#define DISPLAY_TIMEOUT 10000 // Display plate for 10 seconds before showing stats
#define MAX_PLATES 10         // Number of plates to store in EEPROM history
#define EEPROM_START_ADDR 0   // Starting address for EEPROM storage

// LoRa radio parameters
#define LORA_SPREADING_FACTOR 7
#define LORA_SIGNAL_BANDWIDTH 125E3
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0x12
#define LORA_TX_POWER 17      // dBm

// Initialize the LCD
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

unsigned long lastStatsTime = 0;
unsigned long lastPacketTime = 0;
unsigned long uptime = 0;
unsigned int packetsReceived = 0;
int lastRssi = 0;
float lastSnr = 0.0;
bool displayingPlate = false;
String currentPlate = "";
String lastPlates[MAX_PLATES];
int plateIndex = 0;

// System states
enum DisplayMode {
  WAITING,
  DISPLAY_PLATE,
  DISPLAY_STATS
};

DisplayMode currentMode = WAITING;

// ====================== FUNCTION DECLARATIONS ======================
bool initLoRa();
bool initLCD();
void displayWaitingScreen();
void displayPlate(String plateNumber, int rssi, float snr);
void displayStats();
void processLoRaPacket();
void storeToHistory(String plateNumber);
void loadHistory();
void scrollMessage(String message, int row, int delayTime);

void setup() {
  // Initialize Serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  
  delay(100);
  
  Serial.println(F("License Plate LoRa Receiver Starting..."));

  // Initialize components
  if (!initLoRa()) {
    Serial.println(F("FATAL: LoRa initialization failed"));
    // Visual indicator of failure
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
      delay(1000);
    }
  }
  
  if (!initLCD()) {
    Serial.println(F("WARNING: LCD initialization failed"));
  }
  
  // Load previous license plates from EEPROM
  loadHistory();
  
  displayWaitingScreen();
  
  lastStatsTime = millis();
  
  Serial.println(F("Setup complete. Waiting for license plates..."));
}

// ====================== MAIN LOOP ======================
void loop() {
  unsigned long currentTime = millis();
  
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    processLoRaPacket();
    lastPacketTime = currentTime;
    currentMode = DISPLAY_PLATE;
    displayingPlate = true;
  }
  
  if (currentMode == DISPLAY_PLATE && displayingPlate) {
    if (currentTime - lastPacketTime > DISPLAY_TIMEOUT) {
      displayingPlate = false;
      currentMode = DISPLAY_STATS;
      displayStats();
    }
  } 
  else if (currentMode != WAITING && currentTime - lastStatsTime > LOG_INTERVAL) {
    lastStatsTime = currentTime;
    uptime = currentTime / 1000; // Convert to seconds
    
    // Log stats to serial
    Serial.println(F("======= SYSTEM STATS ======="));
    Serial.print(F("Uptime: "));
    Serial.print(uptime / 60);
    Serial.println(F(" minutes"));
    Serial.print(F("Packets received: "));
    Serial.println(packetsReceived);
    Serial.print(F("Last RSSI: "));
    Serial.println(lastRssi);
    Serial.print(F("Last SNR: "));
    Serial.println(lastSnr);
    Serial.println(F("==========================="));
    
    // If not currently showing a plate, update the stats display
    if (!displayingPlate) {
      displayStats();
    }
  }
  
  delay(10);
}

// ====================== FUNCTION IMPLEMENTATIONS ======================

/**
 * Initialize LoRa module with error handling
 */
bool initLoRa() {
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  
  // Try initialization with timeout
  unsigned long startAttemptTime = millis();
  while (!LoRa.begin(LORA_FREQUENCY)) {
    if (millis() - startAttemptTime > 5000) {
      return false; // Timeout after 5 seconds
    }
    delay(100);
  }
  
  // Configure LoRa parameters for optimal reception
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setTxPower(LORA_TX_POWER);
  
  // Enable automatic CRC checking
  LoRa.enableCrc();
  
  Serial.println(F("LoRa initialized successfully"));
  return true;
}

/**
 * Initialize LCD with error handling
 */
bool initLCD() {
  bool success = false;
  
  for (int attempts = 0; attempts < 3; attempts++) {
    Wire.begin();
    delay(100);
    
    lcd.init();
    delay(100);
    
    lcd.backlight();
    lcd.clear();
    success = true;
    break;
  }
  
  if (success) {
    Serial.println(F("LCD initialized successfully"));
    return true;
  } else {
    return false;
  }
}

/**
 * Display waiting screen
 */
void displayWaitingScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("LoRa Receiver"));
  lcd.setCursor(0, 1);
  lcd.print(F("Waiting..."));
}

/**
 * Display a license plate with signal quality
 */
void displayPlate(String plateNumber, int rssi, float snr) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Plate:"));
  
  // Center the plate number if it's short enough
  int startPos = (plateNumber.length() <= 10) ? 
                (LCD_COLUMNS - plateNumber.length() - 6) / 2 + 6 : 
                6;
  
  lcd.setCursor(startPos, 0);
  lcd.print(plateNumber);
  
  // Display signal quality
  lcd.setCursor(0, 1);
  lcd.print(F("RSSI:"));
  lcd.print(rssi);
  lcd.print(F(" SNR:"));
  lcd.print(snr);
}

/**
 * Display system statistics
 */
void displayStats() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Rcvd:"));
  lcd.print(packetsReceived);
  
  lcd.setCursor(0, 1);
  lcd.print(F("Up:"));
  lcd.print(uptime / 60);
  lcd.print(F("m RSSI:"));
  lcd.print(lastRssi);
}

/**
 * Process received LoRa packet
 */
void processLoRaPacket() {
  String receivedData = "";
  
  while (LoRa.available()) {
    char c = (char)LoRa.read();

    if (c >= 32 && c <= 126) {
      receivedData += c;
    }
  }
  
  // Limit string length to prevent buffer issues
  if (receivedData.length() > 12) {
    receivedData = receivedData.substring(0, 12);
  }
  
  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();
  
  // Update statistics
  packetsReceived++;
  lastRssi = rssi;
  lastSnr = snr;
  currentPlate = receivedData;
  
  // Store to history
  storeToHistory(receivedData);
  
  // Log to serial
  Serial.println(F("------------------------------"));
  Serial.print(F("License Plate: "));
  Serial.println(receivedData);
  Serial.print(F("RSSI: "));
  Serial.print(rssi);
  Serial.print(F(" dBm, SNR: "));
  Serial.print(snr);
  Serial.println(F(" dB"));
  Serial.println(F("------------------------------"));
  
  // Update display
  displayPlate(receivedData, rssi, snr);
}

/**
 * Store a license plate to history in EEPROM
 */
void storeToHistory(String plateNumber) {
  lastPlates[plateIndex] = plateNumber;
  plateIndex = (plateIndex + 1) % MAX_PLATES;
  
  int addr = EEPROM_START_ADDR + (plateIndex * 10); // Allocate 10 bytes per plate
  
  for (unsigned int i = 0; i < plateNumber.length() && i < 9; i++) {
    EEPROM.update(addr + i, plateNumber[i]);
  }
  
  // Null terminate the string
  EEPROM.update(addr + min(plateNumber.length(), 9U), 0);
}

/**
 * Load license plate history from EEPROM
 */
void loadHistory() {
  for (int i = 0; i < MAX_PLATES; i++) {
    lastPlates[i] = "";
    int addr = EEPROM_START_ADDR + (i * 10);
    
    for (int j = 0; j < 9; j++) {
      char c = EEPROM.read(addr + j);
      if (c == 0) break;
      lastPlates[i] += c;
    }
  }
  
  Serial.println(F("License plate history loaded"));
  
  // Print history if any exists
  bool hasHistory = false;
  for (int i = 0; i < MAX_PLATES; i++) {
    if (lastPlates[i].length() > 0) {
      if (!hasHistory) {
        Serial.println(F("Previous plates:"));
        hasHistory = true;
      }
      Serial.print(i + 1);
      Serial.print(F(". "));
      Serial.println(lastPlates[i]);
    }
  }
  
  if (!hasHistory) {
    Serial.println(F("No plate history found"));
  }
}

/**
 * Scroll a message that's too long for the LCD
 */
void scrollMessage(String message, int row, int delayTime) {
  if (message.length() <= LCD_COLUMNS) {
    lcd.setCursor(0, row);
    lcd.print(message);
    return;
  }
  
  // Add spaces at the end for smooth scrolling
  message = message + "    ";
  
  for (unsigned int i = 0; i < message.length() - LCD_COLUMNS + 1; i++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(i, i + LCD_COLUMNS));
    delay(delayTime);
  }
}