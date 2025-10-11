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
 * @struct CarrierProfile
 * @brief Carrier-specific configuration profile for optimal modem settings
 *
 * Contains operator-specific parameters for network registration optimization.
 * Each profile includes preferred network modes, radio access technologies,
 * and optional operator locking to improve connection reliability and speed.
 *
 * Network Mode Values:
 * - 2: Automatic mode selection
 * - 13: GSM only (2G)
 * - 38: LTE only (Cat-M/NB-IoT)
 * - 51: GSM + LTE (Cat-M/NB-IoT)
 *
 * CMNB Values (LTE preference):
 * - 0: Cat-M preferred
 * - 1: NB-IoT preferred
 * - 2: Cat-M only
 * - 3: NB-IoT only
 * - -1: Don't modify CMNB setting
 *
 * ACT Values (Access Technology):
 * - 0: GSM
 * - 8: Cat-M (LTE-M)
 * - 9: NB-IoT
 * - -1: Unspecified/automatic
 */
struct CarrierProfile
{
    const char *name;   ///< Human-readable carrier name for identification
    const char *mccmnc; ///< Mobile Country Code + Mobile Network Code (e.g., "22601" for Vodafone Romania)

    /**
     * @brief Preferred network modes to try in order of priority
     *
     * Array of 4 network mode values to attempt sequentially:
     * - 13: GSM only (2G) - Most compatible, lowest data rates
     * - 38: LTE only (Cat-M/NB-IoT) - Modern IoT protocols
     * - 51: GSM + LTE (Cat-M/NB-IoT) - Hybrid mode
     * - 2: Automatic mode selection - Let modem decide
     */
    uint8_t modes[4];

    /**
     * @brief LTE-M/NB-IoT preference setting for CMNB command
     *
     * Controls LTE technology preference when using modes 38 or 51:
     * - 0: Cat-M preferred over NB-IoT
     * - 1: NB-IoT preferred over Cat-M
     * - 2: Cat-M only (disable NB-IoT)
     * - 3: NB-IoT only (disable Cat-M)
     * - -1: Don't modify CMNB setting (use modem defaults)
     */
    int cmnb;

    /**
     * @brief Public Land Mobile Network identifier for operator locking
     *
     * Optional PLMN string for manual operator selection (e.g., "22605" for Digi).
     * Used with COPS command to force registration to specific operator.
     * Set to nullptr to disable operator locking.
     */
    const char *plmn;

    /**
     * @brief Access Technology for operator locking
     *
     * Specifies radio access technology when using PLMN locking:
     * - 0: GSM (2G)
     * - 8: Cat-M (LTE-M)
     * - 9: NB-IoT
     * - -1: Unspecified (let operator choose)
     */
    int act;

    /**
     * @brief Access Point Name for data connections
     *
     * APN string for packet data contexts. Used only if data connectivity
     * is required in addition to SMS. Set to empty string if not needed.
     */
    const char *apn;

    const char *user; ///< Username for APN authentication (usually empty for SMS-only)
    const char *pass; ///< Password for APN authentication (usually empty for SMS-only)
};

/**
 * @brief Array of predefined carrier profiles for Romanian mobile operators
 *
 * Contains optimized configurations for major Romanian carriers including
 * Vodafone, Digi, and Orange. Each profile specifies preferred network modes,
 * LTE technology preferences, and optional operator locking parameters.
 *
 * Profiles are ordered by popularity and coverage. The selectProfile() function
 * searches this array to find the best configuration for a detected carrier.
 *
 * @note Add new carrier profiles here as needed for other operators
 * @note Profiles are region-specific (Romania - MCC 226)
 */
