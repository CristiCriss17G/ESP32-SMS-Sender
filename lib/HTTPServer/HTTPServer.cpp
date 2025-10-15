#include "HTTPServer.hpp"

/**
 * @brief Construct a new HTTPServer object
 *
 * Initializes the HTTP server with the specified port and sets up route handlers.
 * The server will handle GET requests to root ("/") and POST requests to "/send".
 * Also sets up CORS preflight handling for OPTIONS requests.
 *
 * @param settings Reference to global settings for configuration access
 * @param wifiConnection Reference to WiFi connection manager
 * @param sendSMSFunc Function pointer for SMS sending capability
 * @param checkModemRegisteredFunc Function pointer to check modem network status
 * @param port HTTP server port (default 80)
 */
HTTPServer::HTTPServer(GSettings &settings, WifiConnection &wifiConnection, SMSFunction sendSMSFunc, CheckModemRegisteredFunction checkModemRegisteredFunc, int port, int ledPin) : settings(settings), wifiConnection(wifiConnection), sendSMS(sendSMSFunc), checkModemRegistered(checkModemRegisteredFunc), led(ledPin)
{
    server = new WebServer(port);

    server->on("/", HTTP_GET, std::bind(&HTTPServer::handleRoot, this));
    server->on("/send", HTTP_POST, std::bind(&HTTPServer::handleSend, this));
    server->on("/send", HTTP_OPTIONS, std::bind(&HTTPServer::handleOptions, this));
    server->onNotFound(std::bind(&HTTPServer::handleNotFound, this));

    server->begin();
    Serial.println("[HTTP] server started");
}

/**
 * @brief Destroy the HTTPServer object
 *
 * Stops the HTTP server and deallocates the WebServer instance.
 * Ensures clean shutdown of the web server.
 */
HTTPServer::~HTTPServer()
{
    server->stop();
    delete server;
}

void HTTPServer::handleClient()
{
    server->handleClient();
}

/**
 * @brief Handle HTTP GET requests to the root path "/"
 *
 * Serves a complete HTML page with embedded CSS and JavaScript for SMS sending.
 * The page includes:
 * - A form for entering phone number and message
 * - Client-side validation for message length (160 char limit)
 * - JavaScript code to make AJAX calls to the /send endpoint
 * - Responsive styling for better user experience
 *
 * Sets appropriate CORS headers and content type.
 */
