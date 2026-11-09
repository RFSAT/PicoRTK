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
//   part = "base" | "lid" | "usbc_cover" | "ko_test" | "assembly" | "exploded" (previews with ghost boards)

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
pico_stand = 11.5;   // column height under the Pico (two columns, antenna end); 10 in revisions 1-7. Measured on the
                     // printed case: the Pico lies flat 3 mm under the lid, its USB end resting on the foam (3.5 mm gap)
above_pico = 3.0;    // clearance between the Pico PCB top and the lid (measured); 4.5 in revisions 1-7, so with
                     // pico_stand = 11.5 the case keeps the same height
pico_usb_lift = 0;   // preview only: tilt of the Pico's USB end above its antenna end; 0 = flat (it is, from revision 8)
musb_h = 2.95;       // preview only: micro-USB socket height above the Pico PCB; on the print it just touches the lid

/* [Clearances to the walls] */
cable_gap  = 3.5;    // GNSS board edge to the inside of the front wall (UART and USB-C side); the back wall uses the same gap, so the boards stay centred
side_gap   = 0.5;    // other board edges to the walls
sma_flange = 1.9;    // the SMA's square flange sticks out this far beyond the GNSS board edge

/* [Openings] */
usb_micro_w = 10.0;  // micro-USB plug overmould, width (12 in revisions 1-4)
usb_micro_z = 11.5;  // bottom of the U-slot (8.5 in revisions 1-4; 3 mm shallower after test fit)
usbc_w = 10.5;  usbc_h = 6.0;   // USB-C opening: the plug body must pass, because the socket is 5 mm behind the outer wall face (4 mm cable gap + wall); fits slim plugs, use 12.5 x 7 for bulky ones
sma_d  = 6.9;        // 1/4-36 UNS thread is 6.35 mm
sma_tab_drop = 0.5;  // the lid's tongue reaches this much lower onto the SMA than the base slot's centre (0 in revisions 1-5, 1.0 in revision 6)
// LG290P USB-C: "knockout" = the opening is closed by a break-out panel (pry it out with a small flat screwdriver
// in the notch at its top edge; it pivots on its bottom edge and breaks outwards); "open" = plain opening
usbc_mode = "knockout";
ko_slot  = 0.5;      // slot between the panel and the wall (through the wall, except where the panel is held)
ko_web   = 0.4;      // thickness of the holding web and side tabs at the outer face: thinner = easier to break (0.6 in revisions 4-9; try 0.4-0.8 on ko_test)
ko_tab_h = 1.2;      // height of the small tab holding each side of the panel
ko_notch = [4.0, 1.0, 1.0];   // pry notch at the top edge of the panel: width, height, depth from the outer face
cover_fit = 0.15;    // press-in cover: clearance per side in the opening (smaller = tighter)
btn_hole = 3.2;      // BOOTSEL (lid) and RST (floor) access holes

/* [Screws] */
m2_clear = 2.4;  m2_head = 4.4;  m2_pilot = 1.7;
col_d_gnss = 4.5;   // printed columns under the GNSS board's four corner holes
col_d_gnss_tight = 4.0;  // the column at x = 30.5 on the USB-C side (gnss_holes[1]) is thinner: on the printed revision 3
                         // it touched components of the board (4.5 mm in revisions 1-8)
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
y_in1 = gnss_y0 + gnss_w + cable_gap;  // inside of back wall: same gap as the front, so the boards (and the micro-USB) are centred
sma_y0 = gnss_y0 + gnss_w + sma_flange; // at the SMA the back wall steps in to here, so enough thread sticks out
sma_bay = 12;                            // width of that step
z_in1 = pico_top + above_pico;         // underside of the lid

gnss_holes = [[2.5, gnss_y0+2.5], [30.5, gnss_y0+2.5], [2.5, gnss_y0+30.5], [30.5, gnss_y0+30.5]];
pico_holes = [[49, 4.8], [49, 16.2]];      // the two at the antenna end
usb_micro_c = [0, 10.5, pico_top + 1.3];   // x, y, z of the micro-USB socket centre (left wall)
usbc_c   = [16.5, gnss_stand - 1.6];       // x, z centre on the front wall
sma_c    = [16.5, gnss_stand + gnss_t/2];  // x, z centre on the back wall
bootsel  = [12.0, 7.2];                    // Pico BOOTSEL button (top side)
pico_led = [4.6, 2.7];                     // Pico on-board LED, light window
rst      = [33 - 28.4, gnss_y0 + 9.6];     // LG290P RST button, mirrored because the board faces down
bootsel_top = pico_top + 1.4;              // button cap height
rst_tip     = gnss_stand - 1.4;            // button points down

