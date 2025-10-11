#pragma once

#include <WiFi.h>
#include "GSettings.hpp"
#include "ProbeRegistry.hpp"

/**
 * @brief Structure to hold WiFi connection information
 *
 * Contains connection status and IP address information.
 * Includes automatic cleanup of IP address memory via destructor.
 */
struct connect_t
{
    bool isConnected; ///< WiFi connection status flag
    IPAddress ip;     ///< IP address (default constructed if not connected)

    /**
     * @brief Check if the IP address is null (unassigned)
     *
     * @return true if IP address is null
     * @return false if IP address is assigned
     */
    bool isIPNULL() const
    {
        return ip == NULL_IP;
    }

private:
    static IPAddress NULL_IP;
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
     * @brief Construct a new WifiStatus object
     *
     * Initializes the WiFi status with disconnected state and null IP address.
     * Sets up the internal connection structure with safe default values.
     */
    WifiStatus();

    /**
     * @brief Destroy the WifiStatus object
     *
     * Ensures proper cleanup of allocated IP address memory.
     * Automatically called when object goes out of scope.
     */
    ~WifiStatus();

    /**
     * @brief Get the current WiFi connection status
     *
     * @retval true WiFi is connected and operational
     * @retval false WiFi is disconnected or not available
     */
    bool isWifiConnected();

    /**
     * @brief Set the WiFi connection status
     *
     * Updates the internal connection state flag. This method is typically
     * called by the WifiConnection class when connection state changes.
     *
     * @param connected New connection state (true=connected, false=disconnected)
     */
    void setWifiConnected(bool connected);

    /**
     * @brief Get the current IP address as a string
     *
     * @return String representation of the assigned IP address
     * @retval "0.0.0.0" if no IP address is assigned or WiFi is disconnected
     * @retval Valid IP address string (e.g., "192.168.1.100") when connected
     */
    String getIpAddress();

    /**
     * @brief Set the IP address from string representation
     *
     * Parses and stores an IP address from string format. Creates a new
     * IPAddress object and deallocates any previously stored address.
     *
     * @param ipAddress String representation of IP address (e.g., "192.168.1.100")
     * @note Input must be a valid IPv4 address format
     */
    void setIpAddress(String ipAddress);

    /**
     * @brief Set the IP address from IPAddress object
     *
     * Creates an internal copy of the provided IPAddress object.
     * Automatically manages memory allocation and deallocation.
     *
     * @param ipAddress IPAddress object to copy and store
     */
    void setIpAddress(IPAddress ipAddress);

    /**
     * @brief Update the complete connection information
     *
     * Replaces all current connection data with new information from
     * the provided connection structure. Used when connection state changes.
     *
     * @param connection New connection information (status + IP address)
     */
    void updateConnection(connect_t connection);

    /**
     * @brief Serialize WiFi status to JSON format
     *
     * Populates a JSON object with current connection status and IP address.
     * Used for API responses, BLE communication, and status reporting.
     *
     * Output format:
     * {
     *   "connected": true/false,
     *   "ipAddress": "192.168.1.100" or "0.0.0.0"
     * }
     *
     * @param root JSON object reference to populate with status data
     */
    void toJson(JsonObject &root);

    /**
     * @brief Convert WiFi status to human-readable string
     *
     * Creates a formatted string containing connection status and IP address
     * information suitable for debugging, logging, or user display.
     *
     * @return String in format "Connected: true/false, IP Address: x.x.x.x"
     */
    String toString();

private:
    connect_t connection; ///< Internal connection information storage
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
     * @brief Construct a new WifiConnection object
     *
     * Initializes the WiFi connection manager with a reference to global settings
     * for accessing stored WiFi credentials (SSID and password). Sets up internal
     * state tracking and prepares for connection attempts.
     *
     * @param settings Reference to global settings containing WiFi credentials
     */
    WifiConnection(GSettings &settings);

    /**
     * @brief Destroy the WifiConnection object
     *
     * Ensures clean disconnection from WiFi before object destruction.
     * Prevents resource leaks and ensures proper WiFi stack cleanup.
     */
    ~WifiConnection();

    /**
     * @brief Connect to WiFi network using stored credentials
     *
     * Attempts to establish a WiFi connection using SSID and password from
     * global settings. The connection process includes:
     * - Setting WiFi to station mode
     * - Configuring device hostname
     * - Enabling auto-reconnect functionality
     * - Waiting up to 20 seconds for connection establishment
     * - Updating internal status on success/failure
     *
     * @return connect_t Structure containing connection result:
     *         - isConnected: true if connection successful
     *         - ip: Pointer to IPAddress (caller owns memory, nullptr on failure)
     * @note Function includes connection attempt serialization to prevent race conditions
     * @note Returns immediately if connection attempt is already in progress
     */
    connect_t connect();

    /**
     * @brief Disconnect from the current WiFi network
     *
     * Attempts to cleanly disconnect from WiFi if currently connected.
     * Uses ESP32 WiFi.disconnect(true) to also clear stored settings
     * and disable WiFi radio for complete disconnection.
     *
     * @retval true Disconnection was successful
     * @retval false Disconnection failed or already disconnected
     * @note Updates internal status flags on successful disconnection
     */
    bool disconnect();

    /**
     * @brief Get reference to the WiFi status tracking object
     *
     * Provides access to the internal WifiStatus object for reading
     * connection state, IP address information, and status updates.
     *
     * @return WifiStatus& Reference to internal WiFi status tracker
     * @note The returned reference remains valid for the lifetime of this object
     */
    WifiStatus &getStatus();

private:
    GSettings &settings;             ///< Reference to global settings for WiFi credentials
    WifiStatus wifiStatus;           ///< WiFi status tracking object
    bool isConnectionTrying = false; ///< Flag to prevent concurrent connection attempts
};