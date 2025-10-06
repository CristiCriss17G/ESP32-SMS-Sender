/**
 * @file main.cpp
 * @brief ESP32 SMS Sender main application using T-SIM7000G module
 *
 * This application provides SMS sending capabilities through a T-SIM7000G GSM module
 * with configuration via Bluetooth Low Energy (BLE) and optional WiFi connectivity.
 *
 * Hardware: LilyGO T-SIM7000G (ESP32 + SIM7000G GSM module)
 * Board: ESP32 Dev Module
 *
 * Features:
 * - SMS sending via GSM cellular network
 * - BLE configuration interface for WiFi credentials
 * - WiFi connectivity with automatic connection attempts
 * - GSM modem initialization and network registration
 * - LED status indication
 * - Serial debug output
 *
 * Configuration:
 * - Monitor at 115200 baud
 * - Core Debug Level: None (for production)
 * - Uses TinyGSM library for modem communication
 * - NimBLE for Bluetooth Low Energy functionality
 *
 * @author ESP32 SMS Sender Project
 * @version 1.0
 * @date 2025
 */

// LilyGO T-SIM7000G — Send a single SMS with TinyGSM
// Board: ESP32 Dev Module
// Tools -> Core Debug Level: None
// Monitor at 115200

// #include <Arduino.h>
#include <Ticker.h>
#include "GSettings.hpp"
#include "WifiConnection.hpp"
#include "BTLe.hpp"
#include "HTTPServer.hpp"
#include "Modem.hpp"

#define SECOND 1000l  // 1 second
#define MINUTE 60000l // 1 minute

#define SD_MISO 2  ///< SD card SPI MISO pin
#define SD_MOSI 15 ///< SD card SPI MOSI pin
#define SD_SCLK 14 ///< SD card SPI clock pin
#define SD_CS 13   ///< SD card SPI chip select pin
#define LED_PIN 12 ///< Status LED pin

Modem modem; ///< Global modem object

// Global objects
GSettings settings;                      ///< Global settings manager
WifiConnection wifiConnection(settings); ///< WiFi connection manager

// BLE objects
NimBLEServer *pServer = nullptr;                                ///< BLE server instance
NimBLECharacteristic *notifyCharacteristic = nullptr;           ///< BLE notification characteristic
ServerCallbacks serverCallbacks(wifiConnection.getStatus());    ///< BLE server event callbacks
CharacteristicCallbacks chrCallbacks(settings, wifiConnection); ///< BLE characteristic callbacks
HTTPServer *httpServer;                                         ///< HTTP server instance
/**
 * @brief Initialize and configure Bluetooth Low Energy (BLE) functionality
 *
 * Sets up the complete BLE stack for device configuration including:
 * - BLE device initialization with configurable name
 * - BLE server and service creation
 * - Characteristic setup for read/write operations and notifications
 * - Advertising configuration and startup
 *
 * BLE Service Structure:
 * - Service UUID: 9379d945-8ada-41b7-b028-64a8dda4b1f8
 * - Read/Write Char: c62b53d0-1848-424d-9d05-fd91e83f87a8 (WiFi credentials)
 * - Notify Char: 6cd49c0f-0c41-475b-afc5-5d504afca7dc (Status updates)
 *
 * The BLE interface allows remote configuration of WiFi credentials and
 * device settings via mobile apps or BLE clients.
 *
 * @note BLE remains enabled for configuration even when GSM is used for SMS.
 */
