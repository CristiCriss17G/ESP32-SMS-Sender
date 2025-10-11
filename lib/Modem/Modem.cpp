#include "Modem.hpp"

/**
 * @brief Default carrier profile used when operator is unknown or unsupported
 *
 * Fallback profile pointer used by selectProfile() when no matching carrier
 * configuration is found in the PROFILES array. Currently set to nullptr
 * to indicate unknown operator, causing initModem() to use generic settings.
 */
const CarrierProfile *DEFAULT_PROFILE = nullptr; // if unknown operator

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
            isConnected = isCsRegistered(); // for SMS on Digi
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
 * @brief Clean initialization of the GSM modem without carrier-specific configurations
 *
 * Simplified initialization path that performs basic modem setup without
 * attempting carrier-specific optimizations. Useful for debugging or when
 * carrier profiles are causing connection issues.
 */
void Modem::initModemClean()
{
    String res;

    modemPowerOn();

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX, false);
    delay(600);

    Serial.println(F("[MODEM] Initializing..."));
    if (!modem.init())
    {
        Serial.println(F("[MODEM] init failed, restarting modem..."));
        modem.restart();
    }

    String name = modem.getModemName();
    Serial.println("[MODEM] Modem Name: " + name);

    String modemInfo = modem.getModemInfo();
    Serial.println("[MODEM] Modem Info: " + modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3)
    {
        modem.simUnlock(GSM_PIN);
    }

    String imsi = readIMSI();
    String mccmnc = mccmncFromIMSI(imsi);
    const CarrierProfile *prof = selectProfile(mccmnc);
    Serial.printf("[SIM] IMSI=%s  MCCMNC=%s  Profile=%s\n",
                  imsi.c_str(), mccmnc.c_str(), prof ? prof->name : "default");

    // NOTE: don't spam CBANDCFG; many firmwares disallow it
    // Keep DTR low to avoid sleep
    pinMode(MODEM_DTR, OUTPUT);
    digitalWrite(MODEM_DTR, LOW);

    modem.sendAT("+CNMP?");
    if (modem.waitResponse(1000L, res) == 1)
    {
        res.replace(GSM_NL "OK" GSM_NL, "");
        Serial.println("[MODEM][CNMP] Mode=" + res);
    }

    if (!setupRadioWithProfile(prof))
    {
        Serial.println("[MODEM] No CS registration with preferred modes, last resort AUTO...");
        modem.sendAT("+CNMP=2");
        modem.waitResponse();
        waitCsRegistered(30000);
    }

    if (!isCsRegistered())
    {
        Serial.println("[MODEM] Still not CS registered — SMS will fail here.");
    }
    else
    {
        Serial.println("[MODEM] CS registered — SMS ready.");
    }
}

/**
 * @brief Read IMSI from SIM card using AT+CIMI command
 *
 * Sends the AT+CIMI command to retrieve the International Mobile Subscriber Identity
 * from the SIM card. This 15-digit number contains carrier identification information.
 */
String Modem::readIMSI()
{
    modem.sendAT("+CIMI");
    if (modem.waitResponse(2000L, "+CIMI") == 1)
    {
        // ditch echo line
        modem.stream.readStringUntil('\n');
    }
    String imsi = modem.stream.readStringUntil('\n');
    imsi.trim();
    return imsi; // e.g. "22605xxxxxxxxx"
}

/**
 * @brief Extract MCC+MNC from IMSI string for carrier identification
 *
 * Parses the first 5 digits of the IMSI to get Mobile Country Code (3 digits)
 * and Mobile Network Code (2 digits) used for carrier profile selection.
 */
String Modem::mccmncFromIMSI(const String &imsi)
{
    if (imsi.length() < 5)
        return "";
    return imsi.substring(0, 5); // MCC(3)+MNC(2)
}

/**
 * @brief Select carrier-specific configuration profile based on MCCMNC
 *
 * Looks up the appropriate CarrierProfile for the given MCCMNC combination.
 * Searches through the PROFILES array for a matching carrier configuration.
 */
const CarrierProfile *Modem::selectProfile(const String &mccmnc)
{
    for (auto &p : PROFILES)
    {
        if (mccmnc == p.mccmnc)
            return &p;
    }
    return DEFAULT_PROFILE;
}

/**
 * @brief Configure radio parameters using carrier-specific profile
 *
 * Applies carrier-optimized settings including network modes, RAT selection,
 * and operator preferences to improve connection reliability and speed.
 */
bool Modem::setupRadioWithProfile(const CarrierProfile *prof)
{
    String res;
    // URCs only; keep logs quiet
    modem.sendAT("+CREG=2");
    modem.waitResponse();
    modem.sendAT("+CGREG=2");
    modem.waitResponse();

    // Optional: operator lock (reduces re-scan)
    if (prof && prof->plmn && prof->act >= 0)
    {
        modem.sendAT("+COPS=1,2,\"" + String(prof->plmn) + "\"," + String(prof->act));
        modem.waitResponse();
    }
    else
    {
        modem.sendAT("+COPS=0");
        modem.waitResponse(); // automatic
    }

    // Try preferred modes in order
    for (int i = 0; i < 4; ++i)
    {
        uint8_t mode = prof ? prof->modes[i] : 2; // default auto
        if (mode == 0)
            continue;

        // Set mode
        modem.setNetworkMode(mode);

        modem.sendAT("+CMNB?");
        if (modem.waitResponse(1000L, res) == 1)
        {
            res.replace(GSM_NL "OK" GSM_NL, "");
            Serial.println("Preferred CMNB mode: " + res);
        }
        res = "";
        // Set LTE-M/NB preference only if we're in LTE family mode
        if (prof && prof->cmnb >= 0 && (mode == 38 || mode == 51))
        {
            modem.sendAT("+CMNB=" + String(prof->cmnb));
            modem.waitResponse();
        }

        // Give RF a moment
        delay(1500);

        Serial.printf("[RADIO] Trying mode %u ...\n", mode);
        if (waitCsRegistered(30000))
        {
            Serial.println("[RADIO] CS registered.");
            return true;
        }
    }
    return false;
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
 * @brief Wait for Circuit-Switched registration with polling and timeout
 *
 * Continuously polls modem registration status until successful registration
 * or timeout expires. Uses isCsRegistered() internally for status checks.
 */
bool Modem::waitCsRegistered(uint32_t ms)
{
    uint32_t deadline = millis() + ms;
    while (millis() < deadline)
    {
        if (isCsRegistered())
            return true;
        delay(500);
    }
    return false;
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
 * @brief Send SMS with additional safety checks and error recovery
 *
 * Enhanced SMS sending with validation, registration checks, and retry logic.
 * Includes phone number validation, length checks, and modem status verification.
 */
bool Modem::sendSmsSafe(const String &to, const String &text)
{
    if (text.length() < 1 || text.length() > 160)
        return false;
    if (!(to.startsWith("+") && to.substring(1).length() >= 7))
        return false;

    modemBusy = true;

    if (!waitCsRegistered(15000))
    {
        modemBusy = false;
        Serial.println("[SMS] Not CS-registered; abort.");
        return false;
    }

    modem.sendAT("+CMGF=1");
    modem.waitResponse();
    Serial.printf("[SMS] To: %s  Len: %u\n", to.c_str(), (unsigned)text.length());
    bool ok = modem.sendSMS(to.c_str(), text.c_str());

    modemBusy = false;
    return ok;
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