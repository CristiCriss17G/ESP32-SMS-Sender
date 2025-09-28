#pragma once

#include <WiFi.h>
#include "GSettings.hpp"

/**
 * @brief Structure to hold WiFi connection information
 * 
 * Contains connection status and IP address information.
 * Includes automatic cleanup of IP address memory via destructor.
 */
struct connect_t
{
    bool isConnected;       ///< WiFi connection status flag
    IPAddress *ip;         ///< Pointer to IP address (nullptr if not connected)
    
    /**
     * @brief Destructor that cleans up IP address memory
     * 
     * Ensures proper deallocation of IPAddress pointer when struct goes out of scope.
     */
    ~connect_t()
    {
        if (ip != nullptr)
            delete ip;
        ip = nullptr;
    }
};

/**
 * @brief WiFi status manager class
 * 
 * Manages WiFi connection status information including connection state
 * and IP address. Provides methods for status updates, JSON serialization,
 * and string representation of connection information.
 * 
 * Features:
 * - Connection status tracking
 * - IP address management with automatic memory cleanup
 * - JSON serialization for API responses
 * - String representation for debugging/logging
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
     * @brief Destroy the Wifi Status object
     *
     */
    ~WifiStatus();

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
    connect_t connection;    ///< Internal connection information storage
};

/**
 * @brief WiFi connection management class
 * 
 * Handles WiFi network connection, disconnection, and status management
 * for ESP32 devices. Integrates with global settings to retrieve
 * network credentials and provides connection status tracking.
 * 
 * Features:
 * - Automatic WiFi connection with credentials from settings
 * - Connection status monitoring
 * - Hostname configuration
 * - Auto-reconnect functionality
 * - Connection timeout handling
 * - Clean disconnection and resource cleanup
 * 
 * @note Connection attempts are protected against concurrent execution
 */
class WifiConnection
{
public:
    /**
     * @brief Construct a new Wifi Connection object
     *
     * @param settings
     */
    WifiConnection(GSettings &settings);
    ~WifiConnection();

    /**
     * @brief Connect to the WiFi network using stored settings
     *
     * @return connect_t
     */
    connect_t connect();

    /**
     * @brief Disconnect from the WiFi network
     *
     * @return true
     * @return false
     */
    bool disconnect();

    /**
     * @brief Get the Status object
     *
     * @return WifiStatus&
     */
    WifiStatus &getStatus();

private:
    GSettings &settings;           ///< Reference to global settings for WiFi credentials
    WifiStatus wifiStatus;         ///< WiFi status tracking object
    bool isConnectionTrying = false; ///< Flag to prevent concurrent connection attempts
};