# PicoRTK

RTK GNSS rover firmware for a **Raspberry Pi Pico 2 W** with a **Waveshare LG290P GNSS RTK module**. This is Option 2 of the RFSAT embedded RTK receiver.

The LG290P computes the RTK solution. The Pico handles everything around it:

- It fetches NTRIP corrections over Wi-Fi from up to four sources in priority order, with failover.
- It serves NMEA over **Bluetooth SPP** (Android, Windows) or **BLE UART** (iOS), **USB** and **TCP**.
- It hosts a configuration web page.

## Wiring

| Waveshare LG290P | Pico 2 W | Purpose |
|---|---|---|
| 5V | VSYS (or VBUS) | power |
| GND | GND | ground |
| RXD | GP0 (UART0 TX) | RTCM and commands into the receiver |
| TXD | GP1 (UART0 RX) | NMEA out of the receiver |
| RTK | GP2 | fix status (optional) |

**Alternative board: ZJI RF "LG290P Full Freq Dev" (AliExpress).** It carries two 6-pin rows, EN · GND · TX · RX · VCC · PPS, one row for each of the LG290P's two UARTs.

- Use the row marked TX0/RX0 (or TX1/RX1):
  - VCC → VSYS (5 V)
  - GND → GND
  - RX → GP0
  - TX → GP1
  - PPS → GP2 (optional)
- Leave EN unconnected; it is pulled up on the board.
- Before wiring, check with a meter that the TX pin idles at about 3.3 V.
- Connect the antenna through the supplied u.FL–SMA pigtail.

Optional parts:

- **LEDs on GP14 (RTK) and GP15 (corrections)**, each through a 330 Ω resistor to GND.
- **A push-button from GP16 to GND.** Hold it for 3 s to open the setup access point.

Check the pin labels against Waveshare's pinout before powering up.

## Receiver set-up (once, over the module's own USB-C, with QGNSS)

1. Update the LG290P firmware to a release with Galileo HAS.
2. Set rover mode, with all constellations and bands, and E6 on.
3. Set the outputs to GGA, RMC, GSA, GSV and GST at 1 Hz (up to 10 Hz for vehicles).
4. Set **UART1 to 460800 bit/s**, carrying NMEA out and RTCM in.
5. Enable PPP/HAS.
6. Save to flash and power-cycle to confirm the settings persist.

You can also do this through the Pico: with **USB bridge** on (the default), the Pico's USB port carries the receiver's stream, and whatever you type goes to the receiver. QGNSS can therefore connect to the Pico's COM port. Firmware updates of the LG290P still need the module's own USB-C port.

## Flashing

1. Download `PicoRTK-<version>.uf2` from the GitHub Actions run or release.
2. Hold **BOOTSEL** while plugging the Pico into USB.
3. Copy the UF2 onto the `RP2350` drive that appears. The Pico restarts by itself.

Settings live in a separate flash area and survive firmware updates.

## First configuration

1. With no known Wi-Fi, the Pico opens the access point **`PicoRTK-Setup-XXXX`**. The password is `picortk-setup`.
2. Join it and open **http://192.168.42.1**. Log in as user `admin` with password `picortk`, and change both passwords on the **Wi-Fi & outputs** tab.
3. Fill in, then press **Save and restart**:
   - your Wi-Fi networks;
   - the correction sources, in priority order (own base, Metrica VRS, Metrica nearest, EUREF);
   - the Bluetooth mode.
4. On your network, the page is at the Pico's IP address. **Backup → Download settings** saves a file you can upload to the next unit.

## Help on the web page

Every field, button, table column and status line has a short explanation. It appears when you hover with the mouse, when a field gets focus, or when you tap it on a phone. Status labels with help are underlined with dots.

## Finding the Pico / troubleshooting

