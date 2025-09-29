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

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_DEBUG Serial   // comment this to reduce logs
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#define GSM_NL "\r\n"

#define SerialAT Serial1
// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

#define DUMP_AT_COMMANDS

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "live.vodafone.com"; // SET TO YOUR APN
const char gprsUser[] = "live";
const char gprsPass[] = "";

#include <TinyGsmClient.h>
#include <Ticker.h>
#include "GSettings.hpp"
#include "WifiConnection.hpp"
#include "BTLe.hpp"
#include "HTTPServer.hpp"

#define SECOND 1000l  // 1 second
#define MINUTE 60000l // 1 minute

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// ===== Pin definitions (LilyGO T-SIM7000G defaults) =====
#define MODEM_TX 27       ///< GSM modem UART transmit pin
#define MODEM_RX 26       ///< GSM modem UART receive pin
#define MODEM_PWRKEY 4    ///< GSM modem power key control pin
#define MODEM_POWER_ON 23 ///< GSM modem power enable pin
#define MODEM_RST 5       ///< GSM modem reset pin
#define MODEM_DTR 32      ///< GSM modem DTR (Data Terminal Ready) pin
#define MODEM_RI 33       ///< GSM modem RI (Ring Indicator) pin

#define SD_MISO 2  ///< SD card SPI MISO pin
#define SD_MOSI 15 ///< SD card SPI MOSI pin
#define SD_SCLK 14 ///< SD card SPI clock pin
#define SD_CS 13   ///< SD card SPI chip select pin
#define LED_PIN 12 ///< Status LED pin

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
 * @brief Power on the GSM modem
 *
 * Activates the SIM7000G modem by toggling the power key pin.
 * Uses the standard power-on sequence: LOW pulse for 1 second,
 * then HIGH to complete the power-on process.
 */
void modemPowerOn()
{
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, HIGH);
}

/**
 * @brief Power off the GSM modem
 *
 * Safely shuts down the SIM7000G modem by holding the power key
 * LOW for 1.5 seconds, then releasing it to HIGH.
 */
void modemPowerOff()
{
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1500);
  digitalWrite(MODEM_PWRKEY, HIGH);
}

/**
 * @brief Restart the GSM modem
 *
 * Performs a complete modem restart by powering off, waiting,
 * then powering back on. Useful for recovering from error states.
 */
void modemRestart()
{
  modemPowerOff();
  delay(1000);
  modemPowerOn();
}

/**
 * @brief Initialize and configure the GSM modem for SMS functionality
 *
 * Comprehensive modem initialization process:
 * 1. Configure UART communication (115200 baud)
 * 2. Set DTR pin to keep modem awake
 * 3. Initialize modem communication
 * 4. Display modem information and capabilities
 * 5. Handle SIM card PIN if required
 * 6. Test multiple network modes for best connectivity
 * 7. Wait for network registration and signal quality
 * 8. Display final connection status and network information
 *
 * Network Modes Tested (in order):
 * - Mode 2: Automatic selection
 * - Mode 13: GSM only
 * - Mode 38: LTE only
 * - Mode 51: GSM and LTE only
 *
 * @note LED flashes during network registration attempts
 * @note Will retry modem restart if initial initialization fails
 */
void initModem()
{
  String res;

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX, false);
  delay(600);

  pinMode(MODEM_DTR, OUTPUT);
  digitalWrite(MODEM_DTR, LOW); // keep awake

  Serial.println(F("[MODEM] Initializing..."));
  if (!modem.init())
  {
    Serial.println(F("[MODEM] init failed, trying restart()..."));
    modemRestart();
    delay(2000);
    return;
  }

  Serial.println("========SIMCOMATI======");
  modem.sendAT("+SIMCOMATI");
  modem.waitResponse(1000L, res);
  res.replace(GSM_NL "OK" GSM_NL, "");
  Serial.println(res);
  res = "";
  Serial.println("=======================");

  Serial.println("=====Preferred mode selection=====");
  modem.sendAT("+CNMP?");
  if (modem.waitResponse(1000L, res) == 1)
  {
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
  }
  res = "";
  Serial.println("=======================");

  Serial.println("=====Preferred selection between CAT-M and NB-IoT=====");
  modem.sendAT("+CMNB?");
  if (modem.waitResponse(1000L, res) == 1)
  {
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
  }
  res = "";
  Serial.println("=======================");

  String name = modem.getModemName();
  Serial.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  Serial.println("Modem Info: " + modemInfo);

  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3)
  {
    modem.simUnlock(GSM_PIN);
  }

  for (int i = 0; i < 4; i++)
  {
    uint8_t network[] = {
        2,  /*Automatic*/
        13, /*GSM only*/
        38, /*LTE only*/
        51  /*GSM and LTE only*/
    };
    Serial.printf("Try %d method\n", network[i]);
    modem.setNetworkMode(network[i]);
    delay(3000);
    bool isConnected = false;
    int tryCount = 60;
    while (tryCount--)
    {
      int16_t signal = modem.getSignalQuality();
      Serial.print("Signal: ");
      Serial.print(signal);
      Serial.print(" ");
      Serial.print("isNetworkConnected: ");
      isConnected = modem.isNetworkConnected();
      Serial.println(isConnected ? "CONNECT" : "NO CONNECT");
      if (isConnected)
      {
        break;
      }
      delay(1000);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    if (isConnected)
    {
      break;
    }
  }
  digitalWrite(LED_PIN, HIGH);

  Serial.println();
  Serial.println("Device is connected .");
  Serial.println();

  Serial.println("=====Inquiring UE system information=====");
  modem.sendAT("+CPSI?");
  if (modem.waitResponse(1000L, res) == 1)
  {
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
  }
}

/**
 * @brief Check if the GSM modem is registered on the cellular network
 *
 * Performs network registration verification using AT+CREG? command
 * to ensure the modem is ready for SMS operations.
 *
 * Registration Status Codes:
 * - 1: Registered (home network)
 * - 5: Registered (roaming)
 * - Other values: Not registered
 *
 * If not registered, attempts to wait for network registration
 * for up to 60 seconds as a recovery mechanism.
 *
 * @return true if modem is registered and ready for SMS
 * @return false if modem is not registered on any network
 */
bool checkModemRegistered()
{
  // Ensure modem is registered (cheap quick check)
  int reg = -1;
  modem.sendAT("+CREG?");
  if (modem.waitResponse(2000L, "+CREG:") == 1)
  {
    reg = modem.stream.readStringUntil('\n').toInt();
  }
  // Best effort: if not registered, try wait again (non-fatal)
  if (!modem.isNetworkConnected())
  {
    return modem.waitForNetwork(60000L);
  }
  Serial.println("Network registered, status: " + String(reg));
  return true;
}

/**
 * @brief Send an SMS message using the GSM modem
 *
 * @param to
 * @param text
 * @return true
 * @return false
 */
bool sendSMS(const String &to, const String &text)
{
  Serial.printf("[SMS] To: %s  Len: %u\n", to.c_str(), (unsigned)text.length());
  return modem.sendSMS(to.c_str(), text.c_str());
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

  initModem();

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

  httpServer = new HTTPServer(settings, wifiConnection, sendSMS, checkModemRegistered, 80, LED_PIN);
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
 * @note SMS sending is now handled via HTTP API requests rather
 *       than continuous automatic sending
 */
void loop()
{
  bluetoothChangeStatus();
  httpServer->handleClient();
  delay(2); // allow the cpu to switch to other tasks
}