void bluetoothSetup()
{
  // Initialize BLE
  NimBLEDevice::init(settings.getDeviceName().c_str());

  // Create BLE server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  // Create BLE service
  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE characteristic
  // Write characteristic for incoming WiFi credentials
  NimBLECharacteristic *writeCharacteristic = pService->createCharacteristic(
      CHAR_READ_WRITE_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE);
  writeCharacteristic->setValue("Data");
  writeCharacteristic->setCallbacks(&chrCallbacks);

  // Notify characteristic for sending status/IP updates
  notifyCharacteristic = pService->createCharacteristic(
      CHAR_NOTIFY_UUID,
      NIMBLE_PROPERTY::NOTIFY);
  // notifyCharacteristic->addDescriptor(new BLE2902());
  /**
   *  2902 and 2904 descriptors are a special case, when createDescriptor is called with
   *  either of those uuid's it will create the associated class with the correct properties
   *  and sizes. However we must cast the returned reference to the correct type as the method
   *  only returns a pointer to the base NimBLEDescriptor class.
   */
  NimBLE2904 *pBeef2904 = notifyCharacteristic->create2904();
  pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
  // pBeef2904->setCallbacks(&dscCallbacks);
  notifyCharacteristic->setValue("Notify");

  chrCallbacks.setNotifyCharacteristic(notifyCharacteristic);

  // Start the service
  pService->start();

  // Start advertising
  NimBLEAdvertising *pAdvertising = serverCallbacks.getAdvertising();
  pAdvertising->setName(settings.getDeviceName().c_str());
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->enableScanResponse(true);
  // pAdvertising->setMinPreferred(0x06); // Helps with iOS issues
  // pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();
  Serial.println("BLE device is now advertising...");
}

/**
 * @brief Manage BLE advertising based on WiFi connection status
 *
 * Implements intelligent advertising management:
 * - Stops BLE advertising when WiFi is connected (after 5 minutes)
 * - Resumes BLE advertising when WiFi is disconnected (after 5 minutes)
 *
 * This approach conserves power and reduces BLE interference when
 * the device is successfully connected to WiFi and operating normally.
 * BLE remains available for reconfiguration when WiFi is unavailable.
 *
 * @note Uses a 5-minute timer to prevent rapid advertising state changes
 */
void bluetoothChangeStatus()
{
  static uint64_t last = millis();

  if (wifiConnection.getStatus().isWifiConnected())
  {
    // If 5 minutes have passed since last action
    if (millis() - last > 5 * MINUTE)
    {
      // wifiConnection.getStatus().setConnected(true);
      Serial.println("Stop advertising");
      pServer->getAdvertising()->stop();

      // Reset the timer here
      last = millis();
    }
  }
  else
  {
    // If 5 minutes have passed since last action
    if (millis() - last > 5 * MINUTE)
    {
      // wifiConnection.getStatus().setConnected(false);
      Serial.println("Start advertising");
      pServer->getAdvertising()->start();

      // Reset the timer here
      last = millis();
    }
  }
}

/**
 * @brief Arduino setup function - Initialize all system components
 *
 * Initialization sequence:
 * 1. Start serial communication for debugging (115200 baud)
 * 2. Load persistent settings from NVS storage
 * 3. Initialize BLE system for configuration interface
 * 4. Configure status LED
 * 5. Initialize GSM modem and establish network connection
 * 6. Attempt WiFi connection using stored credentials
 *
 * After setup completion, the device is ready to:
 * - Send SMS messages via GSM network
 * - Accept configuration changes via BLE
 * - Maintain WiFi connectivity if credentials are available
 *
 * @note This function runs once at device startup
 */
void setup()
{
  // Console
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }
  delay(300);

  settings.load();

  bluetoothSetup();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println(F("\n=== T-SIM7000G SMS Sender ==="));

  modem.initModem();

  connect_t result = wifiConnection.connect();
  if (result.isConnected)
  {
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(*result.ip);
  }
  else
  {
    Serial.println("Failed to connect to WiFi!");
  }

  httpServer = new HTTPServer(
      settings,
      wifiConnection,
      // Use lambdas to wrap member functions
      [&](const String &number, const String &message)
      { return modem.sendSMS(number, message); },
      [&]()
      { return modem.checkModemRegistered(); },
      80,
      LED_PIN);
}

/**
 * @brief Arduino main loop function - Handle ongoing operations
 *
 * Main execution loop that manages:
 * 1. Bluetooth status and advertising based on WiFi connectivity
 * 2. HTTP server client request processing
 * 3. Brief CPU yield to allow other tasks to execute
 *
 * The loop operates continuously to:
 * - Monitor and adjust BLE advertising based on WiFi status
 * - Process incoming HTTP requests for SMS sending
 * - Maintain responsive system operation
 *
 * @note Uses a minimal 2ms delay to allow ESP32 task scheduler
 *       to handle other system operations efficiently
 * @note SMS sending is handled via HTTP API requests rather
 *       than continuous automatic sending.
 */
void loop()
{
  bluetoothChangeStatus();
  httpServer->handleClient();
  delay(2); // allow the cpu to switch to other tasks
}