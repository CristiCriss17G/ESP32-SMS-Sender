#include "Modem.hpp"

#ifdef DUMP_AT_COMMANDS
StreamDebugger debugger(SerialAT, SerialMon);
Modem::Modem() : modem(debugger)
#else
Modem::Modem() : modem(SerialAT)
#endif
{
}

Modem::~Modem()
{
}

/**
 * @brief Bring up and configure the SIM7000G modem for CS registration and SMS.
 *
 * Steps performed:
 * - Initialize UART and keep the modem awake via DTR
 * - Initialize TinyGSM and print modem capabilities (AT+SIMCOMATI)
 * - Query and print preferred mode and RAT selection (CNMP, CMNB)
 * - Optionally unlock SIM via PIN
 * - Configure verbose registration URCs and iterate network modes
 * - Poll registration until connected or timeout, blinking LED while waiting
 * - Print serving cell/system information via CPSI
 *
 * Blocking behavior: Can take up to a few minutes to cycle through modes.
 */
void Modem::initModem()
{
    String res;

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX, false);
    delay(600);

    pinMode(MODEM_DTR, OUTPUT);
    digitalWrite(MODEM_DTR, LOW); // keep awake

    Serial.println(F("[MODEM] Initializing..."));
    if (!modem.init())
    {
        Serial.println(F("[MODEM] init failed, trying restart()..."));
        modemRestart();
        delay(2000);
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

    // before your for-loop
    modem.sendAT("+CNMP=13");
    modem.waitResponse(); // GSM only
    // modem.sendAT("+CBANDCFG=\"GSM\",\"900,1800\"");
    modem.waitResponse(); // EU 2G bands
    modem.sendAT("+CREG=2");
    modem.waitResponse(); // verbose URCs
    modem.sendAT("+CGREG=2");
    modem.waitResponse();

    for (int i = 0; i < 4; i++)
    {
        uint8_t network[] = {
            13, /*GSM only*/
            51, /*GSM and LTE only*/
            38, /*LTE only*/
            2,  /*Automatic*/
        };
        Serial.printf("Try %d method\n", network[i]);
        modem.setNetworkMode(network[i]);
        delay(3000);
        bool isConnected = false;
        int tryCount = 60;
        while (tryCount--)
        {
            int16_t csq = modem.getSignalQuality();
            Serial.printf("CSQ=%d  ", csq);
            modem.sendAT("+CREG?");
            modem.waitResponse();
            modem.sendAT("+CEREG?");
            modem.waitResponse();
            modem.sendAT("+CPIN?");
            modem.waitResponse();
            modem.sendAT("+CIMI");
            modem.waitResponse();
            // modem.sendAT("+COPS=?");
            // modem.waitResponse();
            Serial.print("isNetworkConnected: ");
            // isConnected = modem.isNetworkConnected();
            isConnected = isCsRegistered(); // pentru SMS pe Digi
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

/**
 * @brief Check Circuit-Switched (CS) registration using AT+CREG?
 *
 * Parses the +CREG status and returns true for 1 (home) or 5 (roaming).
 * Does not wait or retry; quick check only.
 */
bool Modem::isCsRegistered()
{
    modem.sendAT("+CREG?");
    if (modem.waitResponse(2000L, "+CREG:") != 1)
        return false;
    String line = modem.stream.readStringUntil('\n'); // " 2,1,"D160","BDA8",0"
    int c1 = line.indexOf(','), c2 = line.indexOf(',', c1 + 1);
    if (c1 < 0)
        return false;
    String statStr = (c2 > 0) ? line.substring(c1 + 1, c2) : line.substring(c1 + 1);
    statStr.trim();
    int stat = statStr.toInt();
    return (stat == 1 || stat == 5); // 1=home, 5=roaming
}

/**
 * @brief Validate modem registration with a quick query and optional wait.
 *
 * Sends +CREG? for a quick status read, then if the modem is not connected,
 * attempts a best-effort wait using TinyGSM's waitForNetwork for up to 60s.
 *
 * @retval true When network is registered or becomes available within timeout
 * @retval false When the modem remains unregistered after waiting
 */
bool Modem::checkModemRegistered()
{
    // Ensure modem is registered (cheap quick check)
    int reg = -1;
    modem.sendAT("+CREG?");
    if (modem.waitResponse(2000L, "+CREG:") == 1)
    {
        reg = modem.stream.readStringUntil('\n').toInt();
    }
    // Best effort: if not registered, try wait again (non-fatal)
    if (!modem.isNetworkConnected())
    {
        return modem.waitForNetwork(60000L);
    }
    Serial.println("Network registered, status: " + String(reg));
    return true;
}

/**
 * @brief Send a text SMS through the modem.
 *
 * Forwards to TinyGSM::sendSMS. Expects E.164 formatted numbers where possible.
 *
 * @param to Destination number
 * @param text Message body
 * @return true if TinyGSM accepted the message
 */
bool Modem::sendSMS(const String &to, const String &text)
{
    Serial.printf("[SMS] To: %s  Len: %u\n", to.c_str(), (unsigned)text.length());
    return modem.sendSMS(to.c_str(), text.c_str());
}

/**
 * @brief Toggle PWRKEY to start the modem (active-low ~1s pulse).
 */
void Modem::modemPowerOn()
{
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);
}

/**
 * @brief Hold PWRKEY low long enough to request graceful shutdown.
 */
void Modem::modemPowerOff()
{
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1500);
    digitalWrite(MODEM_PWRKEY, HIGH);
}

/**
 * @brief Full power-cycle using PWRKEY off->on with a short delay.
 */
void Modem::modemRestart()
{
    modemPowerOff();
    delay(1000);
    modemPowerOn();
}