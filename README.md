# ESP32 SMS Sender

A comprehensive SMS sending solution using the **LilyGO T-SIM7000G** development board (ESP32 + SIM7000G GSM module). This project provides SMS functionality through cellular networks with remote configuration capabilities via Bluetooth Low Energy (BLE) and an optional web interface.

## 🚀 Features

### Core Functionality

- **SMS Sending**: Send SMS messages via cellular GSM/LTE networks
- **HTTP API**: RESTful API for sending SMS messages programmatically
- **Web Interface**: Built-in HTML form for easy SMS sending
- **BLE Configuration**: Configure WiFi credentials and device settings via Bluetooth

### Connectivity

- **Cellular Network**: Automatic network registration with fallback modes
- **WiFi**: Optional WiFi connectivity with persistent credential storage
- **Bluetooth LE**: Device configuration and status monitoring
- **HTTP Server**: Web-based SMS interface with CORS support

### System Management

- **Persistent Settings**: WiFi credentials and device configuration stored in ESP32 NVS
- **Network Auto-recovery**: Automatic reconnection and network mode fallback
- **Status Monitoring**: LED indicators and serial debugging
- **Remote Configuration**: Update settings without physical access

## 📋 Hardware Requirements

### Primary Hardware

- **LilyGO T-SIM7000G** development board
  - ESP32-WROVER module
  - SIM7000G cellular modem
  - Built-in GPS (optional)
  - MicroSD card slot
  - Battery management

### SIM Card Requirements

- **Compatible SIM card** with SMS capability
- Supported networks: GSM/LTE (2G/4G)
- Activated cellular plan with SMS allowance

### Optional Components

- External antenna for better cellular reception
- LiPo battery for portable operation
- MicroSD card for logging (not implemented)

## 🔧 Pin Configuration

```cpp
// GSM Modem Pins (T-SIM7000G)
#define MODEM_TX 27        // UART transmit
#define MODEM_RX 26        // UART receive
#define MODEM_PWRKEY 4     // Power key control
#define MODEM_POWER_ON 23  // Power enable
#define MODEM_RST 5        // Reset pin
#define MODEM_DTR 32       // Data Terminal Ready
#define MODEM_RI 33        // Ring Indicator

// Status LED
#define LED_PIN 12         // Status indication

// SD Card (optional)
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13
```

## 📚 Software Dependencies

### PlatformIO Libraries

```ini
lib_deps =
    vshymanskyy/TinyGSM@^0.12.0         # GSM modem communication
    h2zero/NimBLE-Arduino@^2.3.6       # Bluetooth Low Energy
    bblanchon/ArduinoJson@^7.4.2       # JSON parsing and generation
    vshymanskyy/StreamDebugger@^1.0.1   # AT command debugging
```

### Platform Configuration

- **Platform**: Espressif32 v6.12.0
- **Framework**: Arduino
- **Board**: ESP32-WROVER-KIT
- **Partition**: Custom 16MB flash layout

## 🛠️ Installation & Setup

### 1. Hardware Setup

1. Insert a compatible SIM card into the T-SIM7000G board
2. Connect the cellular antenna
3. Optional: Connect external power or battery

### 2. Software Setup

1. **Install PlatformIO** IDE or extension
2. **Clone this repository**:
   ```bash
   git clone <repository-url>
   cd esp32-sms-sender
   ```
3. **Configure cellular settings** in `main.cpp`:
   ```cpp
   const char apn[] = "your-carrier-apn";     // e.g., "internet"
   const char gprsUser[] = "your-username";   // Often empty
   const char gprsPass[] = "your-password";   // Often empty
   ```

### 3. Initial Configuration

1. **Build and upload** the firmware:
   ```bash
   platformio run --target upload
   ```
2. **Monitor serial output** at 115200 baud:
   ```bash
   platformio device monitor
   ```
3. **Configure via BLE** (see BLE Configuration section)

## 📱 BLE Configuration

### Connection Process

1. Device advertises as `ESP32-BLE-Example` when WiFi is disconnected
2. Connect using a BLE app (nRF Connect, BLE Scanner, etc.)
3. Find service UUID: `9379d945-8ada-41b7-b028-64a8dda4b1f8`

### Configuration Characteristics

- **Read/Write**: `c62b53d0-1848-424d-9d05-fd91e83f87a8`
- **Notifications**: `6cd49c0f-0c41-475b-afc5-5d504afca7dc`

### Configuration JSON Format

```json
{
  "deviceName": "MyESP32Device",
  "ssid": "YourWiFiNetwork",
  "password": "YourWiFiPassword",
  "restart": true
}
```

### Status Responses

- `S:WC,NR,IP:192.168.1.100` - WiFi connected successfully
- `S:WF,NR` - WiFi connection failed
- `S:SI,NR` - Settings updated (restart required)

## 🌐 HTTP API

### Endpoints

#### GET `/`

Returns HTML interface for SMS sending

```
Response: text/html
```

#### POST `/send`

Send SMS message via JSON payload

```json
Request:
{
  "phone": "+1234567890",
  "message": "Your SMS message text"
}

Response:
{
  "status": "ok"
}
```

### Error Responses

