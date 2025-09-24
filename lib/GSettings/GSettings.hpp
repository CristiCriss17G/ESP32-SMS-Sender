#pragma once

#include <Preferences.h>
#include <ArduinoJson.h>

/**
 * @brief Class to store connection status and IP address
 *
 */
class WifiStatus
{
public:
    /**
     * @brief Construct a new Wifi Status object
     *
     */
    WifiStatus();

    /**
     * @brief Get the Connected object
     *
     * @return true
     * @return false
     */
    bool getConnected();

    /**
     * @brief Set the Connected object
     *
     * @param connected
     */
    void setConnected(bool connected);

    /**
     * @brief Get the Ip Address object
     *
     * @return String
     */
    String getIpAddress();

    /**
     * @brief Set the Ip Address object
     *
     * @param ipAddress
     */
    void setIpAddress(String ipAddress);

    /**
     * @brief Serialize the object to a JSON string
     *
     * @param root
     */
    void toJson(JsonObject &root);

    /**
     * @brief Get the String object
     *
     * @return String
     */
    String toString();

private:
    bool connected = false;
    String ipAddress = "";
};

/**
 * @brief Class to store settings
 *
 */
class Settings
{
public:
    /**
     * @brief Construct a new Settings object
     *
     */
    Settings();

    /**
     * @brief Get the Device Name object
     *
     * @return String
     */
    String getDeviceName();

    /**
     * @brief Set the Device Name object
     *
     * @param deviceName
     */
    void setDeviceName(String deviceName);

    /**
     * @brief Get the Ssid object
     *
     * @return String
     */
    String getSsid();

    /**
     * @brief Set the Ssid object
     *
     * @param ssid
     */
    void setSsid(String ssid);

    /**
     * @brief Get the Password object
     *
     * @return String
     */
    String getPassword();

    /**
     * @brief Set the Password object
     *
     * @param password
     */

    void setPassword(String password);

    /**
     * @brief Load settings from preferences
     *
     */
    void load();

    /**
     * @brief Save settings to preferences
     *
     */
    void save();

    /**
     * @brief Serialize the object to a JSON string
     *
     * @param root
     */
    void toJson(JsonObject &root);

private:
    String deviceName;
    String ssid;
    String password;
    Preferences preferences;
};