#pragma once

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

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif

// ===== Pin definitions (LilyGO T-SIM7000G defaults) =====
#define MODEM_TX 27       ///< GSM modem UART transmit pin
#define MODEM_RX 26       ///< GSM modem UART receive pin
#define MODEM_PWRKEY 4    ///< GSM modem power key control pin
#define MODEM_POWER_ON 23 ///< GSM modem power enable pin
#define MODEM_RST 5       ///< GSM modem reset pin
#define MODEM_DTR 32      ///< GSM modem DTR (Data Terminal Ready) pin
#define MODEM_RI 33       ///< GSM modem RI (Ring Indicator) pin

#define LED_PIN 12 ///< Status LED pin

/**
 * @class Modem
 * @brief High-level wrapper around TinyGSM for SIM7000-based SMS and registration management.
 *
 * Responsibilities:
 * - Power sequencing helpers for the SIM7000G (power on/off/restart)
 * - Modem bring-up and capability probing
 * - Network mode selection and registration checks (CS domain)
 * - Minimal SMS sending primitive used by higher layers (HTTP API)
 *
 * Hardware notes:
 * - Pin definitions target LilyGO T-SIM7000G defaults; adjust if using a different board.
 * - LED on `LED_PIN` may be toggled while waiting for network registration.
 */
class Modem
{
public:
    Modem();
    ~Modem();

    /**
     * @brief Initialize and configure the GSM modem for SMS functionality
     *
     * Comprehensive modem initialization process:
     * 1. Configure UART communication (115200 baud)
     * 2. Set DTR pin to keep modem awake
     * 3. Initialize modem communication
     * 4. Display modem information and capabilities
     * 5. Handle SIM card PIN if required
     * 6. Test multiple network modes for best connectivity
     * 7. Wait for network registration and signal quality
     * 8. Display final connection status and network information
     *
     * Network Modes Tested (in order):
     * - Mode 2: Automatic selection
     * - Mode 13: GSM only
     * - Mode 38: LTE only
     * - Mode 51: GSM and LTE only
     *
     * @note LED flashes during network registration attempts
     * @note Will retry modem restart if initial initialization fails
     * @warning This function is blocking and can take up to a few minutes while scanning modes.
     */
    void initModem();

    /**
     * @brief Check Circuit-Switched (CS) registration using AT+CREG?
     *
     * Parses the +CREG status and returns true for 1 (home) or 5 (roaming).
     * Does not wait or retry; quick check only.
     */
    bool isCsRegistered();

    /**
     * @brief Check if the GSM modem is registered on the cellular network
     *
     * Performs network registration verification using AT+CREG? command
     * to ensure the modem is ready for SMS operations.
     *
     * Registration Status Codes:
     * - 1: Registered (home network)
     * - 5: Registered (roaming)
     * - Other values: Not registered
     *
     * If not registered, attempts to wait for network registration
     * for up to 60 seconds as a recovery mechanism.
     *
     * @retval true Modem is registered and usable for SMS
     * @retval false Modem is not registered on any network
     */
    bool checkModemRegistered();

    /**
     * @brief Send an SMS message using the GSM modem
     *
     * Expects the destination number in E.164 or local dial format.
     * Message text must be GSM-7/UTF-8 compatible as supported by TinyGSM.
     *
     * @param to Destination phone number (prefer E.164, e.g. "+40123456789")
     * @param text Message body to send
     * @retval true Message was accepted for sending by the modem
     * @retval false Sending failed (e.g., not registered, invalid number, or modem error)
     */
    bool sendSMS(const String &to, const String &text);

    /**
     * @brief Power on the GSM modem
     *
     * Activates the SIM7000G modem by toggling the power key pin.
     * Uses the standard power-on sequence: LOW pulse for 1 second,
     * then HIGH to complete the power-on process.
     *
     * @note Blocking: ~1s pulse plus modem boot time.
     */
    static void modemPowerOn();

    /**
     * @brief Power off the GSM modem
     *
     * Safely shuts down the SIM7000G modem by holding the power key
     * LOW for 1.5 seconds, then releasing it to HIGH.
     *
     * @note Blocking: ~1.5s pulse plus shutdown time.
     */
    static void modemPowerOff();

    /**
     * @brief Restart the GSM modem
     *
     * Performs a complete modem restart by powering off, waiting,
     * then powering back on. Useful for recovering from error states.
     *
     * @note Combines modemPowerOff() and modemPowerOn(); overall blocking a few seconds.
     */
    static void modemRestart();

private:
    TinyGsm modem;
};