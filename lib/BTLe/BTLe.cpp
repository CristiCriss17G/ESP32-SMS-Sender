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
    if (!wifiStatus.isWifiConnected())
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

CharacteristicCallbacks::CharacteristicCallbacks(Settings &settings, NimBLECharacteristic *&notifyCharacteristic, WifiConnection *&wifiConnection) : settings(settings), notifyCharacteristic(notifyCharacteristic), wifiConnection(wifiConnection)
{
}

void CharacteristicCallbacks::onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    Serial.println("Read request received");
    // Allocate the JSON document
    JsonDocument doc;
    JsonObject wifiS = doc["wifiStatus"].to<JsonObject>();
    wifiConnection->getStatus().toJson(wifiS);
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
        if (tryWifiConnect && wifiConnection != nullptr)
        {
            if (wifiConnection->getStatus().isWifiConnected())
            {
                Serial.println("Disconnecting from WiFi...");
                wifiConnection->disconnect();
            }
            Serial.println("Connecting to WiFi...");
            connect_t result = wifiConnection->connect();
            if (result.isConnected)
            {
                Serial.println("Connected to WiFi!");
                wifiConnection->getStatus().setWifiConnected(true);
                wifiConnection->getStatus().setIpAddress(*result.ip);
                if (notifyCharacteristic != nullptr)
                {
                    notifyCharacteristic->notify("S:WC,NR,IP:" + wifiConnection->getStatus().getIpAddress());
                }
            }
            else
            {
                Serial.println("Failed to connect to WiFi!");
                wifiConnection->getStatus().setWifiConnected(false);
                if (notifyCharacteristic != nullptr)
                {
                    notifyCharacteristic->notify(String("S:WF,NR"));
                }
            }
        }
        if (newServerInfo)
        {
            Serial.println("New server info received! Needs restart to apply changes.");
            if (notifyCharacteristic != nullptr)
            {
                notifyCharacteristic->notify(String("S:SI,NR"));
            }
        }
        if (doc["restart"].is<bool>() && doc["restart"].as<bool>())
        {
            Serial.println("Restarting esp32 to apply new settings...");
            ESP.restart();
        }
    }
}
