// PicoRTK case: Raspberry Pi Pico 2 W above a Waveshare LG290P GNSS RTK board (component side down)
// Two printed parts: base (bottom part) and lid. BOOTSEL (lid) and RST (floor) are pressed through holes
// with a toothpick or a straightened paperclip.
//
// Coordinates (mm), as in the assembly drawing:
//   x along the Pico, 0 = USB end (both boards are flush there)
//   y across, 0..21 = Pico, -6..27 = LG290P board (centres aligned); the UART socket edge is at y = -6
//   z up, 0 = top surface of the case floor
//
// Render one part:  openscad -D 'part="base"' -o base.stl picortk_case.scad
//   part = "base" | "lid" | "assembly" | "exploded" (previews with ghost boards)

part = "assembly";

/* [Case] */
wall      = 2.0;     // side walls
floor_t   = 2.0;     // floor
lid_t     = 2.0;     // lid plate
fit       = 0.25;    // clearance between printed parts that slide together
$fn       = 48;

/* [Boards, from the user's set-up and Waveshare's drawing] */
gnss_w     = 33;     // LG290P board, square
gnss_t     = 1.0;    // GNSS board thickness that makes the 2 mm gap with 7 mm and 10 mm spacers (see echo below)
gnss_y0    = -6;     // its UART/USB-C edge
gnss_stand = 7;      // column height under the GNSS board (its component side faces the floor; battery 7 mm)
pico_l     = 51;
pico_w     = 21;
pico_t     = 1.0;
pico_stand = 10;     // column height under the Pico (two columns, antenna end)
above_pico = 4.5;    // clearance between the Pico PCB top and the lid (4 mm + 0.5 mm margin)

/* [Clearances to the walls] */
cable_gap  = 4;      // UART socket edge of the GNSS board to the inside of the front wall
side_gap   = 0.5;    // other board edges to the walls
sma_flange = 1.9;    // the SMA's square flange sticks out this far beyond the GNSS board edge

/* [Openings] */
usb_micro_w = 12.0;  // micro-USB plug overmould, width
usb_micro_z = 8.5;   // bottom of the U-slot (Pico top 11 + 1.3 - half the plug height)
usbc_w = 13.0;  usbc_h = 7.6;   // USB-C plug overmould opening
sma_d  = 6.9;        // 1/4-36 UNS thread is 6.35 mm
btn_hole = 3.2;      // BOOTSEL (lid) and RST (floor) access holes

/* [Screws] */
m2_clear = 2.4;  m2_head = 4.4;  m2_pilot = 1.7;
col_d_gnss = 4.5;   // printed columns under the GNSS board's four corner holes
col_d_pico = 4.2;   // printed columns under the Pico's two antenna-end holes
col_pilot  = 1.7;   // M2 thread-forming screws; use 3.2 for M2 heat-set inserts
boss_r = 2.5;

// ---------------- derived positions ----------------
gnss_top   = gnss_stand + gnss_t;
pico_z0    = pico_stand;
pico_top   = pico_z0 + pico_t;
gap        = pico_z0 - gnss_top;
echo(str("Gap between the boards: ", gap, " mm (set gnss_t to your board's thickness)"));

x_in0 = -side_gap;                     // inside of left wall
x_in1 = pico_l + side_gap;             // inside of right wall
y_in0 = gnss_y0 - cable_gap;           // inside of front wall
y_in1 = gnss_y0 + gnss_w + sma_flange; // inside of back wall, clear of the SMA flange
z_in1 = pico_top + above_pico;         // underside of the lid

gnss_holes = [[2.5, gnss_y0+2.5], [30.5, gnss_y0+2.5], [2.5, gnss_y0+30.5], [30.5, gnss_y0+30.5]];
pico_holes = [[49, 4.8], [49, 16.2]];      // the two at the antenna end
usb_micro_c = [0, 10.5, pico_top + 1.3];   // y, z centre on the left wall
usbc_c   = [16.5, gnss_stand - 1.6];       // x, z centre on the front wall
sma_c    = [16.5, gnss_stand + gnss_t/2];  // x, z centre on the back wall
bootsel  = [12.0, 7.2];                    // Pico BOOTSEL button (top side)
pico_led = [4.6, 2.7];                     // Pico on-board LED, light window
rst      = [33 - 28.4, gnss_y0 + 9.6];     // LG290P RST button, mirrored because the board faces down
bootsel_top = pico_top + 1.4;              // button cap height
rst_tip     = gnss_stand - 1.4;            // button points down

