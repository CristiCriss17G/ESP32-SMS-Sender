#include "WifiConnection.hpp"

WifiStatus::WifiStatus() : connection{false, NULL} {}

WifiStatus::~WifiStatus()
{
    if (connection.ip)
    {
        delete connection.ip;
    }
}

bool WifiStatus::isWifiConnected()
{
    return connection.isConnected;
}

void WifiStatus::setWifiConnected(bool connected)
{
    connection.isConnected = connected;
}

String WifiStatus::getIpAddress()
{
    return connection.ip ? connection.ip->toString() : "0.0.0.0";
}

void WifiStatus::setIpAddress(String ipAddress)
{
    connection.ip = new IPAddress();
    connection.ip->fromString(ipAddress);
}

void WifiStatus::setIpAddress(IPAddress ipAddress)
{
    connection.ip = new IPAddress(ipAddress);
}

void WifiStatus::updateConnection(connect_t connection)
{
    this->connection = connection;
}

void WifiStatus::toJson(JsonObject &root)
{
    root["connected"] = connection.isConnected;
    root["ipAddress"] = connection.ip ? connection.ip->toString() : "0.0.0.0";
}

String WifiStatus::toString()
{
    return "Connected: " + String(connection.isConnected) + ", IP Address: " + getIpAddress();
}

WifiConnection::WifiConnection(Settings &settings) : settings(settings), wifiStatus()
{
}

WifiConnection::~WifiConnection()
{
    disconnect();
}

connect_t WifiConnection::connect()
{
    if (isConnectionTrying)
    {
        Serial.println("Connection already in progress");
        return {false, NULL};
    }
    isConnectionTrying = true;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(settings.getDeviceName().c_str());
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.getSsid().c_str(), settings.getPassword().c_str());
    Serial.println("Connecting");
    int maxRetries = 20; // Wait for a maximum of 20 seconds
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 240)
    {
        delay(500);
        Serial.print(".");
        i++;
    }
    if (WiFi.status() != WL_CONNECTED && i == 240)
    {
        Serial.println("Nu s-a putut conecta la retea");
        isConnectionTrying = false;
        return {false, NULL};
    }
    isConnectionTrying = false;
    wifiStatus.updateConnection({true, new IPAddress(WiFi.localIP())});
    return {true, new IPAddress(WiFi.localIP())};
}

bool WifiConnection::disconnect()
{
    if (WiFi.status() == WL_CONNECTED && WiFi.disconnect(true))
    {
        Serial.println("Disconnected from WiFi");
        return true;
    }
    return false;
}

WifiStatus &WifiConnection::getStatus()
{
    return wifiStatus;
}
