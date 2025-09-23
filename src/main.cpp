// LilyGO T-SIM7000G — Send a single SMS with TinyGSM
// Board: ESP32 Dev Module
// Tools -> Core Debug Level: None
// Monitor at 115200

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_DEBUG Serial   // comment this to reduce logs
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#define GSM_NL "\r\n"

#define SerialAT Serial1
// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

#define DUMP_AT_COMMANDS

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "live.vodafone.com"; // SET TO YOUR APN
const char gprsUser[] = "live";
const char gprsPass[] = "";

#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <Ticker.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// ===== Pin definitions (LilyGO T-SIM7000G defaults) =====
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON 23
#define MODEM_RST 5
#define MODEM_DTR 32
#define MODEM_RI 33

#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13
#define LED_PIN 12

// Hardware serial for modem
// HardwareSerial SerialAT(1);

// ===== Your static SMS target and message =====
const char *PHONE_NUMBER = "+40725414285";                  // <-- put Vodafone/Digi number in international format
const char *SMS_TEXT = "Salut! Test SMS de pe T-SIM7000G."; // change as you like

// TinyGsm modem(SerialAT);

void modemPowerOn()
{
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, HIGH);
}

// static void modemPowerOn()
// {
//   // Ensure modem has main power
//   pinMode(MODEM_POWER_ON, OUTPUT);
//   digitalWrite(MODEM_POWER_ON, HIGH);
//   delay(100);

//   // Modem reset pin
//   pinMode(MODEM_RST, OUTPUT);
//   digitalWrite(MODEM_RST, HIGH);
//   delay(100);

//   // Toggle PWRKEY to turn the module on
//   pinMode(MODEM_PWRKEY, OUTPUT);
//   digitalWrite(MODEM_PWRKEY, LOW);
//   delay(1000); // hold ~1s
//   digitalWrite(MODEM_PWRKEY, HIGH);

//   // Give time to boot
//   delay(3000);
// }

void modemPowerOff()
{
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1500);
  digitalWrite(MODEM_PWRKEY, HIGH);
}

void modemRestart()
{
  modemPowerOff();
  delay(1000);
  modemPowerOn();
}

void initModem()
{
  String res;

  // Power up modem
  modemPowerOn();

  // Start modem serial
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX, false);
  delay(600);

  // Optional: keep modem awake (DTR LOW)
  pinMode(MODEM_DTR, OUTPUT);
  digitalWrite(MODEM_DTR, LOW);

  Serial.println("/**********************************************************/");
  Serial.println("To initialize the network test, please make sure your LTE ");
  Serial.println("antenna has been connected to the SIM interface on the board.");
  Serial.println("/**********************************************************/\n\n");

  Serial.println("========INIT========");

  if (!modem.init())
  {
    modemRestart();
    delay(2000);
    Serial.println("Failed to restart modem, attempting to continue without restarting");
    return;
  }

  Serial.println("========SIMCOMATI======");
  modem.sendAT("+SIMCOMATI");
  modem.waitResponse(1000L, res);
  res.replace(GSM_NL "OK" GSM_NL, "");
  Serial.println(res);
  res = "";
  Serial.println("=======================");

  Serial.println("=====Preferred mode selection=====");
  modem.sendAT("+CNMP?");
  if (modem.waitResponse(1000L, res) == 1)
  {
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
  }
  res = "";
  Serial.println("=======================");

  Serial.println("=====Preferred selection between CAT-M and NB-IoT=====");
  modem.sendAT("+CMNB?");
  if (modem.waitResponse(1000L, res) == 1)
  {
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
  }
  res = "";
  Serial.println("=======================");

  String name = modem.getModemName();
  Serial.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  Serial.println("Modem Info: " + modemInfo);

  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3)
  {
    modem.simUnlock(GSM_PIN);
  }

  for (int i = 0; i <= 4; i++)
  {
    uint8_t network[] = {
        2,  /*Automatic*/
        13, /*GSM only*/
        38, /*LTE only*/
        51  /*GSM and LTE only*/
    };
    Serial.printf("Try %d method\n", network[i]);
    modem.setNetworkMode(network[i]);
    delay(3000);
    bool isConnected = false;
    int tryCount = 60;
    while (tryCount--)
    {
      int16_t signal = modem.getSignalQuality();
      Serial.print("Signal: ");
      Serial.print(signal);
      Serial.print(" ");
      Serial.print("isNetworkConnected: ");
      isConnected = modem.isNetworkConnected();
      Serial.println(isConnected ? "CONNECT" : "NO CONNECT");
      if (isConnected)
      {
        break;
      }
      delay(1000);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    if (isConnected)
    {
      break;
    }
  }
  digitalWrite(LED_PIN, HIGH);

  Serial.println();
  Serial.println("Device is connected .");
  Serial.println();

  Serial.println("=====Inquiring UE system information=====");
  modem.sendAT("+CPSI?");
  if (modem.waitResponse(1000L, res) == 1)
  {
    res.replace(GSM_NL "OK" GSM_NL, "");
    Serial.println(res);
  }
}

void setup()
{
  // Console
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println(F("\n=== T-SIM7000G SMS Sender ==="));

  initModem();

  // Initialize modem
  // Serial.println(F("Initializing modem..."));
  // if (!modem.init())
  // {
  //   Serial.println(F("Modem init failed. Retrying once..."));
  //   delay(2000);
  //   modem.restart();
  // }

  // // (Optional) Explicitly set SMS text mode via AT
  // modem.sendAT("+CMGF=1"); // SMS text mode
  // modem.waitResponse();

  // // (Optional) Preferred network mode:
  // // For CAT-M1 priority (then GSM fallback): CNMP=38 (auto is usually fine)
  // // modem.sendAT("+CNMP=38"); modem.waitResponse();

  // // (Optional) Prefer LTE-M over NB-IoT: CMNB=1 (0=CAT-M preferred,1=NB-IoT preferred)
  // // modem.sendAT("+CMNB=1"); modem.waitResponse();

  // // Wait for network registration (no APN needed for SMS)
  // Serial.println(F("Waiting for network..."));
  // if (!modem.waitForNetwork(600000L))
  // { // up to 10 minutes if coverage is spotty
  //   Serial.println(F("Network registration failed."));
  //   return;
  // }
  // Serial.println(F("Network registered."));

  // // Send the SMS
  // Serial.print(F("Sending SMS to "));
  // Serial.println(PHONE_NUMBER);
  // bool res = modem.sendSMS(PHONE_NUMBER, SMS_TEXT);

  // if (res)
  // {
  //   Serial.println(F("SMS sent successfully!"));
  // }
  // else
  // {
  //   Serial.println(F("SMS failed to send."));
  // }
}

void loop()
{

  bool resb = modem.sendSMS(PHONE_NUMBER, SMS_TEXT);

  if (resb)
  {
    Serial.println(F("SMS sent successfully!"));
  }
  else
  {
    Serial.println(F("SMS failed to send."));
  }
  delay(20000);
}