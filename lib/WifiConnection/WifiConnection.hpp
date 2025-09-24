#pragma once

#include <WiFi.h>
#include "GSettings.hpp"

/**
 * @brief Structure to hold connection information
 *
 */
struct connect_t
{
    bool isConnected;
    IPAddress *ip;
};

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
     * @brief Get the connection status
     *
     * @return true
     * @return false
     */
    bool isWifiConnected();

    /**
     * @brief Set the Wifi Connection status
     *
     * @param connected
     */
    void setWifiConnected(bool connected);

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
     * @brief Set the Ip Address object
     *
     * @param ipAddress
     */
    void setIpAddress(IPAddress ipAddress);

    /**
     * @brief Update the connection information
     *
     * @param connection
     */
    void updateConnection(connect_t connection);

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
    connect_t connection;
};

class WifiConnection
{
public:
    WifiConnection(Settings &settings);
    ~WifiConnection();

    connect_t connect();
    bool disconnect();
    WifiStatus &getStatus();

private:
    Settings &settings;
    WifiStatus wifiStatus;
    bool isConnectionTrying = false;
};