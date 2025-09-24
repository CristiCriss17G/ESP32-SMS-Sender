#ifndef BTLE_H
#define BTLE_H

#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "GSettings.hpp"
#include "WifiConnection.hpp"

// BLE parameters
#define BLE_DEVICE_NAME "ESP32-BLE-Example"
#define SERVICE_UUID "9379d945-8ada-41b7-b028-64a8dda4b1f8"
// #define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_READ_WRITE_UUID "c62b53d0-1848-424d-9d05-fd91e83f87a8" // For receiving WiFi credentials
#define CHAR_NOTIFY_UUID "6cd49c0f-0c41-475b-afc5-5d504afca7dc"     // For sending status/IP updates

// Callback class to handle server events
class ServerCallbacks : public NimBLEServerCallbacks
{
public:
    ServerCallbacks(WifiStatus &wifiStatus);
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;
    NimBLEAdvertising *getAdvertising();
    bool isDeviceConnected();

protected:
    // Flag to know if a client is connected
    bool deviceConnected = false;
    WifiStatus &wifiStatus;
    NimBLEAdvertising *pAdvertising;
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
public:
    CharacteristicCallbacks(Settings &settings, NimBLECharacteristic *&notifyCharacteristic, WifiConnection *&wifiConnection);
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

protected:
    Preferences preferences;
    Settings &settings;
    WifiConnection *&wifiConnection;
    NimBLECharacteristic *&notifyCharacteristic;
};

#endif