// lid screw bosses: three stand on the floor beside the GNSS board; the back-left one hangs from the back wall
// above the GNSS board (it would overlap the board's back edge), clear of that corner's screw
bi = boss_r - 0.4;   // bosses overlap the walls by 0.4 mm
bosses_floor = [[36.5, y_in0 + bi], [x_in1 - bi, y_in0 + bi], [x_in1 - bi, y_in1 - bi]];
boss_corbel  = [6.5, y_in1 - bi];
corbel_z0    = gnss_top + boss_r + 0.5;   // its 45-degree support cone ends 0.5 mm above the GNSS board
all_bosses   = concat(bosses_floor, [boss_corbel]);

module rrect(x0, x1, y0, y1, r) { // rounded rectangle in the xy plane
  hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y]) circle(r = r);
}

// ---------------- base ----------------
module base() {
  if (usbc_mode == "knockout") usbc_panel();
  difference() {
    union() {
      // shell
      difference() {
        translate([0, 0, -floor_t]) linear_extrude(z_in1 + floor_t) rrect(x_in0 - wall, x_in1 + wall, y_in0 - wall, y_in1 + wall, 2.5);
        linear_extrude(z_in1 + 1) rrect(x_in0, x_in1, y_in0, y_in1, 0.8);
      }
      // board columns, printed with the floor: 7 mm for the GNSS board, 10 mm for the Pico
      for (k = [0 : 3]) let(h = gnss_holes[k], d = k == 1 ? col_d_gnss_tight : col_d_gnss) translate([h[0], h[1], -0.01]) {
        cylinder(d = d, h = gnss_stand + 0.01);
        cylinder(d1 = d + 2, d2 = d, h = 1);          // fillet at the foot
      }
      for (h = pico_holes) translate([h[0], h[1], -0.01]) {
        cylinder(d = col_d_pico, h = pico_stand + 0.01);
        cylinder(d1 = col_d_pico + 2, d2 = col_d_pico, h = 1);
      }
      // lid screw bosses
      for (b = bosses_floor) translate([b[0], b[1], 0]) cylinder(r = boss_r, h = z_in1);
      translate([boss_corbel[0], boss_corbel[1], corbel_z0]) {
        cylinder(r = boss_r, h = z_in1 - corbel_z0);
        translate([0, 0, -boss_r + 0.5]) cylinder(r1 = 0.5, r2 = boss_r, h = boss_r - 0.5);
      }
      // SMA bay: the back wall steps in to the SMA flange over sma_bay mm
      translate([sma_c[0] - sma_bay/2 - wall, sma_y0, -floor_t]) cube([sma_bay + 2*wall, y_in1 - sma_y0 + 0.5, z_in1 + floor_t]);
    }
    // screw pilots in the board columns (6 mm deep)
    for (h = gnss_holes) translate([h[0], h[1], gnss_stand - 6]) cylinder(d = col_pilot, h = 7);
    for (h = pico_holes) translate([h[0], h[1], pico_stand - 6]) cylinder(d = col_pilot, h = 7);
    // lid screw pilots
    for (b = all_bosses) translate([b[0], b[1], z_in1 - 5]) cylinder(d = m2_pilot, h = 6);   // M2 x 5 countersunk thread-forming screws
    // micro-USB: U-slot from the top of the left wall
    translate([x_in0 - wall - 1, usb_micro_c[1] - usb_micro_w/2, usb_micro_z]) cube([wall + 2, usb_micro_w, z_in1]);
    // USB-C: opening in the front wall
    translate([usbc_c[0], y_in0 + 1, usbc_c[1]]) rotate([90, 0, 0]) linear_extrude(wall + 2)
      rrect(-usbc_w/2, usbc_w/2, -usbc_h/2, usbc_h/2, 1.5);
    // SMA: U-slot in the back wall, closed from above by a tab on the lid (about 6 mm of thread sticks out)
    translate([sma_c[0] - sma_bay/2, y_in1, -floor_t - 1]) cube([sma_bay, wall + 1, z_in1 + floor_t + 2]);   // outside of the bay
    translate([sma_c[0], sma_y0 - 1, sma_c[1]]) rotate([-90, 0, 0]) cylinder(d = sma_d, h = y_in1 - sma_y0 + 2);
    translate([sma_c[0] - sma_d/2, sma_y0 - 1, sma_c[1]]) cube([sma_d, y_in1 - sma_y0 + 2, z_in1]);
    // RST access hole through the floor (the button faces down)
    translate([rst[0], rst[1], -floor_t - 1]) cylinder(d = btn_hole, h = floor_t + 2);
  }
}

