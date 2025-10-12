/**
 * @file BTLe.cpp
 * @brief Implementation of Bluetooth Low Energy functionality for ESP32 SMS Sender
 */

#include "BTLe.hpp"

/**
 * @brief Construct a new ServerCallbacks object
 *
 * Initializes the server callbacks with WiFi status reference and
 * obtains the BLE advertising interface.
 *
 * @param wifiStatus Reference to WiFi status tracker
 */
ServerCallbacks::ServerCallbacks(WifiStatus &wifiStatus, GSettings &settings) : wifiStatus(wifiStatus), settings(settings)
{
    pAdvertising = NimBLEDevice::getAdvertising();
}

/**
 * @brief Handle BLE client connection event
 *
 * Sets the device connected flag and logs connection information
 * including the client's MAC address.
 *
 * @param pServer Pointer to the BLE server instance
 * @param connInfo Connection information containing client details
 */
void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    deviceConnected = true;
    Serial.println(F("Client connected"));
    Serial.print(connInfo.getAddress().toString().c_str());
    Serial.printf("Connected. Encrypted=%d, Bonded=%d, Authenticated=%d, MTU=%d\n",
                  connInfo.isEncrypted(),
                  connInfo.isBonded(),
                  connInfo.isAuthenticated(),
                  connInfo.getMTU());
}

/**
 * @brief Handle BLE client disconnection event
 *
 * Updates the device connected flag and resumes BLE advertising
 * if WiFi is not connected, allowing for device reconfiguration.
 *
 * @param pServer Pointer to the BLE server instance
 * @param connInfo Connection information containing client details
 * @param reason Reason code for disconnection
 */
void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    deviceConnected = false;
    Serial.println(F("Client disconnected"));
    if (settings.getUptime() < 5 * MINUTE)
    {
        // Resume advertising since WiFi is not connected
        pAdvertising->start();
        Serial.println(F("Advertising resumed"));
    }
}

/**
 * @brief Handle BLE authentication completion events
 *
 * @param connInfo Connection information structure
 */
void ServerCallbacks::onAuthenticationComplete(NimBLEConnInfo &connInfo)
{
    if (!connInfo.isEncrypted())
    {
        Serial.println(F("Auth failed or not encrypted; disconnecting."));
        NimBLEDevice::getServer()->disconnect(connInfo);
        return;
    }
    Serial.printf("Pairing OK. Encrypted=%d Bonded=%d\n",
                  connInfo.isEncrypted(),
                  connInfo.isBonded());
}

/**
 * @brief Get the BLE advertising interface
 *
 * @return NimBLEAdvertising* Pointer to the advertising object
 */
NimBLEAdvertising *ServerCallbacks::getAdvertising()
{
    return pAdvertising;
}

/**
 * @brief Check current BLE connection status
 *
 * @return true if a BLE client is currently connected
 * @return false if no client is connected
 */
bool ServerCallbacks::isDeviceConnected()
{
    return deviceConnected;
}

/**
 * @brief Construct a new CharacteristicCallbacks object
 *
 * Initializes callback handler with references to settings and WiFi connection managers.
 *
 * @param settings Reference to global settings for credential management
 * @param wifiConnection Reference to WiFi connection manager
 */
CharacteristicCallbacks::CharacteristicCallbacks(GSettings &settings, WifiConnection &wifiConnection) : settings(settings), wifiConnection(wifiConnection)
{
}

/**
 * @brief Handle BLE characteristic read requests
 *
 * Responds to client requests for current device status by creating a JSON
 * response containing WiFi connection status and device settings.
 * The response includes sanitized settings (passwords are partially masked).
 *
 * JSON Response Format:
 * {
 *   "wifiStatus": {
 *     "connected": true/false,
 *     "ipAddress": "192.168.1.100"
 *   },
 *   "settings": {
 *     "deviceName": "ESP32-Device",
 *     "ssid": "WiFi-Network",
 *     "password": "pass****"
 *   }
 * }
 *
 * @param pCharacteristic Pointer to the characteristic being read
 * @param connInfo Connection information structure
 */