- **LED:** fast blink means the setup access point is on; slow blink means the Pico has joined your Wi-Fi, so the setup network disappears (this is normal).
- **On your network:** the page is at **http://picortk.local** (the device name in lower case), or at the IP shown in your router's client list.
- **USB log:** until the receiver is connected, the Pico's USB port prints a status line every 5 s (Wi-Fi mode, network, IP, NTRIP state). Open it with any serial terminal; the speed doesn't matter. Once receiver data flows, USB carries the receiver stream instead.
- **Back to setup mode:** hold the GP16 button for 3 s; or, if the saved Wi-Fi isn't in range, the setup network returns by itself after about 15 s at power-up.

## NMEA output, monitor and shutdown

- **NMEA tab → Application profiles.** Each profile is a set of LG290P messages plus a fix rate for one kind of use:

  | Profile | Messages | Rate |
  |---|---|---|
  | Survey and GIS (SW Maps, QField, Emlid Flow) | GGA RMC GST GSA GSV | 1 Hz |
  | Static monitoring (deformation, benchmarks, averaging) | GGA GST ZDA GBS PQTMEPE PQTMDOP | 1 Hz |
  | Agriculture and machine guidance (AgOpenGPS) | GGA VTG | 10 Hz |
  | Road vehicles and fleet (tracking, road surveys, distance) | GGA RMC VTG GST PQTMVEL PQTMODO | 5 Hz |
  | Robots and drones (ROS, ArduPilot) | GGA RMC VTG GST | 10 Hz |
  | Marine and chart plotters (OpenCPN, NMEA 0183 instruments) | GGA RMC GLL VTG GSA GSV ZDA | 1 Hz |
  | Time synchronisation (gpsd/chrony, loggers; use PPS) | GGA RMC ZDA | 1 Hz |
  | Minimal (BLE, slow links) | GGA RMC | 1 Hz |
  | Research and integrity | GGA RMC GST GSA GSV GBS PQTMNAV PQTMEPE PQTMDOP PQTMPL PQTMVEL PQTMPVT | 1 Hz |

- **Message buttons:** all 20 output messages of the LG290P (03) that `$PQTMCFGMSGRATE` can switch, grouped as standard NMEA and Quectel proprietary. Hover or tap for what each carries. HDT, THS and PQTMTAR (heading) are not offered: they need the dual-antenna LG580P.
- **Fix rate** 1, 2, 5 or 10 Hz, read from the receiver when the tab opens. GSA and GSV always come at 1 Hz. The page estimates the output in kB/s and says whether it suits BLE (about 2 kB/s), SPP or TCP.
- **Apply to receiver** switches the selected messages on and the other listed ones off on the Pico's UART (GGA always stays on; the Pico needs it), sets the fix rate, enables the odometer when PQTMODO is selected (from 0 m), saves (`$PQTMSAVEPAR`) and restarts the receiver (`$PQTMSRR`). The receiver's replies are listed with an OK count, so a rejected setting is visible.
- **Save and restart** stores the selection in the Pico, which then sends only those types to Bluetooth and TCP. With none selected, everything is sent. USB always carries the full receiver stream.
- **Live output** shows exactly what goes to Bluetooth and TCP, refreshed every second; it can be paused.
- **System → Shut down** (asks for confirmation; so do Restart, Save and restart, and Upload settings):
  - It closes the caster connection, stops Bluetooth, TCP and the receiver link, turns the radio off and halts.
  - Unplug once the LEDs are off.
  - To start again, power it up or briefly short RUN (pin 30) to GND.

## Correction-source failover

| Event on the active source | Action |
|---|---|
| Cannot connect, no reply in 10 s, 5xx / banned / sandbox reply, link broken, no valid RTCM frame for `dataTimeoutSec` (30 s), or correction age in GGA above `maxAgeSec` (10 s) for 10 s while in RTK | Moves to the next source after `retryMs` (2 s) |
| 401/403 (login), SOURCETABLE or 404 (mount point), other 4xx | Disables that source until settings change or restart, then moves on |
| Every usable source has failed in a row | Waits per the back-off steps (15 s, 30 s, 1 min, 2 min, 5 min), then starts again at source 1 |
| Every source is disabled | Stops. The web page shows why. |
| Running on a lower source for `failbackSec` (600 s) | Returns to the highest-priority source |
| Wi-Fi lost | Keeps the current source, waits for Wi-Fi. The receiver uses Galileo HAS meanwhile. |