// ---------------- USB-C break-out panel and its optional cover ----------------
// Local frame on the front wall: u along the wall (x), v up (z), d into the wall from its outer face (y)
module on_front(d0 = 0) {
  multmatrix([[1, 0, 0, usbc_c[0]], [0, 0, 1, y_in0 - wall + d0], [0, 1, 0, usbc_c[1]], [0, 0, 0, 1]]) children();
}
module usbc_panel() {
  w = usbc_w; h = usbc_h; s = ko_slot; r = 1.5;
  on_front() {
    difference() {
      linear_extrude(wall) rrect(-w/2 + s, w/2 - s, -h/2 + s, h/2 - s, r - s/2);
      // pry notch at the top edge, outer face
      translate([-ko_notch[0]/2, h/2 - s - ko_notch[1], -1]) cube([ko_notch[0], ko_notch[1] + 1, ko_notch[2] + 1]);
    }
    // bottom edge: held along its straight part by a thin web at the outer face (the hinge it breaks along)
    translate([-w/2 + r, -h/2 - 0.05, 0]) cube([w - 2*r, s + 0.1, ko_web]);
    // a small tab on each side, at mid-height, also at the outer face
    for (sg = [-1, 1]) translate([sg > 0 ? w/2 - s - 0.05 : -w/2 - 0.05, -ko_tab_h/2, 0]) cube([s + 0.1, ko_tab_h, ko_web]);
    // the top edge is a free slot, so the panel pivots outwards on its bottom edge
  }
}
// Optional press-in cover for an opened USB-C hole: a plug with a thin flange outside and a pull tab.
// Printed flange down (as exported).
module usbc_cover() {
  w = usbc_w; h = usbc_h; c = cover_fit; fl = 1.2; ft = 1.0; plug = wall - 0.4;
  difference() {
    union() {
      linear_extrude(ft) rrect(-w/2 - fl, w/2 + fl, -h/2 - fl, h/2 + fl, 1.5 + fl);   // flange
      translate([0, 0, ft - 0.01]) linear_extrude(plug + 0.01) rrect(-w/2 + c, w/2 - c, -h/2 + c, h/2 - c, 1.5 - c);
      // two crush ribs on the long sides grip the opening
      for (sg = [-1, 1]) translate([-2, sg > 0 ? h/2 - c - 0.01 : -h/2 + c - 0.3 + 0.01, ft + 0.3]) cube([4, 0.3, plug - 0.6]);
      // pull tab above the flange
      translate([-2.5, h/2 + fl - 0.5, 0]) cube([5, 3.5, ft]);
    }
    translate([0, h/2 + fl + 1.8, -1]) cylinder(d = 1.6, h = 3);   // hole in the tab for a fingernail or string
    translate([0, 0, ft + 0.6]) linear_extrude(plug) rrect(-w/2 + c + 1.0, w/2 - c - 1.0, -h/2 + c + 1.0, h/2 - c - 1.0, 0.6);   // hollow: springs a little
  }
}
// Test piece: a 2 mm wall with the same panel, standing on a foot, printed upright like the case wall
module ko_test() {
  plate = [usbc_w + 10, usbc_h + 8];
  difference() {
    union() {
      translate([usbc_c[0] - plate[0]/2, y_in0 - wall, 0]) cube([plate[0], wall, plate[1]]);
      translate([usbc_c[0] - plate[0]/2, y_in0 - wall - 4, -2]) cube([plate[0], wall + 8, 2]);
    }
    on_front(-1) linear_extrude(wall + 2) rrect(-usbc_w/2, usbc_w/2, -usbc_h/2, usbc_h/2, 1.5);
  }
  usbc_panel();
}

