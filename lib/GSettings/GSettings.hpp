#pragma once

#include <Preferences.h>
#include <ArduinoJson.h>
#include "ProbeRegistry.hpp"

/**
 * @brief Global settings manager class
 *
 * Manages persistent storage of device configuration using ESP32 Preferences.
 * Handles WiFi credentials, device name, and provides JSON serialization
 * for configuration exchange via BLE or HTTP interfaces.
 *
 * Features:
 * - Persistent storage using ESP32 NVS (Non-Volatile Storage)
 * - WiFi credential management with security considerations
 * - Device name configuration for BLE advertising
 * - JSON serialization for API/BLE communication
 * - Automatic loading of stored settings on initialization
 * - Thread-safe settings persistence
 *
 * Security Note: Passwords are partially masked in JSON output to prevent
 * accidental exposure in logs or API responses.
 */
class GSettings
{
public:
    /**
     * @brief Construct a new GSettings object
     *
     * Initializes settings with default values. Call load() to read
     * previously stored settings from persistent storage.
     */
    GSettings();

    /**
     * @brief Get the device name used for BLE advertising
     *
     * @return String Current device name
     */
    String getDeviceName();

    /**
     * @brief Set the device name for BLE advertising
     *
     * Updates the device name used for BLE advertising and identification.
     * Call save() to persist the change to non-volatile storage.
     *
     * @param deviceName New device name (should be unique and descriptive)
     */
    void setDeviceName(String deviceName);

    /**
     * @brief Get the WiFi network SSID
     *
     * @return String Current WiFi network name
     */
    String getSsid();

    /**
     * @brief Set the WiFi network SSID
     *
     * Updates the WiFi network name for connection attempts.
     * Call save() to persist the change to non-volatile storage.
     *
     * @param ssid WiFi network name to connect to
     */
    void setSsid(String ssid);

    /**
     * @brief Get the WiFi network password
     *
     * @return String Current WiFi password (plaintext)
     */
    String getPassword();

    /**
     * @brief Set the WiFi network password
     *
     * Updates the WiFi password for network authentication.
     * Call save() to persist the change to non-volatile storage.
     *
     * @param password WiFi network password (stored as plaintext)
     */
    void setPassword(String password);

    /**
     * @brief Load settings from persistent storage
     *
     * Reads all stored settings from ESP32 NVS (Non-Volatile Storage).
     * If no stored values exist, default values are used.
     * This method should be called once during device initialization.
     */
    void load();

    /**
     * @brief Save current settings to persistent storage
     *
     * Writes all current settings to ESP32 NVS for persistence across
     * device restarts and power cycles. This operation is atomic and
     * thread-safe.
     */
    void save();

    /**
     * @brief Serialize settings to JSON object
     *
     * Creates a JSON representation of current settings suitable for
     * API responses or BLE communication. For security, passwords are
     * partially masked (first 4 characters + "****").
     *
     * JSON Format:
     * {
     *   "deviceName": "ESP32-Device",
     *   "ssid": "WiFi-Network",
     *   "password": "pass****"
     * }
     *
     * @param root JSON object to populate with settings data
     */
    void toJson(JsonObject &root);

private:
    String deviceName;
    String ssid;
    String password;
    Preferences preferences;
};