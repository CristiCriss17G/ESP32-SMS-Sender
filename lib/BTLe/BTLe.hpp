/**
 * @file BTLe.hpp
 * @brief Bluetooth Low Energy (BLE) interface for ESP32 SMS Sender configuration
 *
 * This module provides BLE functionality to configure WiFi credentials and
 * device settings remotely via a mobile app or BLE client. It uses NimBLE
 * stack for efficient BLE operations on ESP32.
 */

#ifndef BTLE_H
#define BTLE_H

#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "GSettings.hpp"
#include "WifiConnection.hpp"

// BLE Configuration Parameters
#define BLE_DEVICE_NAME "ESP32-BLE-Example"                         ///< Default BLE device name for advertising
#define SERVICE_UUID "9379d945-8ada-41b7-b028-64a8dda4b1f8"         ///< Primary BLE service UUID
#define CHAR_READ_WRITE_UUID "c62b53d0-1848-424d-9d05-fd91e83f87a8" ///< Characteristic UUID for WiFi credential exchange
#define CHAR_NOTIFY_UUID "6cd49c0f-0c41-475b-afc5-5d504afca7dc"     ///< Characteristic UUID for status notifications

/**
 * @brief BLE Server callback handler class
 *
 * Handles BLE server events such as client connections and disconnections.
 * Manages advertising behavior based on WiFi connection status and provides
 * access to the advertising interface.
 *
 * Features:
 * - Client connection/disconnection event handling
 * - Automatic advertising management based on WiFi status
 * - Connection state tracking
 * - Integration with WiFi status monitoring
 */
class ServerCallbacks : public NimBLEServerCallbacks
{
public:
    /**
     * @brief Construct a new ServerCallbacks object
     *
     * @param wifiStatus Reference to WiFi status tracker for advertising decisions
     */
    ServerCallbacks(WifiStatus &wifiStatus);

    /**
     * @brief Handle client connection events
     *
     * Called when a BLE client connects to the server.
     * Updates connection state and logs connection information.
     *
     * @param pServer Pointer to the BLE server instance
     * @param connInfo Connection information structure
     */
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;

    /**
     * @brief Handle client disconnection events
     *
     * Called when a BLE client disconnects from the server.
     * Resumes advertising if WiFi is not connected to allow reconfiguration.
     *
     * @param pServer Pointer to the BLE server instance
     * @param connInfo Connection information structure
     * @param reason Disconnection reason code
     */
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;

    /**
     * @brief Get the advertising interface
     *
     * @return NimBLEAdvertising* Pointer to the BLE advertising object
     */
    NimBLEAdvertising *getAdvertising();

    /**
     * @brief Check if a BLE client is currently connected
     *
     * @return true if a device is connected
     * @return false if no device is connected
     */
    bool isDeviceConnected();

protected:
    bool deviceConnected = false;    ///< Flag indicating if a BLE client is connected
    WifiStatus &wifiStatus;          ///< Reference to WiFi status for advertising logic
    NimBLEAdvertising *pAdvertising; ///< Pointer to BLE advertising interface
};

/**
 * @brief BLE Characteristic callback handler class
 *
 * Handles read and write operations on BLE characteristics used for
 * device configuration. Processes WiFi credential updates, device settings,
 * and provides status information to connected clients.
 *
 * Features:
 * - WiFi credential reception and validation
 * - Device name configuration
 * - Automatic WiFi connection attempts
 * - Status notifications to connected clients
 * - Settings persistence using ESP32 Preferences
 * - Remote device restart capability
 */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
public:
    /**
     * @brief Construct a new CharacteristicCallbacks object
     *
     * @param settings Reference to global settings for credential storage
     * @param wifiConnection Reference to WiFi connection manager
     */
    CharacteristicCallbacks(GSettings &settings, WifiConnection &wifiConnection);

    /**
     * @brief Set the notification characteristic reference
     *
     * Late-binding setter to establish reference to the notification characteristic
     * used for sending status updates to connected clients.
     *
     * @param c Pointer to the notification characteristic
     */
    void setNotifyCharacteristic(NimBLECharacteristic *c) { notifyCharacteristic = c; }

    /**
     * @brief Handle characteristic read requests
     *
     * Responds to client read requests by providing current device status
     * including WiFi connection information and settings (sanitized).
     * Returns JSON formatted response with current state.
     *
     * @param pCharacteristic Pointer to the characteristic being read
     * @param connInfo Connection information structure
     */
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

    /**
     * @brief Handle characteristic write requests
     *
     * Processes incoming configuration data from BLE clients.
     * Supports the following operations:
     * - WiFi credential updates (SSID and password)
     * - Device name changes
     * - Automatic WiFi connection attempts
     * - Device restart commands
     *
     * Expected JSON format:
     * {
     *   "deviceName": "new-device-name",
     *   "ssid": "wifi-network-name",
     *   "password": "wifi-password",
     *   "restart": true
     * }
     *
     * @param pCharacteristic Pointer to the characteristic being written
     * @param connInfo Connection information structure
     */
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

protected:
    Preferences preferences;                    ///< ESP32 preferences for persistent storage
    GSettings &settings;                        ///< Reference to global settings manager
    WifiConnection &wifiConnection;             ///< Reference to WiFi connection manager
    NimBLECharacteristic *notifyCharacteristic; ///< Pointer to notification characteristic
};

#endif