void HTTPServer::handleRoot()
{
    digitalWrite(led, HIGH);
    sendCors();
    String html = F(R"HTML(
<!doctype html><html><head><meta charset="utf-8"><title>T-SIM7000G SMS</title>
<style>body{font-family:system-ui;margin:2rem;max-width:700px}input,textarea{width:100%;padding:.6rem;margin:.3rem 0}button{padding:.6rem 1rem}</style>
</head><body>
<h1>T-SIM7000G — Send SMS</h1>
<p>You can use the form below, or call the API directly with <code>POST /send</code> and JSON <code>{"phone":"+40712345678","message":"Salut!"}</code>.<br>
Note: Message length is limited to 160 characters (classic SMS).</p>
<form id="f">
  <label>Phone (e.g. +40712345678)</label>
  <input id="phone" value="+407">
  <label>Message (max 160 characters)</label>
  <textarea id="msg" rows="4" maxlength="160">Salut! Test SMS de pe T-SIM7000G.</textarea>
  <button type="button" onclick="send()">Send</button>
</form>
<pre id="out"></pre>
<script>
async function send(){
  const phone=document.getElementById('phone').value.trim();
  const message=document.getElementById('msg').value;
  if(message.length > 160){
    document.getElementById('out').textContent="Error: Message too long (max 160 characters).";
    return;
  }
  const r=await fetch('/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({phone,message})});
  const t=await r.text();
  document.getElementById('out').textContent=t;
}
</script>
</body></html>
)HTML");
    server->send(200, "text/html; charset=utf-8", html);
    digitalWrite(led, LOW);
}

void HTTPServer::handleNotFound()
{
    digitalWrite(led, HIGH);
    sendCors();
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server->uri();
    message += "\nMethod: ";
    message += (server->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server->args();
    message += "\n";
    for (uint8_t i = 0; i < server->args(); i++)
    {
        message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
    }
    server->send(404, "text/plain", message);
    digitalWrite(led, LOW);
}

/**
 * @brief Handle HTTP POST requests to "/send" endpoint for SMS transmission
 *
 * Processes SMS sending requests with JSON payload containing phone number and message.
 *
 * Request format: {"phone": "+40712345678", "message": "Text message"}
 *
 * Validation performed:
 * - Ensures POST method is used
 * - Validates JSON payload structure
 * - Checks phone number format using looksLikePhone()
 * - Validates message length (1-480 characters)
 * - Verifies modem network registration
 *
 * Response format: {"status": "ok"} or {"error": "description"}
 *
 * HTTP Status Codes:
 * - 200: SMS sent successfully
 * - 400: Bad request (invalid JSON, phone format, or message length)
 * - 405: Method not allowed (non-POST request)
 * - 500: Internal server error (SMS sending failed)
 * - 503: Service unavailable (modem not registered)
 */
void HTTPServer::handleSend()
{
    digitalWrite(led, HIGH);
    sendCors();

    if (server->method() != HTTP_POST)
    {
        server->send(405, APPLICATION_JSON, "{\"error\":\"Use POST\"}");
        return;
    }

    String body = server->arg("plain");
    if (body.length() == 0)
    {
        server->send(400, APPLICATION_JSON, "{\"error\":\"Empty body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        server->send(400, APPLICATION_JSON, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String phone = doc["phone"] | "";
    String message = doc["message"] | "";

    if (!looksLikePhone(phone))
    {
        server->send(400, APPLICATION_JSON, "{\"error\":\"Invalid phone format. Use +407...\"}");
        return;
    }
    if (message.length() < 1 || message.length() > 480)
    { // allow >160; modem will segment
        server->send(400, APPLICATION_JSON, "{\"error\":\"Message length 1..480 required\"}");
        return;
    }

    if (!checkModemRegistered())
    {
        server->send(503, APPLICATION_JSON, "{\"error\":\"Modem not registered on network\"}");
        return;
    }

    bool ok = sendSMS(phone, message);
    if (ok)
    {
        server->send(200, APPLICATION_JSON, "{\"status\":\"ok\"}");
    }
    else
    {
        server->send(500, APPLICATION_JSON, "{\"status\":\"fail\"}");
    }
    digitalWrite(led, LOW);
}

/**
 * @brief Send Cross-Origin Resource Sharing (CORS) headers
 *
 * Adds HTTP headers to allow cross-origin requests from web browsers.
 * This enables the API to be accessed from different domains/ports.
 *
 * Headers set:
 * - Access-Control-Allow-Origin: * (allows all origins)
 * - Access-Control-Allow-Methods: POST, GET, OPTIONS
 * - Access-Control-Allow-Headers: Content-Type, Authorization
 */
void HTTPServer::sendCors()
{
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server->sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

/**
 * @brief Handle HTTP OPTIONS requests for CORS preflight
 *
 * Responds to browser preflight requests that occur before cross-origin
 * POST requests. Sets CORS headers and returns HTTP 204 No Content.
 * This is required for proper CORS functionality in modern browsers.
 */
void HTTPServer::handleOptions()
{
    digitalWrite(led, HIGH);
    sendCors();
    server->send(204);
    digitalWrite(led, LOW);
}

/**
 * @brief Validate if a string appears to be a valid phone number format
 *
 * Performs basic format validation for phone number strings:
 * - Length must be between 7 and 20 characters
 * - Must contain only digits (0-9) and optionally a leading plus sign (+)
 * - Does not validate actual number validity or regional formats
 *
 * @param s String to validate as phone number
 * @return true if string matches basic phone number pattern
 * @return false if string doesn't match expected format
 */
bool HTTPServer::looksLikePhone(const String &s)
{
    if (s.length() < 7 || s.length() > 20)
        return false;
    for (size_t i = 0; i < s.length(); ++i)
    {
        char c = s[i];
        if (!(c == '+' || (c >= '0' && c <= '9')))
            return false;
    }
    return true;
}
