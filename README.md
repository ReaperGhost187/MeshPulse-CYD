# MeshPulse CYD

MeshPulse turns an ESP32-S3 Cheap Yellow Display (CYD) into a compact, always-on monitor for MeshCore telemetry delivered over MQTT. It presents repeater connectivity, packet activity, RF signal data, queue depth, IP address, and uptime in a CoreScope-inspired dark dashboard.

![Build target](https://img.shields.io/badge/target-ESP32--S3-blue)
![Display](https://img.shields.io/badge/display-320x240%20CYD-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

- MQTT and repeater online indicators
- RX, TX, and total packet counters
- Last-packet age
- RSSI, SNR, noise-floor, and queue telemetry
- Repeater name and uptime
- Automatic Wi-Fi and MQTT reconnection
- Project-local TFT_eSPI build options; no edits inside the library directory
- Support for USB-C ILI9341 and dual-USB ST7789 CYD variants

## Hardware target

The tested target is a CYD-class ESP32-S3 board with an 8 MB flash chip and a 2.8-inch 320x240 display.

| Function | GPIO |
|---|---:|
| Display MOSI | 13 |
| Display MISO | 12 |
| Display SCLK | 14 |
| Display CS | 15 |
| Display DC | 2 |
| Display reset | -1 (shared reset) |
| Backlight | 21 |
| Touch CS | 33 |

The display and touch controllers use separate SPI buses. `MeshPulse_CYD/build_opt.h` applies the TFT_eSPI configuration to the sketch and every compiled library source file.

## Requirements

Install the following in Arduino IDE or with Arduino CLI:

- **esp32 3.3.10 by Espressif Systems**
- **TFT_eSPI 2.5.43 by Bodmer**
- **PubSubClient 2.8 by Nick O'Leary**
- **ArduinoJson 7.4.3 by Benoit Blanchon**

These are the verified versions used by CI. Later compatible versions may also work.

## Configuration

1. Copy the credentials template:

   ```text
   MeshPulse_CYD/secrets.h.example -> MeshPulse_CYD/secrets.h
   ```

2. Edit `secrets.h` with the Wi-Fi and MQTT connection settings. This file is excluded by `.gitignore`.
3. Edit `MeshPulse_CYD/project_config.h` and set:
   - `MESHPULSE_HEADER_LABEL` to the short label shown in the header.
   - `MESHCORE_REGION` to the region segment used in your MQTT topics.
   - `MESHCORE_REPEATER` to the exact repeater identifier used in your MQTT topics.
4. The default `build_opt.h` targets a USB-C ILI9341 board. For a dual-USB CYD using an ST7789 display, replace it with `build_opt.dual-usb.h`:

   ```text
   build_opt.dual-usb.h -> build_opt.h
   ```

## MQTT topics

For region `ABC` and repeater `repeater-id`, MeshPulse subscribes to:

```text
meshcore/ABC/repeater-id/status
meshcore/ABC/repeater-id/packets
```

Each display monitors one configured repeater so status and packet metrics cannot be mixed between devices.

### Status payload

```json
{
  "status": "online",
  "origin": "Repeater name",
  "stats": {
    "noise_floor": -118,
    "queue_len": 0,
    "uptime_secs": 3600
  }
}
```

### Packet payload

```json
{
  "direction": "rx",
  "RSSI": -96,
  "SNR": 7.5
}
```

`direction` can be `rx` or `tx`. RSSI and SNR may be JSON numbers or numeric strings.

## Build with Arduino IDE

1. Open `MeshPulse_CYD/MeshPulse_CYD.ino`.
2. Select **ESP32S3 Dev Module**.
3. Select the correct serial port.
4. Recommended settings: 8 MB flash, QIO, 240 MHz, USB CDC On Boot enabled.
5. Verify and upload.

On startup, the display briefly flashes red, green, and blue before opening the dashboard. Serial diagnostics use 115200 baud.

## Build with Arduino CLI

From the repository root:

```sh
cp MeshPulse_CYD/secrets.h.example MeshPulse_CYD/secrets.h
arduino-cli compile --fqbn esp32:esp32:esp32s3 MeshPulse_CYD
```

Replace the template values before uploading to hardware. The GitHub Actions workflow performs the same clean compile with placeholder credentials.

## Troubleshooting

- **Blank white display:** confirm that `build_opt.h` matches the display controller on your board.
- **No serial output:** confirm the ESP32-S3 board selection and use 115200 baud.
- **Wi-Fi never connects:** check `secrets.h`; the example values are intentionally nonfunctional.
- **MQTT connects but no data appears:** confirm the region and topic hierarchy match your bridge, then inspect messages with an MQTT client.
- **Incorrect colors:** verify the display variant and driver selection in `build_opt.h`.

## Security

Never commit `MeshPulse_CYD/secrets.h`. If credentials are accidentally published, rotate the Wi-Fi and MQTT credentials immediately; deleting a later commit does not remove them from Git history.

## License

MIT — see [LICENSE](LICENSE).