// lid screw bosses: two floor-standing at the front and back right, one floor-standing at the front middle,
// one wall-hung (corbel) at the back left above the GNSS board
bi = boss_r - 0.4;   // bosses overlap the walls by 0.4 mm
bosses_floor = [[36.5, y_in0 + bi], [x_in1 - bi, y_in0 + bi], [x_in1 - bi, y_in1 - bi]];
boss_corbel  = [10, y_in1 - bi];     // clear of the GNSS screw at the back-left corner and of the SMA
corbel_z0    = gnss_top + boss_r + 0.5;   // its support cone ends 0.5 mm above the GNSS board
all_bosses   = concat(bosses_floor, [boss_corbel]);

module rrect(x0, x1, y0, y1, r) { // rounded rectangle in the xy plane
  hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y]) circle(r = r);
}

// ---------------- base ----------------
module base() {
  difference() {
    union() {
      // shell
      difference() {
        translate([0, 0, -floor_t]) linear_extrude(z_in1 + floor_t) rrect(x_in0 - wall, x_in1 + wall, y_in0 - wall, y_in1 + wall, 2.5);
        linear_extrude(z_in1 + 1) rrect(x_in0, x_in1, y_in0, y_in1, 0.8);
      }
      // board columns, printed with the floor: 7 mm for the GNSS board, 10 mm for the Pico
      for (h = gnss_holes) translate([h[0], h[1], -0.01]) {
        cylinder(d = col_d_gnss, h = gnss_stand + 0.01);
        cylinder(d1 = col_d_gnss + 2, d2 = col_d_gnss, h = 1);          // fillet at the foot
      }
      for (h = pico_holes) translate([h[0], h[1], -0.01]) {
        cylinder(d = col_d_pico, h = pico_stand + 0.01);
        cylinder(d1 = col_d_pico + 2, d2 = col_d_pico, h = 1);
      }
      // lid screw bosses
      for (b = bosses_floor) translate([b[0], b[1], 0]) cylinder(r = boss_r, h = z_in1);
      // back-left boss hangs from the walls above the GNSS board, with a 45-degree cone under it for printing
      translate([boss_corbel[0], boss_corbel[1], corbel_z0]) {
        cylinder(r = boss_r, h = z_in1 - corbel_z0);
        translate([0, 0, -boss_r + 0.5]) cylinder(r1 = 0.5, r2 = boss_r, h = boss_r - 0.5);
      }
    }
    // screw pilots in the board columns (6 mm deep)
    for (h = gnss_holes) translate([h[0], h[1], gnss_stand - 6]) cylinder(d = col_pilot, h = 7);
    for (h = pico_holes) translate([h[0], h[1], pico_stand - 6]) cylinder(d = col_pilot, h = 7);
    // lid screw pilots
    for (b = all_bosses) translate([b[0], b[1], z_in1 - 5]) cylinder(d = m2_pilot, h = 6);   // M2 x 5 countersunk thread-forming screws
    // micro-USB: U-slot from the top of the left wall
    translate([x_in0 - wall - 1, usb_micro_c[0] - usb_micro_w/2, usb_micro_z]) cube([wall + 2, usb_micro_w, z_in1]);
    // USB-C: opening in the front wall
    translate([usbc_c[0], y_in0 + 1, usbc_c[1]]) rotate([90, 0, 0]) linear_extrude(wall + 2)
      rrect(-usbc_w/2, usbc_w/2, -usbc_h/2, usbc_h/2, 1.5);
    // SMA: U-slot in the back wall, closed from above by a tab on the lid (about 6 mm of thread sticks out)
    translate([sma_c[0], y_in1 - 1, sma_c[1]]) rotate([-90, 0, 0]) cylinder(d = sma_d, h = wall + 2);
    translate([sma_c[0] - sma_d/2, y_in1 - 1, sma_c[1]]) cube([sma_d, wall + 2, z_in1]);
    // RST access hole through the floor (the button faces down)
    translate([rst[0], rst[1], -floor_t - 1]) cylinder(d = btn_hole, h = floor_t + 2);
  }
}

