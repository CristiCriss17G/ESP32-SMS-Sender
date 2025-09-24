#pragma once

#include <Preferences.h>
#include <ArduinoJson.h>

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