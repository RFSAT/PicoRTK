# PicoRTK

RTK GNSS rover firmware for a **Raspberry Pi Pico 2 W** with a **Waveshare LG290P GNSS RTK module**. This is Option 2 of the RFSAT embedded RTK receiver.

The LG290P computes the RTK solution. The Pico handles everything around it:

- It fetches NTRIP corrections over Wi-Fi from up to four sources in priority order, with failover.
- It serves NMEA over **Bluetooth SPP** (Android, Windows) or **BLE UART** (iOS), **USB** and **TCP**.
- It hosts a configuration web page.

## Wiring

The Waveshare board (33 × 33 mm) has 9 castellated pads on each side. Use **UART3**: its default protocol setting is NMEA and RTCM3 both in and out, so one UART carries corrections in, NMEA out, and the replies to the NMEA-tab commands. All the Pico pins used are at its USB end.

| Waveshare LG290P pad | Pico 2 W pin | Purpose |
|---|---|---|
| 5V (right edge) | 40 VBUS | power (5 V from the Pico's USB) |
| GND (right edge) | 38 GND (or 3) | ground |
| RXD3 (right edge) | 1 GP0 (UART0 TX) | RTCM and commands into the receiver |
| TXD3 (right edge) | 2 GP1 (UART0 RX) | NMEA and replies out of the receiver |
| RTK (left edge) | 4 GP2 | fix status (optional) |
| PPS (right edge) | 5 GP3 | 1PPS (optional, not used by the firmware yet) |
| RST (right edge) | 6 GP4 | receiver reset (optional, not used by the firmware yet) |

- Both boards use 3.3 V logic, so the signals connect directly.
- Do not connect any 3V3 pad or the 3V3 pin of the 4-pin UART3 connector to the Pico's 3V3: the board makes its own 3.3 V from 5 V, and two regulators must not be tied together.
- The 4-pin UART3 connector (GND, 3V3, TXD3, RXD3) suits bench wiring. In the box, wires soldered to the pads are smaller.
- The I2C connector (SDA, SCL) is not used; Waveshare lists I2C as not yet supported.

**Prototype assembly (current):** no soldering on the GNSS board.

- Join the boards with the supplied SH1.0 4-pin cable on the LG290P's UART socket: yellow RXD3 → GP0 (pin 1), orange TXD3 → GP1 (pin 2), black GND → pin 3, and red 3V3 → 3V3 OUT (pin 36). Check that orange sits next to red at the socket (printed order GND · 3V3 · TXD3 · RXD3).
  - The socket's 3V3 pin is the board's own 3.3 V rail (Waveshare schematic), so this one cable powers the GNSS board too.
  - While the cable is connected, leave the GNSS board's USB-C empty, so that two regulators never share the rail.
- Stick the boards back to back with 1–2 mm foam tape:
  - the Pico components up, micro-USB on the left;
  - the LG290P components down, its SMA socket pointing away from you, so its UART socket is in the near-left corner under Pico pins 1–3;
  - the GNSS board's edge flush with the Pico's USB end;
  - the Pico's antenna end overhanging.
- The SMA socket goes through the case wall and the ANN-MB2 screws straight onto it.
- The coin cell goes in the RTC socket; it is charged by the board.
- The stack is about 51 × 33 × 12 mm.

**Compact assembly (later, soldered):** as below, with 5V/GND/RXD3/TXD3 wired to the pads.

- Mount the two boards back to back, with insulating tape and a thin foam pad between them. The LG290P board covers the USB half of the Pico. Its right edge (5V, GND, RXD3, TXD3, RST, PPS) lies across the Pico's USB end, so every wire is under 15 mm. The stack is about 51 × 33 × 11 mm.
- Keep the Pico's antenna end (the 18 mm opposite the USB) uncovered and away from metal: that is the Wi-Fi and Bluetooth antenna.
- The LG290P board's edge-mounted SMA socket goes out through the box wall; the ANN-MB2 screws straight onto it. Use a plastic box.

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
4. Check that **UART3** (the one wired to the Pico) runs at **460800 bit/s** with NMEA out and RTCM3 in (the default: `$PQTMCFGUART,R,3` and `$PQTMCFGPROT,R,1,3`).
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

## Correction modes: RTK or Galileo HAS

The **Corrections** switch on the Home page changes the mode at once, without a restart, and the choice is saved.

- **RTK via NTRIP:** corrections from the sources on the Corrections tab, at centimetre level once fixed. With **Galileo HAS fallback** ticked (the default), the receiver falls back to HAS if the corrections stop.
- **Galileo HAS:** the receiver uses the Galileo High Accuracy Service broadcast on E6, with no internet needed. Expect about 20 cm horizontal after several minutes of convergence.
  - NTRIP and RTCM from Bluetooth/TCP apps are off in this mode.
  - Wi-Fi stays on for the web page.
  - While the HAS solution is active, GGA reports reference station 9002, and the page shows "Galileo HAS".

**Accuracy and HAS status on the Home page:**

- The Position tile shows the receiver's own error estimate ("Accuracy (estimated)", horizontal and vertical, from PQTMEPE or GST).
- It also shows the Galileo HAS engine's state (PQTMPPPNAV): converging or converged, accuracy and correction age.
- The Pico turns these three messages on at the UART at every start-up. The NMEA tab filter still decides whether they reach Bluetooth/TCP.

**Receiver replies:** the Receiver tab lists the answers to commands (OK, ERROR, version), so they are not lost in the busy NMEA stream.

HAS is set in the receiver with `$PQTMCFGPPP,W,2,1,120,0.10,0.15` (E6 HAS, WGS84, 120 s timeout, converged at 0.10 m horizontal and 0.15 m vertical). It is sent at every start-up and on every mode change; turning the fallback off sends `$PQTMCFGPPP,W,0`. HAS needs LG290P firmware with PPP support. If the receiver answers `$PQTMCFGPPP,ERROR`, which shows in the NMEA live view, update it with QGNSS.

### Methods tab: methods, corrections, reference point, movement

The LG290P computes **one** position at a time. It cannot run SBAS and Galileo HAS side by side: once HAS corrections arrive, its GGA carries the HAS solution, converging or not (the protocol has no option to keep SBAS meanwhile). The **Methods** tab therefore shows the methods over time:

- One row per method group: RTK (RTCM), Galileo HAS, SBAS (EGNOS), GNSS only. The running one is marked ●; the others keep their last figures, with how long they ran and how long ago they stopped.
- **Estimated H · V** is the receiver's own figure (PQTMPPPNAV for HAS, otherwise PQTMEPE or GST). **Actual error H · V** is the distance from the reference point.
- **Reference point:** press *Set reference from RTK fix* while the receiver has RTK fixed (the button is disabled otherwise), then leave the antenna where it is. It is saved; *Clear reference* removes it. The API also accepts `force` for a point taken from any fix (`POST /api/ref` with `set`, `force` or `clear`).
- **Accuracy over time:** drawn from the stored history, so it survives restarts and firmware updates. Spans of 30 min, 2 h (default), 6 h or 24 h, on UTC clock time; estimate (solid) and actual error (dashed) on a background coloured by method, hatched where there is no data (off or restarting). Long spans are thinned to at most 720 points (`GET /api/trace?span=seconds`). Until the receiver has given a date, it shows this start-up's samples instead.
- **Corrections** for each method: RTK and DGNSS show the base station, the age of the corrections and whether they are **live** (RTCM reached the receiver in the last 5 s) or **stale** (RTCM has stopped; the receiver keeps using the last data for up to about 2 minutes by default, `PQTMCFGRTK` timeout). HAS shows the age of its E6 corrections, SBAS the EGNOS satellite (PRN). The Home page says "RTK fixed (stale corrections)" in that case.
- **Speed and movement** from RMC: moving above 0.5 m/s for 3 s, stationary below 0.3 m/s for 10 s, so a stop at traffic lights does not flip it.
- Positions go to the apps **exactly as the receiver computes them**: nothing is held or replaced, so tests show real behaviour. The switching between methods is the receiver's own.

### Sky tab

A sky plot and the signal strength of every satellite and band, from the receiver's GSV (in view, elevation, azimuth, C/N0 per signal) and GSA (used in the position) messages, refreshed every 2 s (`GET /api/sky`).

- **Sky plot:** north up, zenith in the centre, rings at 0°, 30° and 60° elevation. Colours by system as in PocketGNSS; filled = used in the position, ring = not used, outer ring = tracked on more than one band.
- **Bands:** for each system and band (L1/L2/L5, G1/G2, E1/E6/E5b/E5a, B1I/B3I/B2a …), the satellites tracked out of those in view, mean and best C/N0. The Galileo E6 row is highlighted with a note: HAS is sent on E6, so this shows directly whether the antenna receives it.
- **Signal strength:** one group of bars per satellite, one bar per band, highest frequency first; faded when not used.
- GSV and GSA must be on in the receiver (NMEA tab); the tab says so when they are missing.
- **No lock yet:** a satellite that loses lock stays on the plot, grey, for up to 10 minutes. The live sky is also saved to flash with every history write (`/sky.bin`), so after a restart the plot starts with the previous run's satellites, grey and dashed, with the time they were taken; each is replaced as the receiver locks on, and the rest disappear 3 minutes after live data starts. They have moved since (about 0.5° a minute for GPS), so they show roughly where to expect satellites, not where they are.

### History in flash

The Methods tab also keeps a **history**: one record every 10 s with the UTC date and time, uptime, method, corrections (live / stale / none), correction age, base station or EGNOS PRN, estimated and actual error, position, speed, moving, satellites and HDOP.

- Stored in the Pico's flash (`/hist2a.bin`, `/hist2b.bin`, 72 bytes a record, up to 768 KB, about 30 hours). History in the earlier format is deleted when this version starts. When full, the oldest records go first.
- Written every 5 minutes and at *Shut down* / restart from the page (including the records still on their way from core 1), so pulling the power loses at most the last 5 minutes. Each start-up adds a `restart` line.
- **Download CSV** (`GET /api/history.csv`) sends everything, oldest first. While it is sent (a few seconds), the correction stream and Bluetooth/TCP pause. **Clear history** (`POST /api/history/clear`) deletes it. `GET /api/history` returns the record count and time span.
- Flashing a new UF2 keeps the history, like the settings.

### Restart recorder

Each processor core writes a checkpoint number as it works (one RAM write per step). The numbers survive a restart, and a hard fault also records the code address. After a crash or a watchdog restart, the next start reports where the previous run stopped:

- **System tab → Last restart**, and `previousRun` / `previousRunProblem` in `/api/status`, e.g. "hung, restarted by the watchdog; core 0 at 3 web server, core 1 at 35 waiting for shared data".
- On USB once at start-up (`[boot] previous run: ...`), before the receiver stream starts.
- Restart and Shut down from the page are recorded as a normal end. A crash address maps to a source line with `arm-none-eabi-addr2line -e PicoRTK.ino.elf <pc>` on the ELF of the same build.

## Private build with your settings built in

A Pico flashed with a private build starts with your Wi-Fi networks, caster logins and bench position, and does not need the setup access point. Its web page shows "(private build)" next to the version.

```
python3 tools/make_defaults.py picortk-config.json   # the file from System -> Backup -> Download settings
bash tools/build.sh                                   # build/PicoRTK.ino.uf2 now carries those defaults
python3 tools/make_defaults.py --remove               # back to the public defaults
```

- The generated `firmware/PicoRTK/defaults_local.h` holds passwords in plain text. It is git-ignored: never commit it, and never share the UF2 built with it.
- The CI firmware on GitHub is built without it.
- Settings saved on a Pico still take precedence over the built-in ones. **System → Restore defaults** deletes the saved settings and restarts with the built-in ones.

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
