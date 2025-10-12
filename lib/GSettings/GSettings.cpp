/**
 * @file GSettings.cpp
 * @brief Implementation of global settings management
 */

#include "GSettings.hpp"

/**
 * @brief Static initialization of program start timestamp
 *
 * Captures the millis() value when the program begins, used by getUptime()
 * to calculate runtime duration across all GSettings instances.
 */
uint64_t GSettings::startTime = millis();

/**
 * @brief Construct a new GSettings object with default values
 *
 * Initializes all settings with sensible defaults. The device name
 * defaults to "ESP32-BLE-Example", while WiFi credentials start empty.
 */
GSettings::GSettings() : deviceName("ESP32-BLE-Example"), ssid(""), password("")
{
    ProbeRegistry::instance().registerProbe("settings", [this](JsonObject &dst)
                                            { this->toJson(dst); });
}

/**
 * @brief Get the current device name
 *
 * @return String The device name used for BLE advertising
 */
String GSettings::getDeviceName()
{
    return deviceName;
}

/**
 * @brief Set a new device name
 *
 * @param deviceName New device name for BLE advertising and identification
 */
void GSettings::setDeviceName(String deviceName)
{
    this->deviceName = deviceName;
}

/**
 * @brief Get the current WiFi SSID
 *
 * @return String The WiFi network name for connection attempts
 */
String GSettings::getSsid()
{
    return ssid;
}

/**
 * @brief Set a new WiFi SSID
 *
 * @param ssid WiFi network name to use for connection attempts
 */
void GSettings::setSsid(String ssid)
{
    this->ssid = ssid;
}

/**
 * @brief Get the current WiFi password
 *
 * @return String The WiFi password for network authentication
 */
String GSettings::getPassword()
{
    return password;
}

/**
 * @brief Set a new WiFi password
 *
 * @param password WiFi password for network authentication (stored as plaintext)
 */
void GSettings::setPassword(String password)
{
    this->password = password;
}

/**
 * @brief Load settings from ESP32 persistent storage
 *
 * Reads stored configuration from ESP32 NVS (Non-Volatile Storage).
 * If no stored values exist, the current default values are retained.
 * Uses the "global-settings" namespace for storage organization.
 */
void GSettings::load()
{
    preferences.begin("global-settings", false);
    deviceName = preferences.getString("deviceName", deviceName);
    ssid = preferences.getString("ssid", ssid);
    password = preferences.getString("password", password);
    preferences.end();
}

/**
 * @brief Save current settings to ESP32 persistent storage
 *
 * Writes all current configuration values to ESP32 NVS for persistence
 * across device restarts. Uses the "global-settings" namespace and
 * ensures atomic write operations.
 */
void GSettings::save()
{
    preferences.begin("global-settings", false);
    preferences.putString("deviceName", deviceName);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
}

/**
 * @brief Convert settings to JSON format with security considerations
 *
 * Serializes current settings to a JSON object suitable for API responses
 * or BLE communication. For security, passwords are partially masked by
 * showing only the first 4 characters followed by "****".
 *
 * @param root JSON object reference to populate with settings data
 */
void GSettings::toJson(JsonObject &root)
{
    root["deviceName"] = deviceName;
    root["ssid"] = ssid;
    root["password"] = password.isEmpty() ? "" : password.substring(0, 4) + "****";
}

/**
 * @brief Get system uptime in milliseconds
 *
 * @return uint64_t
 */
uint64_t GSettings::getUptime()
{
    return (millis() - startTime);
}