// ---------------- lid ----------------
lip_t = 1.2; lip_h = 2.0;
module lid() {
  difference() {
    union() {
      translate([0, 0, z_in1]) linear_extrude(lid_t) rrect(x_in0 - wall, x_in1 + wall, y_in0 - wall, y_in1 + wall, 2.5);
      // locating lip inside the walls, cut back around bosses and the two U-slots
      difference() {
        translate([0, 0, z_in1 - lip_h]) linear_extrude(lip_h)
          difference() {
            rrect(x_in0 + fit, x_in1 - fit, y_in0 + fit, y_in1 - fit, 0.6);
            rrect(x_in0 + fit + lip_t, x_in1 - fit - lip_t, y_in0 + fit + lip_t, y_in1 - fit - lip_t, 0.3);
          }
        for (b = all_bosses) translate([b[0], b[1], 0]) cylinder(r = boss_r + 0.4, h = 100);
        translate([x_in0 - 1, usb_micro_c[0] - usb_micro_w/2 - 0.5, 0]) cube([lip_t + 2, usb_micro_w + 1, 100]);
        translate([sma_c[0] - sma_d/2 - 0.5, y_in1 - lip_t - 1, 0]) cube([sma_d + 1, lip_t + 2, 100]);
      }
      // tab that fills the SMA U-slot down to the connector
      difference() {
        translate([sma_c[0] - sma_d/2 + fit/2, y_in1 + fit/2, sma_c[1]]) cube([sma_d - fit, wall - fit, z_in1 - sma_c[1] + 0.01]);
        translate([sma_c[0], y_in1 - 1, sma_c[1]]) rotate([-90, 0, 0]) cylinder(d = sma_d + 0.3, h = wall + 2);
      }
    }
    // screws, countersunk
    for (b = all_bosses) translate([b[0], b[1], z_in1 - lip_h - 1]) {
      cylinder(d = m2_clear, h = 10);
      translate([0, 0, lip_h + 1 + lid_t - (m2_head - m2_clear)/2 + 0.01]) cylinder(d1 = m2_clear, d2 = m2_head, h = (m2_head - m2_clear)/2);
    }
    // micro-USB: the plug overmould is taller than the space under the lid, so the slot continues through the lid edge
    translate([x_in0 - wall - 1, usb_micro_c[0] - usb_micro_w/2, z_in1 - lip_h - 1]) cube([wall + 1 + fit, usb_micro_w, lid_t + lip_h + 2]);
    // BOOTSEL access hole and LED window
    translate([bootsel[0], bootsel[1], z_in1 - 1]) cylinder(d = btn_hole, h = lid_t + 2);
    translate([pico_led[0], pico_led[1], z_in1 - 1]) cylinder(d = 2.0, h = lid_t + 2);
  }
}

// ---------------- ghost boards for the preview ----------------
module ghost() {
  color("RoyalBlue", 0.85) translate([0, gnss_y0, gnss_stand]) cube([gnss_w, gnss_w, gnss_t]);
  color("Gold") translate([sma_c[0], gnss_y0 + gnss_w - 0.5, sma_c[1]]) rotate([-90, 0, 0]) cylinder(d = 6.35, h = 10.5);
  color("Silver") translate([12, gnss_y0 - 1, gnss_stand - 3.2]) cube([9, 6.5, 3.2]);          // USB-C
  color("WhiteSmoke") translate([5.2, gnss_y0, gnss_stand - 4.25]) cube([6.6, 4.5, 4.25]);   // UART socket
  color("Orange", 0.8) translate([7, 2, 0]) cube([20, 20, gnss_stand]);                       // battery block (7 mm)
  color("ForestGreen", 0.9) translate([0, 0, pico_z0]) cube([pico_l, pico_w, pico_t]);
  color("Silver") translate([-1.3, 6.6, pico_top]) cube([5.7, 7.8, 2.6]);                     // micro-USB
  color("LightGrey", 0.7) translate([1, -4, gnss_top]) cube([31, 29, gap]);                   // foam
}

if (part == "base") base();
else if (part == "lid") translate([0, 0, z_in1 + lid_t]) mirror([0, 0, 1]) lid();   // printed top face down
else if (part == "assembly") { color("Gainsboro") base(); ghost(); color("LightSteelBlue", 0.45) lid(); }
else if (part == "exploded") { color("Gainsboro") base(); ghost(); color("LightSteelBlue") translate([0, 0, 18]) lid(); }