**Sources with "Send position (GGA)" on (VRS, nearest station):**

- The Pico waits up to 60 s for a fix before connecting.
- It then sends the receiver's GGA every `ggaSec`.

**Bench position (Corrections tab):** VRS and nearest-station mount points (Metrica MSM_VRS, MSM_NEAR) choose what to send from the rover's GGA. Until they get one, they answer "200 OK" but send nothing. Without a receiver, or before its first fix, enter an approximate latitude, longitude and altitude. The Pico then sends that as GGA, using NTP time, and the stream starts. Once the receiver has a fix, its own GGA is used instead. Set it back to 0 to turn it off.

**RTCM checks and inbound corrections:**

- Every RTCM frame is CRC-checked before it reaches the receiver.
- When NTRIP is not streaming, RTCM sent by a Bluetooth or TCP app (one running its own NTRIP client) is passed to the receiver as well.

## Architecture

| Core | Work |
|---|---|
| 0 | Wi-Fi manager, NTRIP client and source selector, Bluetooth/TCP outputs, web page, LEDs |
| 1 | Receiver UART (460800, 8 KB FIFO), USB bridge, NMEA validation and GGA extraction |

The cores exchange data through lock-free single-producer/single-consumer rings (RTCM to the receiver, NMEA to the network) and a mutex-protected copy of the latest GGA.

| File | Contents |
|---|---|
| `firmware/PicoRTK/PicoRTK.ino` | start-up, core loops |
| `receiver.*` | core 1: UART, USB bridge |
| `ntrip.*` | NTRIP client state machine |
| `selector.*` | failover policy (portable, unit-tested) |
| `ntrip_proto.*` | request, reply parsing, chunked decoding (portable, unit-tested) |
| `rtcm.*`, `nmea.*`, `ring.h` | framing, checksums, queues (portable, unit-tested) |
| `outputs.*` | SPP, BLE, TCP |
| `net.*`, `web.*`, `status.*`, `config.*` | Wi-Fi, web API, LEDs and button, settings JSON |
| `firmware/web/index.html` | configuration page, embedded at build time by `tools/embed_web.py` |
| `tools/fake_caster.py` | bench caster with GOOD / STALL / DROP / AUTH mount points for failover tests |

## Building

```
arduino-cli core install rp2040:rp2040@6.3.0     # board manager URL: earlephilhower/arduino-pico
arduino-cli lib install ArduinoJson@7.4.2
./tools/build.sh                                  # -> build/PicoRTK.ino.uf2
```

The board options are Pico 2 W, **IPv4 + Bluetooth**, and 4 MB flash with 1 MB for the file system. CI (`.github/workflows/build.yml`) runs:

- the host unit tests;
- the protocol test against the fake caster;
- the UF2 build. Tags `v*` publish a release.

## Bench test with the fake caster

```
python3 tools/fake_caster.py --port 2101
```

Set the sources to `<PC IP>:2101`, with source 1 on `/STALL`, source 2 on `/AUTH` (no login) and source 3 on `/GOOD`. The expected event log:

1. source 1 fails with "no RTCM data" after 30 s;
2. source 2 fails with "login rejected (401)" and is disabled;
3. source 3 streams.

`/DROP` closes the connection after 20 s.

## Known limits (v0.1)

- **SPP + BLE together is experimental.** Two Arduino-Pico Bluetooth libraries share one BTstack. Use SPP for Android and BLE for iOS.
- **Galileo HAS** is handled by the LG290P itself. Whether it falls back automatically when RTCM stops is to be confirmed on the bench.
- **Receiver configuration** at start-up is limited to the raw `bootCmds` you enter. The Pico does not otherwise configure the LG290P.
- **The Extended variant (LTE-M via SIM7080G) is not in this version.**