// ---------------- lid ----------------
lip_t = 1.2; lip_h = 2.0;
module lid() {
  difference() {
    union() {
      // lid plate: same outline as the base, including the step at the SMA
      translate([0, 0, z_in1]) linear_extrude(lid_t) difference() {
        rrect(x_in0 - wall, x_in1 + wall, y_in0 - wall, y_in1 + wall, 2.5);
        translate([sma_c[0] - sma_bay/2, y_in1]) square([sma_bay, wall + 1]);
      }
      // locating lip inside the walls, cut back around bosses and the two U-slots
      difference() {
        translate([0, 0, z_in1 - lip_h]) linear_extrude(lip_h)
          difference() {
            rrect(x_in0 + fit, x_in1 - fit, y_in0 + fit, y_in1 - fit, 0.6);
            rrect(x_in0 + fit + lip_t, x_in1 - fit - lip_t, y_in0 + fit + lip_t, y_in1 - fit - lip_t, 0.3);
          }
        for (b = all_bosses) translate([b[0], b[1], 0]) cylinder(r = boss_r + 0.4, h = 100);
        translate([x_in0 - 1, usb_micro_c[1] - usb_micro_w/2 - 0.5, 0]) cube([lip_t + 2, usb_micro_w + 1, 100]);
        translate([sma_c[0] - sma_bay/2 - wall - fit, sma_y0 - fit, 0]) cube([sma_bay + 2*wall + 2*fit, y_in1 - sma_y0 + 2, 100]);
      }
      // tab that fills the SMA U-slot down to the connector
      difference() {
        translate([sma_c[0] - sma_d/2 + fit/2, sma_y0 + fit/2, sma_c[1] - sma_tab_drop]) cube([sma_d - fit, y_in1 - sma_y0 - fit, z_in1 - sma_c[1] + sma_tab_drop + 0.01]);
        translate([sma_c[0], sma_y0 - 1, sma_c[1] - sma_tab_drop]) rotate([-90, 0, 0]) cylinder(d = sma_d + 0.3, h = y_in1 - sma_y0 + 2);
      }
    }
    // screws, countersunk
    for (b = all_bosses) translate([b[0], b[1], z_in1 - lip_h - 1]) {
      cylinder(d = m2_clear, h = 10);
      translate([0, 0, lip_h + 1 + lid_t - (m2_head - m2_clear)/2 + 0.01]) cylinder(d1 = m2_clear, d2 = m2_head, h = (m2_head - m2_clear)/2);
    }
    // micro-USB: the plug overmould is taller than the space under the lid, so the slot continues through the lid edge
    translate([x_in0 - wall - 1, usb_micro_c[1] - usb_micro_w/2, z_in1 - lip_h - 1]) cube([wall + 1 + fit, usb_micro_w, lid_t + lip_h + 2]);
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
  // the Pico, tilted about its antenna-end columns (x = 49) so its USB end sits pico_usb_lift higher
  translate([49, 0, pico_z0]) rotate([0, atan(pico_usb_lift / 49), 0]) translate([-49, 0, -pico_z0]) {
    color("ForestGreen", 0.9) translate([0, 0, pico_z0]) cube([pico_l, pico_w, pico_t]);
    color("Silver") translate([-1.3, 6.6, pico_top]) cube([5.7, 7.8, musb_h]);                // micro-USB
  }
  color("LightGrey", 0.7) translate([1, -4, gnss_top]) cube([31, 29, gap]);                   // foam
}

if (part == "base") base();
else if (part == "usbc_cover") usbc_cover();
else if (part == "ko_test") translate([0, 0, 2]) ko_test();
else if (part == "lid") translate([0, 0, z_in1 + lid_t]) rotate([180, 0, 0]) lid();   // printed top face down: turned over (a rotation, not a mirror)
else if (part == "lid_check") translate([0, 0, z_in1 + lid_t]) rotate([180, 0, 0]) import("picortk_case_lid.stl");   // the printed lid turned back over
else if (part == "assembly") { color("Gainsboro") base(); ghost(); color("LightSteelBlue", 0.45) lid(); }
else if (part == "exploded") { color("Gainsboro") base(); ghost(); color("LightSteelBlue") translate([0, 0, 18]) lid(); }