```json
{"error": "Invalid phone format. Use +1234567890"}
{"error": "Message length 1..480 required"}
{"error": "Modem not registered on network"}
{"status": "fail"}
```

### CORS Support

All endpoints support cross-origin requests:

- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: POST, GET, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type, Authorization`

## 🔧 Configuration Options

### Network Settings

```cpp
// GSM PIN (if required)
#define GSM_PIN ""

// APN Settings (carrier-specific)
const char apn[] = "internet";        // Your carrier's APN
const char gprsUser[] = "";           // Usually empty
const char gprsPass[] = "";           // Usually empty
```

### Build Options

```cpp
#define TINY_GSM_MODEM_SIM7000        // Enable SIM7000G support
#define TINY_GSM_DEBUG Serial         // Enable AT command debugging
#define TINY_GSM_RX_BUFFER 1024       // UART buffer size
#define DUMP_AT_COMMANDS              // Detailed AT command logging
```

## 📊 System Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Mobile App    │    │   Web Browser   │    │  BLE Scanner    │
│                 │    │                 │    │                 │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │ HTTP                 │ HTTP/WiFi            │ BLE
          │                      │                      │
┌─────────▼──────────────────────▼──────────────────────▼───────┐
│                     ESP32 T-SIM7000G                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐              │
│  │HTTP Server  │ │WiFi Manager │ │BLE Server   │              │
│  └─────────────┘ └─────────────┘ └─────────────┘              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐              │
│  │SMS Handler  │ │Settings Mgr │ │Modem Driver │              │
│  └─────────────┘ └─────────────┘ └─────────────┘              │
└─────────────────────────┬──────────────────────────────────────┘
                          │ AT Commands
                          ▼
                ┌─────────────────┐
                │   SIM7000G      │ ───► 📱 Cellular Network
                │   GSM Modem     │      📨 SMS Messages
                └─────────────────┘
```

## 🚨 Troubleshooting

### Common Issues

#### Modem Not Responding

```
Solution: Check SIM card insertion and antenna connection
Debug: Monitor serial output for AT command responses
```

#### Network Registration Failed

```
Solution: Verify SIM card activation and carrier compatibility
Debug: Check signal strength and try different network modes
```

#### BLE Connection Issues

```
Solution: Clear BLE cache on client device, restart ESP32
Debug: Monitor BLE advertising status in serial output
```

#### WiFi Connection Failed

```
Solution: Verify credentials, check signal strength
Debug: Use BLE interface to update WiFi settings
```

### Debug Commands

```cpp
// Enable detailed logging
#define TINY_GSM_DEBUG Serial
#define DUMP_AT_COMMANDS

// Monitor output
platformio device monitor --baud 115200
```

## 🔒 Security Considerations

### Password Storage

- WiFi passwords stored in ESP32 NVS (encrypted by hardware)
- BLE responses mask passwords: `"password": "pass****"`
- No plaintext credential logging

### Network Security

- HTTP server runs on local network only
- CORS headers allow controlled cross-origin access
- No external internet connectivity required for operation

### BLE Security

- Device stops advertising when WiFi connected (power saving)
- Configuration only possible when in setup mode
- No sensitive data transmitted over BLE without encryption

## 📈 Performance Characteristics

### Power Consumption

- **Active Operation**: ~200-300mA (GSM + WiFi + BLE)
- **Idle Mode**: ~100-150mA (GSM registered, WiFi connected)
- **Sleep Mode**: Not implemented (continuous operation design)

### Response Times

- **SMS Delivery**: 5-30 seconds (network dependent)
- **HTTP Response**: <100ms (local processing)
- **BLE Configuration**: 1-3 seconds
- **WiFi Connection**: 10-30 seconds

### Message Limits

- **SMS Length**: 160 characters (standard), 480 characters (segmented)
- **Concurrent Requests**: Single-threaded processing
- **Network Modes**: GSM/LTE fallback supported

## 🤝 Contributing

### Development Setup

1. Fork the repository
2. Create a feature branch
3. Make changes with proper documentation
4. Test on actual hardware
5. Submit pull request

### Code Style

- Doxygen-compatible documentation
- Consistent indentation (2 spaces)
- Descriptive variable names
- Error handling for all network operations

### Testing Checklist

- [ ] SMS sending functionality
- [ ] BLE configuration interface
- [ ] WiFi connectivity
- [ ] HTTP API endpoints
- [ ] Error recovery mechanisms
- [ ] Serial debug output

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙋 Support

### Documentation

- **API Reference**: See inline Doxygen documentation
- **Hardware Guide**: [LilyGO T-SIM7000G Documentation](https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G)
- **Library References**: See individual library documentation

### Community

- **Issues**: Report bugs via GitHub Issues
- **Discussions**: Use GitHub Discussions for questions
- **Pull Requests**: Contributions welcome

### Hardware Support

- **Board**: LilyGO T-SIM7000G official documentation
- **Modem**: SIMCom SIM7000G AT command reference
- **Cellular**: Contact your carrier for APN settings

---

**⚠️ Important**: This project involves cellular connectivity which may incur charges from your mobile carrier. Monitor your usage and ensure you have an appropriate data/SMS plan.
