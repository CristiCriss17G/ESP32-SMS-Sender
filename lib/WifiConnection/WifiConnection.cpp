#include "WifiConnection.hpp"

/**
 * @brief Static null IP address constant for connect_t initialization
 *
 * Provides a default "no address" value (0.0.0.0) used when WiFi connection
 * fails or is not established. Prevents null pointer dereference issues.
 */
IPAddress connect_t::NULL_IP(0, 0, 0, 0);

/**
 * @brief Construct a new WifiStatus object
 *
 * Initializes the WiFi status with disconnected state and null IP address.
 */
WifiStatus::WifiStatus() : connection{false, 0U}
{
    ProbeRegistry::instance().registerProbe("wifi", [this](JsonObject &dst)
                                            { this->toJson(dst); });
}

/**
 * @brief Destroy the WifiStatus object
 *
 * Cleans up allocated IP address memory if it exists.
 */
WifiStatus::~WifiStatus()
{
}

/**
 * @brief Get the current WiFi connection status
 *
 * @return true if WiFi is connected
 * @return false if WiFi is disconnected
 */
bool WifiStatus::isWifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Set the WiFi connection status
 *
 * Updates the internal connection state flag.
 *
 * @param connected true to mark as connected, false for disconnected
 */
void WifiStatus::setWifiConnected(bool connected)
{
    connection.isConnected = connected;
}

/**
 * @brief Get the current IP address as a string
 *
 * @return String representation of IP address, or "0.0.0.0" if no IP assigned
 */
String WifiStatus::getIpAddress()
{
    static uint32_t lastRead = 0;
    if (millis() - lastRead > 60000 || connection.isIPNULL())
    {
        // Refresh IP address every 60 seconds or if not set
        connection.ip = IPAddress(WiFi.localIP());
        lastRead = millis();
        Serial.println(F("Refreshed IP address from WiFi stack"));
    }
    return connection.isIPNULL() ? WiFi.localIP().toString() : connection.ip.toString();
}

/**
 * @brief Set the IP address from string representation
 *
 * Parses string IP address and stores it as IPAddress object.
 * Creates new IPAddress instance and deallocates previous one if it exists.
 *
 * @param ipAddress String representation of IP address (e.g., "192.168.1.100")
 */
void WifiStatus::setIpAddress(String ipAddress)
{
    connection.ip.fromString(ipAddress);
}

/**
 * @brief Set the IP address from IPAddress object
 *
 * Creates a new IPAddress instance as a copy of the provided address.
 *
 * @param ipAddress IPAddress object to copy
 */
void WifiStatus::setIpAddress(IPAddress ipAddress)
{
    connection.ip = ipAddress;
}

/**
 * @brief Update the complete connection information
 *
 * Replaces the current connection status and IP address with new information.
 *
 * @param connection New connection information structure
 */
void WifiStatus::updateConnection(connect_t connection)
{
    this->connection = connection;
}

/**
 * @brief Serialize WiFi status information to JSON object
 *
 * Populates a JSON object with connection status and IP address information.
 * Used for status reporting and API responses.
 *
 * @param root JSON object to populate with status information
 */
void WifiStatus::toJson(JsonObject &root)
{
    root["connected"] = isWifiConnected();
    root["ipAddress"] = getIpAddress();
}

/**
 * @brief Convert WiFi status to human-readable string
 *
 * @return String containing connection status and IP address information
 */
String WifiStatus::toString()
{
    return "Connected: " + String(isWifiConnected()) + ", IP Address: " + getIpAddress();
}

/**
 * @brief Construct a new WifiConnection object
 *
 * Initializes WiFi connection manager with reference to global settings.
 *
 * @param settings Reference to global settings containing WiFi credentials
 */
WifiConnection::WifiConnection(GSettings &settings) : settings(settings), wifiStatus()
{
}

/**
 * @brief Destroy the WifiConnection object
 *
 * Ensures WiFi disconnection before object destruction.
 */
WifiConnection::~WifiConnection()
{
    disconnect();
}

/**
 * @brief Attempt to connect to WiFi network using stored settings
 *
 * Performs the following operations:
 * - Prevents multiple simultaneous connection attempts
 * - Sets WiFi to station mode and configures hostname
 * - Enables auto-reconnect functionality
 * - Attempts connection using SSID and password from settings
 * - Waits up to 120 seconds for successful connection
 * - Updates internal status on success/failure
 *
 * Connection process includes visual feedback via Serial output.
 *
 * @return connect_t Structure containing connection result and IP address
 *         - isConnected: true if connection successful
 *         - ip: Pointer to IPAddress object (caller owns memory)
 */
connect_t WifiConnection::connect()
{
    if (isConnectionTrying)
    {
        Serial.println("Connection already in progress");
        return {false, 0U};
    }
    isConnectionTrying = true;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(settings.getDeviceName().c_str());
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.getSsid().c_str(), settings.getPassword().c_str());
    Serial.println("Connecting to WiFi...");
    int maxRetries = 40; // Wait for a maximum of 20 seconds (40 * 500ms = 20s)
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < maxRetries)
    {
        delay(500);
        Serial.print(".");
        i++;
    }
    if (WiFi.status() != WL_CONNECTED && i == maxRetries)
    {
        Serial.println("Could not connect to network");
        isConnectionTrying = false;
        return {false, 0U};
    }
    isConnectionTrying = false;
    wifiStatus.updateConnection({true, WiFi.localIP()});
    return {true, WiFi.localIP()};
}

/**
 * @brief Disconnect from the current WiFi network
 *
 * Attempts to cleanly disconnect from WiFi if currently connected.
 * Uses the ESP32 WiFi.disconnect() function with true parameter
 * to also disable WiFi and clear stored settings.
 *
 * @return true if disconnection was successful
 * @return false if disconnection failed or already disconnected
 */
bool WifiConnection::disconnect()
{
    if (WiFi.status() == WL_CONNECTED && WiFi.disconnect(true))
    {
        Serial.println("Disconnected from WiFi");
        return true;
    }
    return false;
}

/**
 * @brief Get reference to the WiFi status object
 *
 * @return WifiStatus& Reference to internal WiFi status tracker
 */
WifiStatus &WifiConnection::getStatus()
{
    return wifiStatus;
}