static const CarrierProfile PROFILES[] = {
    // Vodafone RO (22601) — typically LTE-M available; SMS OK everywhere
    {
        "Vodafone RO", "22601",
        /*modes*/ {38, 51, 13, 2}, /*prefer LTE-M/NB first, fallback GSM*/
        /*cmnb*/ 0,                /*CAT-M preferred*/
        /*lock*/ "22601", 8,       /*optional: lock CAT-M (8)*/
        /*APN*/ "", "", ""         /*fill your APN if you need data*/
    },

    // Digi RO (22605) — practical for SIM7000G only on GSM
    {
        "Digi RO", "22605",
        /*modes*/ {13, 2, 38, 51}, /*prefer GSM first*/
        /*cmnb*/ -1,
        /*lock*/ "22605", 0,       /*optional: lock GSM*/
        /*APN*/ "internet", "", "" /*common generic APN; change if required*/
    },

    // Orange RO (22610) — LTE-M/NB often available
    {"Orange RO", "22610",
     /*modes*/ {38, 51, 13, 2},
     /*cmnb*/ 0,
     /*lock*/ "22610", 8,
     /*APN*/ "", "", ""},
};

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
     * @brief Clean initialization of the GSM modem without carrier-specific configurations
     *
     * Performs a simplified modem initialization sequence:
     * - Basic UART setup and modem wake-up
     * - Standard AT command initialization
     * - Generic network registration without carrier profiles
     * - Minimal configuration for basic SMS functionality
     *
     * This method is used when carrier-specific optimizations are not needed
     * or when the automatic carrier detection fails.
     *
     * @note Less comprehensive than initModem() but more reliable across carriers
     */
    void initModemClean();

    /**
     * @brief Read the International Mobile Subscriber Identity (IMSI) from the SIM card
     *
     * Retrieves the IMSI using the AT+CIMI command. The IMSI is a unique identifier
     * stored on the SIM card that contains the Mobile Country Code (MCC) and
     * Mobile Network Code (MNC) used for carrier identification.
     *
     * IMSI Format: MCCMNC + subscriber number (typically 15 digits)
     * Example: "226010123456789" (MCC=226, MNC=01 for T-Mobile Romania)
     *
     * @return String containing the IMSI, or empty string if read failed
     * @note Requires SIM card to be inserted and unlocked
     */
    String readIMSI();

    /**
     * @brief Extract Mobile Country Code and Mobile Network Code from IMSI
     *
     * Parses the IMSI string to extract the MCC (first 3 digits) and MNC
     * (next 2-3 digits) which together identify the mobile network operator.
     * This information is used for carrier-specific configuration selection.
     *
     * @param imsi The IMSI string obtained from readIMSI()
     * @return String containing MCCMNC (e.g., "22601" for T-Mobile Romania)
     * @retval Empty string if IMSI format is invalid or too short
     */
    String mccmncFromIMSI(const String &imsi);

    /**
     * @brief Select carrier-specific configuration profile based on MCCMNC
     *
     * Looks up the appropriate CarrierProfile for the given MCCMNC combination.
     * Carrier profiles contain optimized settings for network registration,
     * preferred radio access technologies, and other operator-specific parameters.
     *
     * @param mccmnc Mobile Country Code + Mobile Network Code string
     * @return Pointer to CarrierProfile if found, DEFAULT_PROFILE if unknown carrier
     * @note Returns nullptr or DEFAULT_PROFILE for unsupported carriers
     */
    const CarrierProfile *selectProfile(const String &mccmnc);

    /**
     * @brief Configure radio parameters using carrier-specific profile
     *
     * Applies carrier-optimized settings including:
     * - Preferred network modes (2G/3G/4G priority)
     * - Radio Access Technology (RAT) selection
     * - Band preferences and network operator settings
     * - Registration timeouts and retry parameters
     *
     * This method optimizes the modem configuration for the detected carrier
     * to improve connection reliability and speed.
     *
     * @param prof Pointer to CarrierProfile containing optimization parameters
     * @retval true Configuration applied successfully
     * @retval false Configuration failed or profile was invalid
     * @note May take several seconds to apply all settings
     */
    bool setupRadioWithProfile(const CarrierProfile *prof);

    /**
     * @brief Check Circuit-Switched (CS) registration using AT+CREG?
     *
     * Parses the +CREG status and returns true for 1 (home) or 5 (roaming).
     * Does not wait or retry; quick check only.
     */
    bool isCsRegistered();

    /**
     * @brief Wait for Circuit-Switched registration with timeout
     *
     * Continuously polls the modem registration status using AT+CREG? until
     * either successful registration is achieved or the timeout expires.
     * This is useful when the modem needs time to complete network registration.
     *
     * Registration Status:
     * - 1: Registered (home network) - SUCCESS
     * - 5: Registered (roaming) - SUCCESS
     * - Other values: Not registered - CONTINUE WAITING
     *
     * @param ms Timeout in milliseconds (default: 30000ms = 30 seconds)
     * @retval true Registration successful within timeout period
     * @retval false Timeout expired without successful registration
     * @note Blocking function that polls every few seconds until timeout
     */
    bool waitCsRegistered(uint32_t ms = 30000);

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
     * @brief Send SMS with additional safety checks and error recovery
     *
     * Enhanced SMS sending function that includes additional validation and
     * recovery mechanisms compared to the basic sendSMS() method:
     * - Pre-flight validation of modem registration status
     * - Phone number format validation and normalization
     * - Message length checks and segmentation warnings
     * - Automatic retry on temporary failures
     * - Detailed error logging for troubleshooting
     *
     * Safety Features:
     * - Validates network registration before attempting send
     * - Implements retry logic for temporary network issues
     * - Provides more detailed error reporting
     * - Handles edge cases in phone number formatting
     *
     * @param to Destination phone number (E.164 format recommended)
     * @param text Message content (will be segmented if >160 characters)
     * @retval true Message sent successfully or queued for delivery
     * @retval false Send failed after all retry attempts or invalid parameters
     * @note Slightly slower than sendSMS() due to additional checks
     */
    bool sendSmsSafe(const String &to, const String &text);

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
    volatile bool modemBusy = false;
};