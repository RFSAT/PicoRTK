# PicoRTK case (Pico 2 W + Waveshare LG290P)

Two printed parts: `picortk_case_base.stl` and `picortk_case_lid.stl`. Both are generated from the parametric `picortk_case.scad` (OpenSCAD 2021 or later).

**Outside size:** 56 × 42.9 × 21 mm (base 19 mm high, lid 2 mm).

## Layout (mm, floor top = 0)

| Item | Height |
|---|---|
| LG290P board, component side down | 7 mm spacers; its battery reaches the floor |
| Gap between the boards | 2 mm |
| Pico 2 W | 10 mm spacers, at its two antenna-end holes |
| Free space above the Pico PCB | 6 mm |

- Both boards are flush at the Pico's USB end and centred across.
- The UART-socket edge of the GNSS board is 4 mm from the front wall, for the cable plug.

## Openings

| Opening | Where | How it's made |
|---|---|---|
| Pico micro-USB | left wall | U-slot 12 mm wide, closed by the lid |
| LG290P USB-C | front wall | 13 × 7.6 mm opening |
| SMA | back wall | U-slot; a tab on the lid closes it over the connector; about 6 mm of thread sticks out |
| BOOTSEL | lid | 3.2 mm hole: press with a toothpick |
| RST | floor | 3.2 mm hole: press with a straightened paperclip (the button faces down) |
| Pico LED | lid | 2 mm light window |

## Hardware

| Part | Qty |
|---|---|
| M2 female–female standoff, 7 mm (GNSS board) | 4 |
| M2 female–female standoff, 10 mm (Pico) | 2 |
| M2 × 4 countersunk screw, from below into the standoffs | 6 |
| M2 × 4 pan-head screw, boards onto the standoffs | 6 |
| M2 × 6 countersunk thread-forming screw for plastic, for the lid | 4 |

## Printing

- **Base:** print floor down.
- **Lid:** printed top face down (the STL is already oriented).
- **Settings:** 0.2 mm layers, 3 walls, PETG or PLA. No supports needed. The one boss hung from the back wall has a 45° cone under it.

## Assembly

1. Screw the six standoffs to the floor from below.
2. Put the boards on, already taped together. The SMA drops into its U-slot and the micro-USB into its slot.
3. Screw the GNSS board down at its four corners; they are clear of the Pico. Then screw the Pico at its two antenna-end holes.
4. Plug in the UART cable, fit the lid, and screw it down.

## Check against your boards before printing

All of these are parameters at the top of the `.scad` file.

- **`gnss_t = 1.0`:** your spacer heights (7 and 10 mm) and the 2 mm gap imply a 1.0 mm GNSS board. If it is 1.6 mm, the Pico spacer must be 10.6 mm, or the gap becomes 1.4 mm. OpenSCAD prints the resulting gap.
- **Button positions:**
  - Pico BOOTSEL: x = 12.0, y = 7.2 from the USB end and pin-1 edge.
  - LG290P RST: x = 4.6, y = 3.6 from the USB end and UART edge. It must not be covered by the battery.
  - Both were measured from Waveshare and Raspberry Pi drawings.
- **SMA flange:** the inside of the back wall sits 1.9 mm beyond the GNSS board edge, for the SMA's square flange.
- **Wires over the Pico:** 6 mm of space is enough for soldered wires. Dupont sockets on upward pins need `above_pico` of about 13.
