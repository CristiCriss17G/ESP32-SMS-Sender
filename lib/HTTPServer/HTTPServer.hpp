#pragma once

#include <WebServer.h>
#include <ArduinoJson.h>
#include "GSettings.hpp"
#include "WifiConnection.hpp"

/**
 * @brief Function pointer type for SMS sending functionality
 *
 * @param to Destination phone number in international format
 * @param text SMS message text content
 * @return true if SMS was sent successfully
 * @return false if SMS sending failed
 */
using SMSFunction = std::function<bool(const String &to, const String &text)>;

/**
 * @brief Function pointer type for checking modem network registration
 *
 * @return true if modem is registered and ready to send SMS
 * @return false if modem is not registered on the network
 */
using CheckModemRegisteredFunction = std::function<bool()>;

/**
 * @brief HTTP Server class for ESP32 SMS Sender application
 *
 * This class provides a web server interface for sending SMS messages.
 * It serves an HTML form interface and a REST API endpoint for sending
 * SMS through the connected GSM modem.
 *
 * Features:
 * - Web interface with HTML form for SMS sending
 * - REST API endpoint (POST /send) with JSON payload
 * - CORS support for cross-origin requests
 * - Phone number format validation
 * - Modem registration status checking
 *
 * REST API
 * - Endpoint: POST /send
 * - Request headers: Content-Type: application/json
 * - Request body (JSON):
 *   {
 *     "to": "+40123456789",   // E.164 or local format
 *     "text": "Hello world"   // message body, UTF-8/GSM-7
 *   }
 * - Response (application/json):
 *   Success: { "ok": true }
 *   Failure: { "ok": false, "error": "reason" }
 * - Error cases: invalid JSON, missing fields, bad phone format, modem not registered
 *
 * @note Requires a GSM modem with SMS capability and valid network registration
 */
class HTTPServer
{
public:
    /**
     * @brief Construct a new HTTP Server object
     *
     * @param settings Reference to the global settings object for accessing configuration
     * @param wifiConnection Reference to the WiFi connection object for network status
     * @param sendSMSFunc Function pointer for sending SMS messages
     * @param checkModemRegisteredFunc Function pointer for checking if modem is registered to network
     * @param port HTTP server port number (default: 80)
     * @param ledPin GPIO pin number for LED indicator (default: -1, no LED)
     */
    HTTPServer(GSettings &settings, WifiConnection &wifiConnection, SMSFunction sendSMSFunc, CheckModemRegisteredFunction checkModemRegisteredFunc, int port = 80, int ledPin = -1);
    /**
     * @brief Destructor for HTTP Server object
     *
     * Stops the web server and cleans up allocated memory
     */
    ~HTTPServer();

    /**
     * @brief Handle incoming client requests
     *
     * Must be called regularly in the main loop to process HTTP requests.
     */
    void handleClient();

    /** @brief HTTP content type constant for JSON responses */
    const char *APPLICATION_JSON = "application/json";

private:
    int led;
    WebServer *server;                                 ///< Pointer to the ESP32 WebServer instance
    GSettings &settings;                               ///< Reference to global settings object
    WifiConnection &wifiConnection;                    ///< Reference to WiFi connection manager
    SMSFunction sendSMS;                               ///< Function pointer for SMS sending
    CheckModemRegisteredFunction checkModemRegistered; ///< Function pointer for checking modem registration

    /**
     * @brief Handle the root endpoint (GET /)
     *
     * Serves the main HTML page with SMS sending form interface.
     * Sets CORS headers and returns a complete HTML page with JavaScript
     * for interacting with the SMS API.
     */
    void handleRoot();

    /**
     * @brief Handle 404 Not Found errors
     *
     * Responds to requests for unknown endpoints with a 404 error message.
     */
    void handleNotFound();

    /**
     * @brief Handle SMS sending endpoint (POST /send)
     *
     * Input JSON fields:
     * - to (string, required): phone number in E.164 or local format
     * - text (string, required): message body (160 GSM-7 chars typical per SMS)
     *
     * Behavior:
     * - Validates JSON and fields
     * - Validates phone number format via looksLikePhone
     * - Checks modem registration via checkModemRegistered
     * - Calls sendSMS on success path
     *
     * Responses:
     * - 200, {"ok": true} on success
     * - 400, {"ok": false, "error": "..."} for bad input
     * - 503, {"ok": false, "error": "modem not registered"} when offline
     */
    void handleSend();

    /**
     * @brief Send CORS (Cross-Origin Resource Sharing) headers
     *
     * Adds necessary headers to allow cross-origin requests from web browsers.
     * Enables access from any origin with POST, GET, and OPTIONS methods.
     */
    void sendCors();

    /**
     * @brief Handle preflight OPTIONS requests
     *
     * Responds to CORS preflight requests by sending appropriate headers
     * and returning a 204 No Content status.
     */
    void handleOptions();

    /**
     * @brief Validate if a string looks like a phone number
     *
     * Performs basic validation to check if the input string resembles
     * a phone number format (contains only digits and optional leading '+').
     * Typical accepted lengths are 8..16 digits depending on region.
     *
     * @param s String to validate as phone number
     * @retval true If string appears to be a valid phone number format
     * @retval false If string does not match phone number pattern
     */
    bool looksLikePhone(const String &s);
};