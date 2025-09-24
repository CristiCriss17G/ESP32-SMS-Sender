#include "BTLe.hpp"

ServerCallbacks::ServerCallbacks(WifiStatus &wifiStatus) : wifiStatus(wifiStatus)
{
    pAdvertising = NimBLEDevice::getAdvertising();
}

void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    deviceConnected = true;
    Serial.println("Client connected");
    Serial.print(connInfo.getAddress().toString().c_str());
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    deviceConnected = false;
    Serial.println("Client disconnected");
    if (!wifiStatus.getConnected())
    {
        // Resume advertising since WiFi is not connected
        pAdvertising->start();
        Serial.println("Advertising resumed");
    }
}

NimBLEAdvertising *ServerCallbacks::getAdvertising()
{
    return pAdvertising;
}

bool ServerCallbacks::isDeviceConnected()
{
    return deviceConnected;
}

CharacteristicCallbacks::CharacteristicCallbacks(WifiStatus &wifiStatus, Settings &settings, NimBLECharacteristic *&notifyCharacteristic) : wifiStatus(wifiStatus), settings(settings), notifyCharacteristic(notifyCharacteristic)
{
}

void CharacteristicCallbacks::onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    Serial.println("Read request received");
    // Allocate the JSON document
    JsonDocument doc;
    JsonObject wifiS = doc["wifiStatus"].to<JsonObject>();
    wifiStatus.toJson(wifiS);
    JsonObject settingsS = doc["settings"].to<JsonObject>();
    settings.toJson(settingsS);
    String output;
    serializeJson(doc, output);
    pCharacteristic->setValue(output.c_str());
}

void CharacteristicCallbacks::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0)
    {
        Serial.println("Received WiFi credentials!");
        // Serial.printf("Received: %s\n", rxValue.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, rxValue.c_str());
        if (error)
        {
            Serial.print("Failed to parse JSON: ");
            Serial.println(error.c_str());
            return;
        }
        bool toSavePreferences = false;
        bool tryWifiConnect = false;
        bool newServerInfo = false;
        if (doc["deviceName"].is<const char *>())
        {
            settings.setDeviceName(doc["deviceName"].as<String>());
            toSavePreferences = true;
            Serial.printf("Device Name: %s\n", settings.getDeviceName().c_str());
        }
        if (doc["ssid"].is<const char *>() && doc["password"].is<const char *>())
        {
            settings.setSsid(doc["ssid"].as<String>());
            settings.setPassword(doc["password"].as<String>());
            toSavePreferences = true;
            tryWifiConnect = true;
            Serial.printf("SSID: %s\n", settings.getSsid().c_str());
        }
        if (toSavePreferences)
        {
            settings.save();
        }
        if (tryWifiConnect && data_send != nullptr)
        {
            if (data_send->isConnected())
            {
                Serial.println("Disconnecting from WiFi...");
                data_send->disconnect();
            }
            Serial.println("Connecting to WiFi...");
            HTTPTransmission::connect_t result = data_send->connect();
            if (result.isConnected)
            {
                Serial.println("Connected to WiFi!");
                wifiStatus.setConnected(true);
                wifiStatus.setIpAddress(result.ip->toString());
                if (notifyCharacteristic != nullptr)
                {
                    notifyCharacteristic->notify("S:WC,NR,IP:" + wifiStatus.getIpAddress());
                }
            }
            else
            {
                Serial.println("Failed to connect to WiFi!");
                wifiStatus.setConnected(false);
                if (notifyCharacteristic != nullptr)
                {
                    notifyCharacteristic->notify("S:WF");
                }
            }
        }
        if (newServerInfo)
        {
            Serial.println("New server info received! Needs restart to apply changes.");
            if (notifyCharacteristic != nullptr)
            {
                notifyCharacteristic->notify("S:SI,NR");
            }
        }
        if (doc["restart"].is<bool>() && doc["restart"].as<bool>())
        {
            Serial.println("Restarting esp32 to apply new settings...");
            ESP.restart();
        }
    }
}
