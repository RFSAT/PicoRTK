# PicoRTK case (Pico 2 W + Waveshare LG290P)

Two printed parts: `picortk_case_base.stl` and `picortk_case_lid.stl`. Both are generated from the parametric `picortk_case.scad` (OpenSCAD 2021 or later).

**Outside size:** 56 × 44 × 19.5 mm (base 17.5 mm high, lid 2 mm).

## Layout (mm, floor top = 0)

| Item | Height |
|---|---|
| LG290P board, component side down | 7 mm printed columns; its battery reaches the floor |
| Gap between the boards | 2 mm |
| Pico 2 W | 10 mm printed columns, at its two antenna-end holes |
| Pico PCB (1 mm) to the underside of the lid | 4.5 mm (4 mm + 0.5 mm margin) |

- Both boards are flush at the Pico's USB end and centred across.
- The GNSS board is 3.5 mm from the front wall (UART cable plug, USB-C) and 3.5 mm from the back wall, so the boards and the micro-USB slot are centred on the case.
- At the SMA, the back wall steps in to the connector's flange over 12 mm, so about 6 mm of thread still sticks out.

## Openings

| Opening | Where | How it's made |
|---|---|---|
| Pico micro-USB | left wall | U-slot 12 mm wide, continued through the lid edge (the plug is taller than the space above the Pico) |
| LG290P USB-C | front wall | 10.5 × 6 mm opening, centred on the socket; fits slim USB-C plugs (set `usbc_w = 12.5`, `usbc_h = 7` for bulky ones) |
| SMA | back wall | U-slot; a tab on the lid closes it over the connector; about 6 mm of thread sticks out |
| BOOTSEL | lid | 3.2 mm hole: press with a toothpick |
| RST | floor | 3.2 mm hole: press with a straightened paperclip (the button faces down) |
| Pico LED | lid | 2 mm light window |

## Board columns

The base has six printed columns, each with a 1.7 mm pilot hole for an M2 thread-forming screw:

- four 7 mm columns (4.5 mm across) under the GNSS board's corner holes;
- two 10 mm columns (4.2 mm across) under the Pico's antenna-end holes.

To use M2 heat-set inserts instead, set `col_pilot = 3.2`.

## Hardware

| Part | Qty |
|---|---|
| M2 × 5 thread-forming screw for plastic (boards onto the columns) | 6 |
| M2 × 5 countersunk thread-forming screw (lid) | 4 |

## Printing

- **Base:** print floor down.
- **Lid:** printed top face down (the STL is already oriented).
- **Settings:** 0.2 mm layers, 3 walls, PETG or PLA. No supports needed: the back-left lid boss hangs from the wall above the GNSS board on a 45° cone. The one boss hung from the back wall has a 45° cone under it.

## Assembly

1. Put the boards on, already taped together. The SMA drops into its U-slot and the micro-USB into its slot.
2. Screw the GNSS board down at its four corners; they are clear of the Pico. Then screw the Pico at its two antenna-end holes.
3. Plug in the UART cable, fit the lid, and screw it down.

## Check against your boards before printing

All of these are parameters at the top of the `.scad` file.

- **`gnss_t = 1.0`:** the column heights (7 and 10 mm) and the 2 mm gap imply a 1.0 mm GNSS board. If it is 1.6 mm, set `pico_stand = 10.6`, or the gap becomes 1.4 mm. OpenSCAD prints the resulting gap.
- **Button positions:**
  - Pico BOOTSEL: x = 12.0, y = 7.2 from the USB end and pin-1 edge.
  - LG290P RST: x = 4.6, y = 3.6 from the USB end and UART edge. It must not be covered by the battery.
  - Both were measured from Waveshare and Raspberry Pi drawings.
- **SMA flange:** at the SMA the wall steps in to 1.9 mm beyond the GNSS board edge (`sma_flange`), for the connector's square flange.
- **Wires over the Pico:** 4.5 mm of space suits wires soldered to the pads. Dupont sockets on upward pins need `above_pico` of about 13.