void CharacteristicCallbacks::onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    if (!connInfo.isEncrypted())
    {
        // Should not happen if permissions set correctly; reject anyway
        Serial.println(F("Read rejected: not encrypted"));
        return;
    }
    Serial.println(F("Read request received"));
    // Allocate the JSON document
    String output = ProbeRegistry::instance().collectAllAsJson();
    Serial.print(F("Sending response: "));
    Serial.println(output);
    pCharacteristic->setValue(output.c_str());
}

/**
 * @brief Handle BLE characteristic write requests for device configuration
 *
 * Processes JSON configuration data sent by BLE clients to update device settings.
 * Supports multiple configuration operations that can be combined in a single request:
 *
 * Supported JSON Fields:
 * - "deviceName": Updates BLE device name
 * - "ssid": WiFi network name
 * - "password": WiFi network password
 * - "restart": Boolean flag to restart ESP32 after applying changes
 *
 * Operation Flow:
 * 1. Validates and parses incoming JSON data
 * 2. Updates device settings if valid data is provided
 * 3. Saves settings to persistent storage if changes were made
 * 4. Attempts WiFi connection if new credentials were provided
 * 5. Sends status notifications to connected clients
 * 6. Restarts device if requested
 *
 * Status Notification Codes:
 * - "S:WC,NR,IP:<address>" - WiFi connected successfully
 * - "S:WF,NR" - WiFi connection failed
 * - "S:SI,NR" - Server info updated (restart required)
 *
 * @param pCharacteristic Pointer to the characteristic being written
 * @param connInfo Connection information structure
 */
void CharacteristicCallbacks::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    if (!connInfo.isEncrypted())
    {
        // Should not happen if permissions set correctly; reject anyway
        Serial.println(F("Write rejected: not encrypted"));
        return;
    }

    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0)
    {
        Serial.println(F("Received WiFi credentials!"));
        // Serial.printf("Received: %s\n", rxValue.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, rxValue.c_str());
        if (error)
        {
            Serial.print(F("Failed to parse JSON: "));
            Serial.println(error.c_str());
            return;
        }
        bool toSavePreferences = false;
        bool tryWifiConnect = false;
        bool newServerInfo = false;
        if (doc["deviceName"].is<const char *>())
        {
            settings.setDeviceName(doc["deviceName"].as<String>());
            toSavePreferences = true;
            Serial.printf("Device Name: %s\n", settings.getDeviceName().c_str());
        }
        if (doc["ssid"].is<const char *>() && doc["password"].is<const char *>())
        {
            settings.setSsid(doc["ssid"].as<String>());
            settings.setPassword(doc["password"].as<String>());
            toSavePreferences = true;
            tryWifiConnect = true;
            Serial.printf("SSID: %s\n", settings.getSsid().c_str());
        }
        if (toSavePreferences)
        {
            settings.save();
        }
        if (tryWifiConnect)
        {
            if (wifiConnection.getStatus().isWifiConnected())
            {
                Serial.println(F("Disconnecting from WiFi..."));
                wifiConnection.disconnect();
            }
            Serial.println(F("Connecting to WiFi..."));
            connect_t result = wifiConnection.connect();
            if (result.isConnected)
            {
                Serial.println(F("Connected to WiFi!"));
                wifiConnection.getStatus().setWifiConnected(true);
                wifiConnection.getStatus().setIpAddress(result.ip);
                if (notifyCharacteristic != nullptr)
                {
                    notifyCharacteristic->notify("S:WC,NR,IP:" + wifiConnection.getStatus().getIpAddress());
                }
            }
            else
            {
                Serial.println(F("Failed to connect to WiFi!"));
                wifiConnection.getStatus().setWifiConnected(false);
                if (notifyCharacteristic != nullptr)
                {
                    notifyCharacteristic->notify(String("S:WF,NR"));
                }
            }
        }
        if (newServerInfo)
        {
            Serial.println(F("New server info received! Needs restart to apply changes."));
            if (notifyCharacteristic != nullptr)
            {
                notifyCharacteristic->notify(String("S:SI,NR"));
            }
        }
        if (doc["restart"].is<bool>() && doc["restart"].as<bool>())
        {
            Serial.println(F("Restarting esp32 to apply new settings..."));
            ESP.restart();
        }
    }
}
