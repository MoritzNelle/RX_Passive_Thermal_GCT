# RX Passive Thermal GCT

Firmware for the target units of a set of passive thermal ground control targets (GCTs). The targets are placed on the ground in the flight area and are used as temperature references when thermal drone images are calibrated. Each target is an ESP32 with nine DS18B20 temperature sensors, a real-time clock and an SD card. When the master station asks for data, the target reads all nine sensors and sends the values back over ESP-NOW. While logging is on, it also writes them to its own SD card.

The master station firmware is in [TX_Passive_Thermal_GCT](https://github.com/MoritzNelle/TX_Passive_Thermal_GCT). In the code the targets are also called servants ("servent").

## Why

The usual way to calibrate thermal drone images is with aluminium sheets on the ground and a handheld thermal gun. That method is not very accurate, and the temperature of the references drifts during a flight. These targets measure and timestamp their own temperature for as long as the master is logging, so any drift during the flight is recorded.

## Hardware

What the code and `platformio.ini` use:

- ESP32 board (PlatformIO board `nodemcu-32s`)
- 9 DS18B20 temperature sensors on one OneWire bus, read at 12-bit resolution
- DS3231 real-time clock on I2C
- SD card module on SPI
- One NeoPixel RGB LED for status

The firmware expects all nine sensors. If one of them does not answer at start-up, the LED blinks red and the unit waits.

## Pins

| Function | GPIO |
|---|---|
| OneWire bus (DS18B20 data) | 4 |
| NeoPixel status LED | 2 |
| SD card CS | 5 |
| SD card SCK | 18 |
| SD card MISO | 19 |
| SD card MOSI | 23 |
| RTC SDA | 21 |
| RTC SCL | 22 |

The SPI pins are the ESP32 defaults. `SCK_PIN`, `MISO_PIN` and `MOSI_PIN` in `main.cpp` are only there for reference and are not passed to the SD library. The RTC pins appear as a comment in the code and match the ESP32 default I2C pins.

## How it works

The target has no timer of its own for readings. It waits for messages from the master and acts on the action ID at the start of each one.

| Action ID | Direction | Meaning |
|---|---|---|
| 1001 | Master to target | Connection test. Resets the target's connection timer. |
| 1002 | Master to target | Logging is on. The target saves every reading to its SD card. |
| 1003 | Master to target | Logging is off. |
| 3001 | Master to target | Request all temperatures. |
| 2001 | Target to master | Answer with all nine temperatures. |

The answer is a single ESP-NOW packet with this struct. It has to be the same in both firmwares:

```cpp
typedef struct temp {
  int actionID;   // 2001
  float sens1;
  float sens2;
  // ...
  float sens9;
} temp;
```

`doc/Action_IDs.txt` is an older planning table. It lists a hard reset (8362) and config values (100X) that are not implemented. Logging on and off now uses 1002 and 1003 instead of the value sent with 3001.

## Build and flash

This is a PlatformIO project with the Arduino framework. On the first build PlatformIO downloads the ESP32 platform and these libraries:

- OneWire (paulstoffregen)
- DallasTemperature (milesburton)
- RTClib (Adafruit)
- Adafruit NeoPixel
- SPI

In VS Code with the PlatformIO extension, open the folder and use Build and Upload. On the command line:

```sh
pio run                        # build
pio run -t upload              # build and flash
pio device monitor -b 115200   # serial monitor
```

The only environment is `az-delivery-devkit-v4`. `platformio.ini` does not set a monitor speed, so pass `-b 115200` to read the self check on the serial monitor.

If your computer does not find the board, install the CP210x USB to UART driver from `board_driver/` (Windows and macOS). Linux already includes it.

## Settings

The user settings are near the top of `src/main.cpp`, under `USER VARIABLES`.

| Variable | Default | What it does |
|---|---|---|
| `GCTID` | `1` | Number of this target, 1 to 4. It goes into every log line. |
| `fileName` | `"/data_GCT1.csv"` | Log file name on the SD card. See the note below. |
| `pingInterval` | `1000` | Time in ms the target expects between two connection tests (500 or more). Use the same value as `pingCheckIntervall` on the master. |
| `masterAdress` | `{0x48, 0xE7, 0x29, 0x8C, 0x6B, 0x5C}` | MAC address of your master station. |

`setup()` copies `"/data_GCT1.csv"` into `fileName` again before it opens the file. Every target writes to `/data_GCT1.csv` unless you change that line too.

The master only knows each target by its MAC address. The target's position in the master's `broadcastAddresses` list (1 to 4) is the number the master writes to its own log, so give each target the matching `GCTID`. esptool prints the board's MAC address near the start of every upload, so you can copy it from the upload output.

How often the target is read is set on the master (`logIntervall` and `tempUpdateIntervall` in the TX firmware).

### Setting the clock

The log uses the time from the DS3231. To set it to your computer's time, uncomment this line in `setup()` and flash:

```cpp
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
```

Then comment it out again and flash once more. If you leave it in, the clock goes back to the compile time at every restart. When the RTC reports a year before 2024, the firmware prints a warning on the serial monitor.

## Status LED

| LED | Meaning |
|---|---|
| Yellow, steady | Self check running |
| Red, blinking | A sensor is missing at start-up, or the SD card can't be mounted or written to |
| Red, steady | RTC not found. The unit stops. |
| Yellow, blinking | No connection test from the master for more than `pingInterval` + 2 s |
| Green, blinking | Connected, not logging |
| Green, steady | Connected and logging |

## Log file

While logging is on, the target appends to its log file every time it answers a 3001 request. One answer adds nine lines, one per sensor. The file has no header row.

```
timestamp,GCTID,sensor,temperature
```

| Column | Content |
|---|---|
| `timestamp` | Time from the target's RTC, `YYYY-MM-DD HH:MM:SS` |
| `GCTID` | The value set in the code |
| `sensor` | 1 to 9 |
| `temperature` | °C with two decimals. -127.00 means the sensor did not answer (the DallasTemperature value for a disconnected sensor). |

Example lines (made-up values):

```
2024-06-12 10:15:00,1,1,24.31
2024-06-12 10:15:00,1,2,24.25
```

Sensor numbers follow the order in which the DallasTemperature library finds the sensors on the bus (`getTempCByIndex`). They are not tied to a position on the target.

The master writes the same readings to its own SD card, with its own RTC time, so every reading is stored twice.

## Known issues

From the TODO and MARK comments in `src/main.cpp`:

- The fast red blinking that should warn about a clock set before 2024 (LED status 7) does not work.
- After an SD write error the unit stops and blinks red, but according to a comment the LED can still show the ready state now and then.
- `fileName` is overwritten in `setup()`, see Settings.
