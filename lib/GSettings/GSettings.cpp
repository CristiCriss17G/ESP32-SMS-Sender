#include "GSettings.hpp"

WifiStatus::WifiStatus() : connected(false), ipAddress("") {}

bool WifiStatus::getConnected()
{
    return connected;
}

void WifiStatus::setConnected(bool connected)
{
    this->connected = connected;
}

String WifiStatus::getIpAddress()
{
    return ipAddress;
}

void WifiStatus::setIpAddress(String ipAddress)
{
    this->ipAddress = ipAddress;
}

void WifiStatus::toJson(JsonObject &root)
{
    root["connected"] = connected;
    root["ipAddress"] = ipAddress;
}

String WifiStatus::toString()
{
    return "Connected: " + String(connected) + ", IP Address: " + ipAddress;
}

Settings::Settings() : deviceName("ESP32-BLE-Example"), ssid(""), password("") {}

String Settings::getDeviceName()
{
    return deviceName;
}

void Settings::setDeviceName(String deviceName)
{
    this->deviceName = deviceName;
}

String Settings::getSsid()
{
    return ssid;
}

void Settings::setSsid(String ssid)
{
    this->ssid = ssid;
}

String Settings::getPassword()
{
    return password;
}

void Settings::setPassword(String password)
{
    this->password = password;
}

void Settings::load()
{
    preferences.begin("global-settings", false);
    deviceName = preferences.getString("deviceName", deviceName);
    ssid = preferences.getString("ssid", ssid);
    password = preferences.getString("password", password);
    preferences.end();
}

void Settings::save()
{
    preferences.begin("global-settings", false);
    preferences.putString("deviceName", deviceName);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
}

void Settings::toJson(JsonObject &root)
{
    root["deviceName"] = deviceName;
    root["ssid"] = ssid;
    root["password"] = password.isEmpty() ? "" : password.substring(0, 4) + "****";
}
