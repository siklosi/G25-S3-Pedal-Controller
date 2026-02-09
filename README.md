# G25 S3 Pedal Controller

<p align="center">
<img width="942" height="898" alt="Screenshot" src="https://github.com/user-attachments/assets/55dc657b-81f2-4e87-99bd-e6b90304eda6" />
![20260209_202703](https://github.com/user-attachments/assets/4f3992d7-f7ac-43d9-b6e1-bf7186fe9088)

</p>
A modern ESP32-S3 based USB game controller for Logitech G25 pedals featuring web-based configuration, custom response curves, and profile management.

## ✨ Features

- **USB HID Game Controller** - Works as a native joystick with 3 axes (Gas/Brake/Clutch)
- **Web-Based Configuration** - Modern, responsive interface accessible via WiFi
- **Real-Time Monitoring** - Live pedal position visualization with raw and processed values
- **Live Configuration** - All settings (deadzones, curves, smoothing) are applied instantly in RAM for real-time tuning
- **Persistent Storage** - Explicit "Save to Flash" required to store settings permanently across reboots
- **Custom Response Curves** - 11-point adjustable curves with visual editor
- **Curve Presets** - Linear, Progressive, S-Shape, and Aggressive profiles
- **Custom Curve Slots** - Save up to 4 custom curves per pedal
- **Profile Management** - Save/load complete configurations with device storage
- **Profile Import/Export** - Backup configurations to PC as JSON files
- **Advanced Calibration** - Min/max calibration, deadzone control, axis inversion
- **Smoothing & Limiting** - Configurable output smoothing and ceiling adjustments
- **WiFiManager** - Easy WiFi setup with captive portal
- **WebSocket Live Data** - 20Hz real-time pedal data streaming

## 🛠️ Hardware Requirements

### ESP32-S3 Board
- **Board**: ESP32-S3-DevKitC-1 (or compatible)
- **Flash**: 8MB minimum (N8 or N16 variants) with custom partition table
- **PSRAM**: Optional (not required for this project)
- **USB**: Native USB support required

### Hall Effect Sensors
- **Type**: Analog hall effect sensors (e.g., SS49E, A3144, or similar)
- **Quantity**: 3 sensors (one per pedal)
- **Output**: Analog voltage output (0-3.3V)

### Wiring
Connect hall sensors to the following GPIO pins:

| Pedal   | GPIO Pin | Color Code (Typical) |
|---------|----------|----------------------|
| Throttle| GPIO 4   | Green                |
| Brake   | GPIO 5   | Red                  |
| Clutch  | GPIO 6   | Blue                 |

**Power Supply:**
- Sensor VCC → 3.3V
- Sensor GND → GND
- Sensor OUT → GPIO pins above

## 📋 Software Requirements

### PlatformIO
This project uses PlatformIO for building and uploading firmware.

**Install PlatformIO:**
```bash
# VS Code Extension (recommended)
# Install PlatformIO IDE extension from VS Code marketplace

# Or CLI
pip install platformio
```

### Dependencies
All dependencies are managed automatically via `platformio.ini`:
- ArduinoJson 6.21.3
- WebSockets 2.4.1
- WiFiManager 2.0.17
- Joystick_ESP32S2 0.9.4

## 🚀 Installation

### 1. Clone Repository
```bash
git clone https://github.com/yourusername/g25-s3-pedals.git
cd g25-s3-pedals
```

### 2. Upload Filesystem (Web Interface)
```bash
# Upload LittleFS filesystem containing web interface
pio run --target uploadfs
```

### 3. Build and Upload Firmware
```bash
# Build and upload firmware
pio run --target upload
```

### 4. Monitor Serial Output
```bash
# View serial output for debugging
pio device monitor
```

## 📡 WiFi Setup

### First-Time Setup
1. **Power on the ESP32-S3** - It will create a WiFi access point
2. **Connect to AP** - Network name: `G25_S3_Pedals`
3. **Configure WiFi** - Your device should open a captive portal automatically
   - If not, navigate to `192.168.4.1`
4. **Enter credentials** - Select your WiFi network and enter password
5. **Connect** - Device will restart and connect to your network

### Finding Device IP
Check serial monitor output after boot:
```
WiFi Connected!
IP: 192.168.1.xxx
```

Or check your router's DHCP client list for "G25_S3_Pedals"

## 🎮 Usage

### Accessing Web Interface

1. Open browser and navigate to: `http://[ESP32_IP_ADDRESS]`
2. You should see the G25 S3 configuration interface
3. WebSocket connection indicator should show "Connected" (green dot)

### Calibration

#### Quick Calibration
1. **Release all pedals** completely
2. Click **Min** button for each pedal
3. **Press each pedal fully**
4. Click **Max** button for each pedal
5. **IMPORTANT:** Click **Save All to Flash** to perform a permanent save. Live changes are RAM-only.

#### Advanced Options
- **Inverted** - Reverse pedal direction if sensor is mounted backwards
- **Deadzones** - Set start/end deadzones to eliminate jitter
  - *Start*: Adds margin to Min value (ignores initial press)
  - *End*: Subtracts margin from Max value (reaches 100% earlier)
- **Smoothing** - Apply exponential smoothing (0-95%)
- **Output Limit** - Cap maximum output (useful for brake pedal)

### Response Curves

#### Curve Editor
- Click directly on the chart to adjust curve points
- 11 adjustable points from 0% to 100% input
- Real-time preview with live pedal position indicator (red dot) that follows the curve

#### Presets
- **Linear** - 1:1 response (default)
- **Progressive** - Gradual increase, sensitive at start
- **S-Shape** - Smooth acceleration/deceleration
- **Aggressive** - Quick response, less precision

#### Custom Curves
1. Adjust curve to your preference
2. **Hold Shift + Click** "Custom X" button to save
3. Click button (without Shift) to load saved curve
4. Up to 4 custom curves per pedal

### Profile Management

#### Device Profiles (Stored on ESP32)
- **Save** - Enter name and click "Save" to store current configuration
- **Load** - Select profile from list and click "Load Selected"
- **Delete** - Select profile and click "Delete"

#### PC Backup/Restore
- **Export File** - Download current configuration as JSON
- **Import File** - Upload previously exported configuration

### Testing in Games

1. **Windows**: Device appears as "Custom Sim - G25 S3 Pedals"
2. **Check Device Manager** → Human Interface Devices
3. **Test in Game Controller Settings**:
   - Right-click Start → Settings → Devices → Printers & Scanners
   - Or search "Set up USB game controllers"
4. **Verify axes**:
   - X-Axis: Throttle
   - Y-Axis: Brake
   - Z-Axis: Clutch

## 🔧 Configuration Details

### Curve Format
Curves are stored as 11-point arrays representing output values (0-100%) at input positions (0%, 10%, 20%...100%).

Example linear curve:
```json
[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
```

Example progressive curve:
```json
[0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100]
```

### Configuration File Structure
```json
{
  "gas": {
    "min": 200,
    "max": 3900,
    "dzStart": 10,
    "dzEnd": 10,
    "inverted": false,
    "smooth": 20,
    "ceil": 100,
    "curve": [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
  },
  "brake": { ... },
  "clutch": { ... },
  "customs": [
    [0, 10, 20, ...],  // Custom slot 1
    [0, 5, 15, ...],   // Custom slot 2
    [0, 10, 20, ...],  // Custom slot 3
    [0, 10, 20, ...]   // Custom slot 4
  ]
}
```

### File Locations
- **Active Config**: `/config.json`
- **Web Interface**: `/index.html`
- **Profiles**: `/profiles/*.json`

## 🐛 Troubleshooting

### Device Not Recognized by PC
- **Check USB cable** - Ensure data lines are connected (not charge-only cable)
- **Driver issues** - Windows should auto-install HID drivers
- **Serial output** - Check for "USB Initialized" message
- **Try different port** - Some USB ports may have compatibility issues

### WiFi Connection Issues
- **Reset WiFi settings** - Uncomment `wm.resetSettings()` in code, upload, then comment out again
- **Check serial monitor** - Look for IP address assignment
- **AP mode** - If connection fails, device creates "G25_S3_Pedals" AP

### Web Interface Not Loading
- **Filesystem upload** - Ensure you ran `pio run --target uploadfs`
- **Browser cache** - Try hard refresh (Ctrl+F5) or incognito mode
- **IP address** - Verify correct IP from serial monitor
- **Check LittleFS** - Files should be in `data/` folder before upload

### Erratic Pedal Readings
- **Calibration** - Recalibrate min/max values
- **Wiring** - Check for loose connections or electrical noise
- **Smoothing** - Increase smoothing value (30-50%)
- **Deadzones** - Add small deadzones (5-15) to eliminate jitter

### Configuration Not Saving
- **Flash space** - Ensure 8MB flash is properly configured
- **Serial output** - Look for "Saved!" messages
- **LittleFS** - Check if filesystem initialized correctly
- **Power cycle** - Restart device to verify persistence

## 🔄 Updating Firmware

### Preserve Configuration
Configurations are stored in LittleFS and survive firmware updates (but not filesystem uploads).

```bash
# Update firmware only (keeps config)
pio run --target upload

# Update both firmware and filesystem (erases config)
pio run --target uploadfs
pio run --target upload
```

**Backup before filesystem update:**
1. Export profiles using "Export File" feature
2. Upload new filesystem
3. Import profiles back

## 📊 Technical Specifications

- **ADC Resolution**: 12-bit (0-4095)
- **Output Resolution**: 12-bit (0-4095)
- **Sampling Rate**: ~1000Hz per sensor
- **WebSocket Update Rate**: 20Hz
- **Web Server**: HTTP on port 80
- **WebSocket**: WS on port 81
- **USB VID**: 0x1209
- **USB PID**: 0xABCD

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **WiFiManager** by tzapu - Simplified WiFi configuration
- **ArduinoJson** by Benoit Blanchon - JSON parsing/serialization
- **WebSockets** by Links2004 - Real-time communication
- **Joystick_ESP32S2** by schnoog - USB HID implementation
- **Chart.js** - Interactive curve visualization




---

**Made with ❤️ for sim racing enthusiasts**
