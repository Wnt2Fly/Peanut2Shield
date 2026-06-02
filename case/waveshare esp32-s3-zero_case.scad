// Waveshare ESP32-S3-Zero case with two button actuators
// v32: tightens the button-to-cutout gaps and adds LED light-pipe glue/retention features.
// v31: removes the raised edge/stiffening rib on the top cap, widens the button pads, and changes the switch contact nubs from cones to flat cylinders.
// v30: fixes snap receiver pockets so they are derived from the same tab coordinates and correct installed Z height.
// v29: sets measured lid-to-button gap to 1.80 mm and derives the top button nub height from it.
// v28: removes board support rails, sets USB opening bottom to 1.80 mm, interior height to 5.00 mm, and lid rim depth to 3.25 mm.
// v27: removes the rear/back segment of the top cap underside rim.
// v26: floor_h=0.50, rail_h=0.50, USB slot z0=2.30, and cap rim calculated to slightly touch the 1.6 mm PCB.
// v24: USB relief is localized to the connector body only, not a full-height pocket around the opening.
// v23: cleans the USB/front wall area and adds a matching USB clearance notch in the snap-on top cap.
// v22: adds engraved button designations on the outside/top face of the cap.
// v21: LED window reworked as a captured separate clear light-pipe part for slicers.
// - Default all view shows bottom tray, top lid/cap, and the clear LED pipe installed in the lid.
// - Export top/lid/cap produces the actual snap-on clamshell lid with a clean LED hole.
// - Export led_insert produces only the clear LED pipe, aligned to the top lid origin.
// - The LED pipe has a small underside flange so it is retained and slicers stop confusing it with a loose center circle.
// Button islands are full top-plate thickness; only the press nubs project from the inside face.
// Buttons, LED window/clear insert, and button contact nubs are on the TOP piece.
// Bottom is a closed tray for the board, rails, USB-C side opening, and snap receivers.
// v10 base was fit-checked against the uploaded ESP32-S3-Zero STEP model.
// Board/button coordinates below are extracted from the STEP CARTESIAN_POINT data, not eyeballed from the drawing.
// Recreated from STL measurements (not a mesh import).
// Coordinate system: base lower-left-front corner = [0,0,0]. Units: mm.

$fn = 64;

// Export selector:
//   "all"       = bottom tray + TOP LID/CAP + clear LED insert laid out as separate printable bodies
//   "base"      = base only
//   "led_insert"= clear LED plug only, at the SAME origin as the top so it can be imported as an aligned part
//   "led_insert_layout" = clear LED plug laid out next to the cap for visual checking
//   "top_with_led_preview" = top with LED insert shown in-place for checking only, not for multi-material export
//   "top"/"lid"/"cap" = snap-on clamshell top lid only
//   "lid_split" = split two-piece lid laid out together
//   "lid_left"  = left-side lid half
//   "lid_right" = right-side lid half
//   "lid_front" = legacy alias for left-side lid half
//   "lid_back"  = legacy alias for right-side lid half
// This makes it easier to import the LED insert as a separate part and assign clear PLA.
export_part = "base";   // default: bottom + TOP LID + separate LED insert laid out side-by-side

// ---------------- measured outer dimensions ----------------
base_w = 20.40;
base_d = 26.40;
floor_h = 0.50;      // v26/v28: thin closed bottom floor
interior_h = 5.00;   // v28: full inside height above floor
base_h = floor_h + interior_h;  // outside height = bottom floor + interior = 5.50 mm
corner_r = 2.20;
wall_t  = 1.20;

// Waveshare ESP32-S3-Zero board outline from drawing.
board_w = 18.01;          // STEP board max X ~18.0087 mm
board_d = 23.51;          // STEP board max Y ~23.5086 mm
board_corner_r = 1.00;
board_clear_x = 0.30;      // FDM clearance; v8 had exactly 18.00 mm and would be too tight
board_clear_y = 0.50;
board_x0 = (base_w - board_w) / 2;
board_y0 = 1.20;           // board USB edge sits against the inside of the connector wall

// Interior tray opening. Slightly widened vs the measured STL so the 18.00 mm board actually fits.
inner_x0 = board_x0 - board_clear_x/2;
inner_x1 = board_x0 + board_w + board_clear_x/2;
inner_y0 = board_y0;
inner_y1 = board_y0 + board_d + board_clear_y;

// Raised internal side rails measured at z=1.2..2.95
rail_w = 1.50;
rail_h = 0.00;       // v28: rails removed; board sits on the floor
rail_z = floor_h;
rail_y0 = 1.20;
rail_y1 = 25.20;
rail_inner_left_x0  = inner_x0;
rail_inner_left_x1  = inner_x0 + rail_w;
rail_inner_right_x0 = inner_x1 - rail_w;
rail_inner_right_x1 = inner_x1;
rail_r = 0.45;

// USB-C opening.
// USB-C receptacle shell is roughly 8.4 x 2.6 mm; these are printable clearance dimensions.
// The original STL connector access was on the front-notch end, not the opposite wall.
// Toggle usb_on_back_wall if your slicer/model coordinate view shows it on the wrong edge.
// USB-C connector mouth/plug clearance. USB-C receptacle shell is about 8.4 x 2.6 mm;
// 9.4 x 3.7 gives sane FDM clearance without turning it into the old giant notch.
usb_c_open_w  = 9.40;
usb_c_open_h  = 3.70;
usb_c_open_r  = 1.35;
usb_c_center_x = board_x0 + board_w/2;
usb_c_open_x0 = usb_c_center_x - usb_c_open_w/2;
usb_c_open_z0 = 1.80;     // v28: USB-C opening bottom height
usb_on_back_wall = false;   // false = original notch/button end at y=0; true = opposite wall at y=base_d

// v24 USB cleanup / lid relief.
// Do NOT make a big full-height pocket around the whole USB opening.
// The only extra inside clearance is a localized pocket for the USB-C metal body
// where it projects back into the case over the PCB.
usb_inner_pocket = false;              // legacy v23 big bay; keep OFF
usb_body_pocket = true;                // localized connector-body relief only
usb_body_pocket_w = 8.90;              // USB-C receptacle body width; not cable shell width
usb_body_pocket_h = 3.10;              // local body height
usb_body_pocket_depth = 3.10;          // how far the connector body projects into the box
usb_body_pocket_z0 = floor_h + rail_h + 0.65; // bottom of relief: board support + approx PCB/top offset

// Matching clearance in the TOP cap/lid so it closes around the USB-C receptacle body.
// v25 IMPORTANT: this is NOT a through-cut in the visible top lid.
// It cuts only the underside plug/rim/skirt -- the part that inserts into the bottom box.
cap_usb_notch = true;
cap_usb_notch_w = usb_body_pocket_w + 0.90;
cap_usb_notch_depth = usb_body_pocket_depth + 0.70;
cap_usb_notch_extra_h = 0.30;
cap_usb_cut_visible_top = false;     // keep false: only notch the underside rim, not the top plate
cap_usb_notch_z_clear = 0.03;        // tiny overlap so CSG cleanly removes the rim material

// The old bottom pry relief overlapped visually with the USB area. Default off in v23.
bottom_pry_relief = false;

// Original STL had an open floor channel here. v6 closes the bottom/floor,
// leaving only the USB-C wall opening above.
front_cut_x0 = 5.70;
front_cut_x1 = 14.70;
front_cut_y1 = 7.99;

// Center LED window. The base still has a circular cutout, but v11 adds a matching
// separate insert object so this area can be assigned to clear PLA/transparent filament
// in the slicer. For a single-material print, set show_led_insert = false.
center_hole_x = 10.20;
center_hole_y = 10.20;
center_hole_r = 1.50;          // nominal LED location radius from original STL

// v21 LED lens/light-pipe parameters.
// The top lid gets a slightly oversized clean hole. The separate clear part has a
// smaller flush shaft plus a larger underside flange, like a tiny rivet/mushroom.
// This is more slicer-friendly than a same-radius cylinder touching the lid wall.
led_hole_r = 1.3;             // hole cut in the top lid
led_pipe_r = 1.50;             // v32: tighter fit in hole; still leaves ~0.08 mm radial clearance
led_pipe_h = 1.47;             // should be cap_top_h + tiny overlap; set after cap_top_h if you change cap_top_h
led_flange_r = 2.35;           // v32: larger hidden glue flange/washer
led_flange_h = 0.60;           // v32: taller flange gives more glue area
led_retention_ribs = true;     // v32: tiny ribs on clear pipe for glue/friction
led_retention_rib_count = 4;
led_retention_rib_w = 0.20;
led_retention_rib_out = 0.10;
led_insert_top_proud = 0.00;   // keep outside face flush; do not make proud unless you want a bump
show_led_insert = true;        // preview/layout only. Export top and led_insert separately for multi-material.

// Button/keyhole actuator geometry.
// STEP-measured switch bodies: 3.00 x 2.50 mm.
// STEP switch centers in board coordinates, with board origin at lower-left / non-USB end:
//   left  = [4.870, 14.83863]
//   right = [13.220, 14.83863]
// Because the case coordinate origin is at the USB/front edge, convert Y as:
//   case_y = board_y0 + board_d - step_switch_y_from_non_usb_end
step_btn_left_x = 4.870;
step_btn_right_x = 13.220;
step_btn_y_from_non_usb_end = 14.83862964;
btn_y = board_y0 + board_d - step_btn_y_from_non_usb_end;
btn_left_x = board_x0 + step_btn_left_x;
btn_right_x = board_x0 + step_btn_right_x;
btn_outer_r = 1.75;
btn_inner_r = 1.48;      // v32: larger pad, smaller visible clearance gap
btn_stem_w = 2.05;       // v32: wider actuator tongue / base
btn_clear_w = 2.40;      // v32: reduced gap around tongue; increase if it fuses
btn_stem_y0 = btn_y;
btn_stem_y1 = board_y0 + 15.80;

// Raised contact points on the inside/top of the two flexible printed buttons.
// These are the parts that actually reach the tactile switches. Tune nub_h after test fitting:
// too low = no click; too high = button held down constantly.
button_nub_r1 = 0.80;
button_nub_r2 = 0.22;
button_nub_h  = 0.55;
button_nub_z0 = floor_h - 0.01;   // legacy/base-button value, not used by v16 top buttons

// v16 top-cover button settings. The top panel is thicker for stiffness, but the
// two button islands/tongues are thinner so they can flex. In print orientation,
// the outside/top face is z=0 and these features grow upward into the inside of the cap.
top_button_flex_h = 1.45;        // v18: explicit full top thickness; avoids pre-definition weirdness/towers
top_button_nub_r1 = 1.05;     // v31: wider flat contact foot
top_button_nub_r2 = 1.05;     // v31: same as r1, so this is a flat cylinder, not a cone

// v29 button reach. You measured 1.80 mm from the inside/top case surface to the tactile switch button.
// Leave a tiny gap so the switch is not held down constantly. Set this to 0.00 if you want it just touching.
top_button_gap_to_switch = 1.80;
top_button_idle_clearance = 0.05;
top_button_nub_h = top_button_gap_to_switch - top_button_idle_clearance;  // 1.75 mm default

// Button designations on the visible outside face of the top cap.
// Default is tiny engraved B/R because the part is small. Set to "BOOT" / "RST"
// if you prefer words, but reduce label_size or move the positions if they crowd the buttons.
button_labels = true;
button_label_left = "B";        // board label: BOOT
button_label_right = "R";       // board label: RESET/RST
button_label_size = 1.65;
button_label_depth = 0.28;      // engraved depth into outside face; keep shallow so buttons stay strong
button_label_font = "Liberation Sans:style=Bold";
button_label_on_button = true;  // true = engrave on round button pads; false = labels just below buttons
button_label_y_offset = -0.05;  // fine tune on button when button_label_on_button=true
button_label_side_y_offset = -3.35; // used when button_label_on_button=false

// Separate lid as printed next to the case
part_gap = 2.05;
lid_w = 18.50;
lid_d = 23.80;
lid_h = 1.20;
lid_corner_r = 1.35;
lid_tab_out = 0.35;
lid_tab_y0 = 7.44;
lid_tab_y1 = 16.44;

// Split-lid / anti-flimsy options.
// v14 splits the lid the OTHER WAY: left/right halves, with the seam running front-to-back.
// This matches the style in your photo better than the previous USB/rear split.
lid_split_x = lid_w / 2;
lid_split_clearance = 0.28;      // gap between left/right lid halves at the center seam
lid_reinforce_h = 0.65;          // raised seam-rail height above lid_h
lid_reinforce_w = 1.05;          // width of each raised seam rail, measured in X
lid_reinforce_inset_y = 1.35;    // keep seam rails away from rounded front/back ends

// Small pry notch at one end of the center seam. The two half-notches combine into a tool/fingernail slit.
lid_pry_slit_l = 5.00;           // length of pry notch along Y
lid_pry_slit_w = 0.90;           // width of notch into each half along X
lid_pry_slit_end = "front";      // "front" = USB/button end, "back" = rear end

// Optional little raised alignment bead along the seam. This is only a low-profile top feature;
// it stiffens and gives your finger/tool a ridge without adding underside geometry that could hit the board.
lid_center_bead = true;
lid_center_bead_w = 0.55;
lid_center_bead_h = 0.35;
lid_side_label = false;          // tiny preview text labels; set false for final export

// Preview-only board fit checker. Leave false for STL export. Set true in OpenSCAD preview to see
// whether the board outline, buttons, and USB-C opening line up.
show_board_check = false;
board_preview_z = floor_h + 0.05;
button_preview_r = 1.35;
switch_body_w = 3.00;      // from STEP: BUTTON-2X3XH1_5 body X span
switch_body_d = 2.50;      // from STEP: body Y span
switch_body_h = 1.60;      // from STEP body height

// ---------------- helper geometry ----------------
module rounded_rect_2d(w, d, r) {
    hull() {
        translate([r, r]) circle(r=r);
        translate([w-r, r]) circle(r=r);
        translate([r, d-r]) circle(r=r);
        translate([w-r, d-r]) circle(r=r);
    }
}

module rect_from_to(x0,y0,x1,y1) {
    translate([x0,y0]) square([x1-x0, y1-y0], center=false);
}

module rounded_box(w,d,h,r) {
    linear_extrude(height=h) rounded_rect_2d(w,d,r);
}

// Rounded opening extruded through a Y-facing wall.
// The 2D rounded rectangle is drawn in X/Z and extruded along Y.
module y_wall_rounded_opening(x0, y_inside, z0, w, h, r, depth, front=true) {
    rr = min(r, min(w,h)/2 - 0.01);
    if (front) {
        // Front/connector end at y=0.  Start just inside the wall and cut outward through y=0.
        translate([x0, y_inside, z0])
            rotate([90, 0, 0])
                linear_extrude(height=depth)
                    rounded_rect_2d(w, h, rr);
    } else {
        // Rear wall at y=base_d.  Start just inside the rear wall and cut outward through y=base_d.
        translate([x0, y_inside, z0])
            rotate([-90, 0, 0])
                linear_extrude(height=depth)
                    rounded_rect_2d(w, h, rr);
    }
}

module usb_c_wall_opening() {
    if (usb_on_back_wall)
        y_wall_rounded_opening(usb_c_open_x0, base_d - wall_t + 0.20, usb_c_open_z0, usb_c_open_w, usb_c_open_h, usb_c_open_r, wall_t + 1.00, false);
    else
        y_wall_rounded_opening(usb_c_open_x0, wall_t + 0.20, usb_c_open_z0, usb_c_open_w, usb_c_open_h, usb_c_open_r, wall_t + 1.00, true);
}

module usb_inner_pocket_cutter() {
    // Legacy v23 broad cleanup bay. Kept for comparison only; default OFF in v24.
    if (usb_inner_pocket) {
        translate([usb_c_center_x - (usb_c_open_w/2 + 1.15),
                   wall_t - 0.05,
                   floor_h - 0.02])
            cube([usb_c_open_w + 2*1.15,
                  3.60 + 0.05,
                  base_h - floor_h + 0.06]);
    }
}

module usb_body_pocket_cutter() {
    // v24: localized relief only around the USB-C connector body that protrudes
    // back into the case. This does not carve a tall pocket around the full opening.
    if (usb_body_pocket) {
        translate([usb_c_center_x - usb_body_pocket_w/2,
                   wall_t - 0.05,
                   usb_body_pocket_z0])
            cube([usb_body_pocket_w,
                  usb_body_pocket_depth + 0.05,
                  usb_body_pocket_h]);
    }
}

module cap_usb_notch_cutter() {
    // v25: clearance for the USB-C connector is cut ONLY from the cap's underside rim/skirt,
    // i.e. the part of the lid that inserts down into the bottom box.
    // It should not break through the visible/top outside surface of the lid.
    if (cap_usb_notch) {
        if (cap_usb_cut_visible_top) {
            // Debug/legacy behavior: through-cut the whole lid. Usually NOT what we want.
            translate([usb_c_center_x - cap_usb_notch_w/2,
                       -0.08,
                       -0.03])
                cube([cap_usb_notch_w,
                      cap_usb_notch_depth + 0.08,
                      cap_top_h + cap_rim_h + cap_top_rib_h + cap_usb_notch_extra_h]);
        } else {
            // Correct behavior: start at the underside rim level, after the top plate.
            // The rim begins at cap_rim_y0, so the cut is localized to the insert/skirt,
            // not the outside front edge of the lid.
            translate([usb_c_center_x - cap_usb_notch_w/2,
                       cap_rim_y0 - 0.08,
                       cap_top_h - cap_usb_notch_z_clear])
                cube([cap_usb_notch_w,
                      cap_usb_notch_depth + 0.08,
                      cap_rim_h + 2*cap_usb_notch_z_clear]);
        }
    }
}

// The actual solid button actuator: round pad plus narrow tongue.
module button_actuator_2d(cx) {
    union() {
        translate([cx, btn_y]) circle(r=btn_inner_r);
        rect_from_to(cx-btn_stem_w/2, btn_stem_y0, cx+btn_stem_w/2, btn_stem_y1);
    }
}

// The keyhole-shaped clearance removed from the floor.
// Subtracting this leaves the button actuator island/tongue behind.
module button_clearance_2d(cx) {
    difference() {
        union() {
            translate([cx, btn_y]) circle(r=btn_outer_r);
            rect_from_to(cx-btn_clear_w/2, btn_stem_y0, cx+btn_clear_w/2, btn_stem_y1);
        }
        button_actuator_2d(cx);
    }
}

module floor_plan_2d() {
    difference() {
        rounded_rect_2d(base_w, base_d, corner_r);

        // v6: NO front floor/channel cut here. Bottom is closed in.

        // Center through-hole.
        translate([center_hole_x, center_hole_y]) circle(r=center_hole_r);

        // Keyhole clearances, leaving the actual buttons/tongues solid.
        button_clearance_2d(btn_left_x);
        button_clearance_2d(btn_right_x);
    }
}

module base_floor() {
    linear_extrude(height=floor_h) floor_plan_2d();
}

module outer_wall_shell() {
    difference() {
        rounded_box(base_w, base_d, base_h, corner_r);

        // Hollow tray. Starts above the bottom plate.
        translate([inner_x0, inner_y0, floor_h])
            linear_extrude(height=base_h-floor_h+0.02)
                rounded_rect_2d(inner_x1-inner_x0, inner_y1-inner_y0, max(corner_r-wall_t,0.01));

        // Proper USB-C wall opening only, with closed bottom/floor below it.
        usb_c_wall_opening();

        // Keep floor/keyhole pattern from being filled by the shell body.
        translate([-0.01,-0.01,-0.01])
            linear_extrude(height=floor_h+0.02)
                difference() {
                    rounded_rect_2d(base_w+0.02, base_d+0.02, corner_r);
                    floor_plan_2d();
                }
    }
}

module side_rail(x0,x1) {
    translate([x0, rail_y0, rail_z])
        linear_extrude(height=rail_h)
            offset(r=rail_r) offset(delta=-rail_r)
                square([x1-x0, rail_y1-rail_y0], center=false);
}

module button_contact_nub(cx) {
    // A small conical contact point on the inside face of each flex button.
    // It concentrates the press at the tactile switch center and gives a little extra reach.
    translate([cx, btn_y, button_nub_z0])
        cylinder(h=button_nub_h, r1=button_nub_r1, r2=button_nub_r2);
}

module button_contact_nubs() {
    button_contact_nub(btn_left_x);
    button_contact_nub(btn_right_x);
}

module led_window_insert() {
    // v32: separate clear-PLA LED light pipe with a larger glue flange plus small ribs.
    // Print/import this as a separate aligned part. The outside face is flush at z=0.
    // Add a dot of CA/UV glue around the underside flange after printing.
    color([0.75, 1.0, 1.0, 0.45])
        union() {
            // visible flush shaft through the top lid
            translate([center_hole_x, center_hole_y, 0])
                cylinder(h=led_pipe_h + led_insert_top_proud, r=led_pipe_r);

            // tiny vertical ribs: more glue/friction surface without changing the visible circle much
            if (led_retention_ribs) {
                for (i=[0:led_retention_rib_count-1]) {
                    rotate([0,0,360*i/led_retention_rib_count])
                        translate([center_hole_x + led_pipe_r - 0.02, center_hole_y - led_retention_rib_w/2, 0.12])
                            cube([led_retention_rib_out, led_retention_rib_w, cap_top_h - 0.18]);
                }
            }

            // hidden inner glue flange/washer. Slight z overlap avoids paper-thin slicer gaps.
            translate([center_hole_x, center_hole_y, cap_top_h - 0.02])
                cylinder(h=led_flange_h, r=led_flange_r);
        }
}

module base_case() {
    // v16: bottom is a plain closed tray. No button cutouts, no LED hole, no button nubs.
    // The buttons/LED live in the snap-on top cover.
    union() {
        difference() {
            rounded_box(base_w, base_d, base_h, corner_r);

            // Hollow tray starts above the closed bottom floor.
            translate([inner_x0, inner_y0, floor_h])
                linear_extrude(height=base_h-floor_h+0.02)
                    rounded_rect_2d(inner_x1-inner_x0, inner_y1-inner_y0, max(corner_r-wall_t,0.01));

            // USB-C wall opening remains in the tray wall.
            usb_c_wall_opening();
            usb_inner_pocket_cutter();
            usb_body_pocket_cutter();
        }
        // v28: board support rails removed. Board sits directly on the 0.50 mm floor.
        // side_rail(rail_inner_left_x0, rail_inner_left_x1);
        // side_rail(rail_inner_right_x0, rail_inner_right_x1);
    }
}


module lid_2d() {
    union() {
        rounded_rect_2d(lid_w, lid_d, lid_corner_r);
        // Long centered side tabs/ears measured at Y 7.44..16.44
        rect_from_to(-lid_tab_out, lid_tab_y0, 0.05, lid_tab_y1);
        rect_from_to(lid_w-0.05, lid_tab_y0, lid_w+lid_tab_out, lid_tab_y1);
    }
}

module lid() {
    linear_extrude(height=lid_h) lid_2d();
}

module lid_half_mask_2d(left=true) {
    // v14: split left/right. The seam runs front-to-back along Y.
    if (left)
        rect_from_to(-5, -5, lid_split_x - lid_split_clearance/2, lid_d+5);
    else
        rect_from_to(lid_split_x + lid_split_clearance/2, -5, lid_w+5, lid_d+5);
}

module lid_pry_slit_2d(left=true) {
    // Notch at one end of the center seam. The two half-notches combine into a small pry slit.
    y0 = (lid_pry_slit_end == "front") ? -0.05 : lid_d - lid_pry_slit_l + 0.05;
    if (left)
        translate([lid_split_x - lid_split_clearance/2 - lid_pry_slit_w, y0])
            square([lid_pry_slit_w, lid_pry_slit_l], center=false);
    else
        translate([lid_split_x + lid_split_clearance/2, y0])
            square([lid_pry_slit_w, lid_pry_slit_l], center=false);
}

module split_lid_half_2d(left=true) {
    difference() {
        intersection() {
            lid_2d();
            lid_half_mask_2d(left);
        }
        lid_pry_slit_2d(left);
    }
}

module split_lid_ridge(left=true) {
    // Raised rib parallel to the front/back direction, right next to the split.
    // Together, the two halves form a central stiffening ridge/seam.
    ridge_x = left ? (lid_split_x - lid_split_clearance/2 - lid_reinforce_w)
                   : (lid_split_x + lid_split_clearance/2);
    translate([ridge_x, lid_reinforce_inset_y, lid_h])
        cube([lid_reinforce_w, lid_d - 2*lid_reinforce_inset_y, lid_reinforce_h]);

    // Optional small bead right at the inside edge of each half for more definition at the seam.
    if (lid_center_bead) {
        bead_x = left ? (lid_split_x - lid_split_clearance/2 - lid_center_bead_w)
                      : (lid_split_x + lid_split_clearance/2);
        translate([bead_x, lid_reinforce_inset_y, lid_h + lid_reinforce_h])
            cube([lid_center_bead_w, lid_d - 2*lid_reinforce_inset_y, lid_center_bead_h]);
    }
}

module split_lid_half(left=true) {
    union() {
        linear_extrude(height=lid_h) split_lid_half_2d(left);
        split_lid_ridge(left);
        if (lid_side_label) {
            label_txt = left ? "LEFT" : "RIGHT";
            translate([left ? lid_w*0.25 : lid_w*0.75, lid_d/2, lid_h+0.02])
                linear_extrude(height=0.18)
                    text(label_txt, size=1.4, halign="center", valign="center");
        }
    }
}

module split_lid_pair() {
    // Lay the two halves side-by-side for easy printing. The installed seam is lengthwise.
    split_lid_half(true);
    translate([lid_w + 2.0, 0, 0]) split_lid_half(false);
}


module board_preview() {
    // Thin ghost board, placed where it should sit in the tray.
    color([0,0.25,1,0.35])
        translate([board_x0, board_y0, board_preview_z])
            linear_extrude(height=0.80)
                rounded_rect_2d(board_w, board_d, board_corner_r);

    // STEP-measured tactile switch bodies and plunger centers. These should sit below the two actuator pads.
    color([1,0.15,0.05,0.38]) {
        translate([btn_left_x-switch_body_w/2, btn_y-switch_body_d/2, board_preview_z + 0.80])
            cube([switch_body_w, switch_body_d, switch_body_h]);
        translate([btn_right_x-switch_body_w/2, btn_y-switch_body_d/2, board_preview_z + 0.80])
            cube([switch_body_w, switch_body_d, switch_body_h]);
    }
    color([1,0,0,0.78]) {
        translate([btn_left_x, btn_y, board_preview_z + 2.45]) cylinder(h=0.35, r=button_preview_r);
        translate([btn_right_x, btn_y, board_preview_z + 2.45]) cylinder(h=0.35, r=button_preview_r);
    }

    // USB-C shell / mouth check at the connector wall.
    color([1,0.5,0,0.65])
        translate([usb_c_open_x0, -0.35, usb_c_open_z0])
            cube([usb_c_open_w, 0.70, usb_c_open_h]);
}



// ---------------- v15 true clamshell snap-on top ----------------
// This is NOT a left/right split lid. It is a separate top half that snaps onto the bottom tray.
// The cap is a shallow inverted tray: solid top panel + downward inner skirt/rim.
// The bottom gets small receiver reliefs/pockets for the cap snap tabs.

// Installed cap sizing. The top panel intentionally follows the same footprint as the bottom,
// while an underside rim fits into the tray opening. This keeps the outside size compact.
cap_w = base_w;
cap_d = base_d;
cap_top_h = 1.45;             // thicker/stiffer than the original 1.2 mm lid
cap_corner_r = corner_r;

// Underside plug/rim that drops into the bottom tray.
// v26 rim calculation:
// v28 geometry:
// PCB bottom = floor_h = 0.50 mm because board rails are removed.
// PCB top    = floor_h + pcb_thickness = 0.50 + 1.60 = 2.10 mm.
// Interior top = base_h = 5.50 mm, with 5.00 mm clear height above the floor.
// Rim bottom when assembled = base_h - cap_rim_h = 5.50 - 3.25 = 2.25 mm.
// This gives about 0.15 mm clearance above the PCB top.
pcb_thickness = 1.60;
board_touch_preload = 0.05;   // legacy reference only; cap_rim_h is explicit in v28
cap_rim_h = 3.15;             // v28: lid rails/skirt come down 3.25 mm into the 5.00 mm interior
cap_rim_wall = 1.05;
cap_fit_clearance = 0.22;     // clearance against the base inner opening
cap_rim_x0 = inner_x0 + cap_fit_clearance;
cap_rim_x1 = inner_x1 - cap_fit_clearance;
cap_rim_y0 = inner_y0 + cap_fit_clearance;
cap_rim_y1 = inner_y1 - cap_fit_clearance;
cap_rim_r = 1.10;

// v27: remove the rear/back rim segment so the cap only inserts on the front/sides.
// Coordinate note: front/USB side is y=0; back side is y=base_d.
cap_back_rim = false;
cap_back_rim_cut_extra = 0.25;

// Raised perimeter rib on top to fight flex without making the whole cover chunky.
// v31: disabled because the long raised edge/rib was the unwanted bit marked in red.
use_cap_top_rib = false;
cap_top_rib_h = 0.55;
cap_top_rib_w = 1.10;
cap_top_rib_inset = 1.00;

// Small pry slit/notch in the front edge so the cap can be popped back off.
cap_pry_w = 0.00;
cap_pry_h = 0.00;
cap_pry_z0 = 0.25;
cap_pry_x0 = base_w/2 - cap_pry_w/2;

// Snap tab geometry. These are low-profile flexible catches on the cap rim.
// If it is too tight, reduce snap_tab_out or increase snap_receiver_clearance.
snap_tab_w = 6.60;             // length along Y
snap_tab_h = 0.85;             // height along Z
snap_tab_out = 0.42;           // how far tabs stick outward into bottom receiver pockets
snap_tab_z = 0.75;             // from cap underside; lower tabs are easier to pry open
snap_tab_y1 = 8.20;
snap_tab_y2 = 18.15;
snap_receiver_clearance = 0.18;
snap_receiver_depth = 0.55;

// v30: receiver pockets must line up with the cap tabs after the cap is installed.
// The cap is modeled print-side-up with the rim extending upward. In the assembled case,
// the underside of the top plate sits at base_h and the rim/tabs point downward into the tray.
// Therefore a tab at model z = cap_top_h + snap_tab_z maps to installed z = base_h - snap_tab_z.
snap_receiver_z0 = base_h - snap_tab_z - snap_tab_h - snap_receiver_clearance;
snap_receiver_h  = snap_tab_h + 2*snap_receiver_clearance;

// Keep tab and receiver Y positions tied together. If you move a snap tab, the matching
// receiver pocket moves with it.
snap_positions_y = [snap_tab_y1, snap_tab_y2];

// Optional visual helpers: set true to show ghost snap-tab positions inside the bottom.
show_snap_check = false;

// Optional anti-rattle pads on the cap underside.
// v18 had these enabled, but they were floating over the cap cavity. Default OFF.
// Do not turn on unless we redesign them as ribs connected to the rim.
use_cap_board_pads = false;
cap_board_pad_h = 0.25;
cap_board_pad_w = 2.50;
cap_board_pad_d = 0.85;
cap_board_pad_z = cap_rim_h - 0.02;

module snap_receiver_cutters() {
    // v30: receiver pockets are now matched to the actual cap snap tabs.
    // Previously these were using base_h - cap_rim_h + snap_tab_z, which put the pockets
    // too low and made them visually/mechanically miss the tabs.
    for (yy = snap_positions_y) {
        // left receiver, cut into inside face of left wall
        translate([inner_x0 - snap_receiver_depth,
                   yy - snap_tab_w/2 - snap_receiver_clearance,
                   snap_receiver_z0])
            cube([snap_receiver_depth + 0.10,
                  snap_tab_w + 2*snap_receiver_clearance,
                  snap_receiver_h]);

        // right receiver, cut into inside face of right wall
        translate([inner_x1 - 0.10,
                   yy - snap_tab_w/2 - snap_receiver_clearance,
                   snap_receiver_z0])
            cube([snap_receiver_depth + 0.10,
                  snap_tab_w + 2*snap_receiver_clearance,
                  snap_receiver_h]);
    }
}

module snap_check_ghosts() {
    // Installed-position ghost of where the cap tabs land in the bottom tray.
    // These should sit directly inside the receiver pockets.
    if (show_snap_check) {
        color([1,0,0,0.35])
        for (yy = snap_positions_y) {
            translate([inner_x0 - snap_tab_out,
                       yy - snap_tab_w/2,
                       base_h - snap_tab_z - snap_tab_h])
                cube([snap_tab_out, snap_tab_w, snap_tab_h]);
            translate([inner_x1,
                       yy - snap_tab_w/2,
                       base_h - snap_tab_z - snap_tab_h])
                cube([snap_tab_out, snap_tab_w, snap_tab_h]);
        }
    }
}

module pry_relief_cutter_bottom() {
    // Matching shallow front relief on the bottom half, aligned with the cap pry notch.
    translate([cap_pry_x0, -0.03, base_h - 1.25])
        cube([cap_pry_w, wall_t + 0.10, 1.05]);
}

module bottom_clamshell() {
    union() {
        difference() {
            base_case();
            snap_receiver_cutters();
            if (bottom_pry_relief) pry_relief_cutter_bottom();
        }
        snap_check_ghosts();
    }
}

module cap_outer_top_2d() {
    rounded_rect_2d(cap_w, cap_d, cap_corner_r);
}

module cap_button_outer_clearance_2d(cx) {
    union() {
        translate([cx, btn_y]) circle(r=btn_outer_r);
        rect_from_to(cx-btn_clear_w/2, btn_stem_y0, cx+btn_clear_w/2, btn_stem_y1);
    }
}

module cap_button_actuator(cx) {
    // v18: full-thickness island/tongue, flush with the exterior AND inside face of the top.
    // v16 deliberately made these 0.90 mm thick for flex, which made them look recessed/thin.
    linear_extrude(height=top_button_flex_h) button_actuator_2d(cx);

    // Pointed/rounded nub on the inside face that reaches down to the tactile switch
    // after the cap is installed. If buttons do not click, increase top_button_nub_h.
    translate([cx, btn_y, top_button_flex_h - 0.01])
        cylinder(h=top_button_nub_h, r1=top_button_nub_r1, r2=top_button_nub_r2);
}


module button_label_one(txt, cx, cy) {
    // Outside face of the cap is z=0 in this print orientation. Cut upward from just below z=0
    // to engrave the label into the exterior face without creating a separate colored object.
    translate([cx, cy, -0.02])
        linear_extrude(height=button_label_depth + 0.03)
            text(txt, size=button_label_size, font=button_label_font, halign="center", valign="center");
}

module button_label_cutters() {
    if (button_labels) {
        label_y = button_label_on_button ? (btn_y + button_label_y_offset) : (btn_y + button_label_side_y_offset);
        button_label_one(button_label_left, btn_left_x, label_y);
        button_label_one(button_label_right, btn_right_x, label_y);
    }
}

module cap_top_plate() {
    union() {
        difference() {
            rounded_box(cap_w, cap_d, cap_top_h, cap_corner_r);

            // front fingernail notch in the cap edge
            translate([cap_pry_x0, -0.04, cap_pry_z0])
                cube([cap_pry_w, cap_top_h + 0.20, cap_pry_h]);

            // LED opening through top cover. Fill with separate led_window_insert() for clear PLA.
            // v21 uses led_hole_r instead of exact center_hole_r so the clear part is slicer-friendly.
            translate([center_hole_x, center_hole_y, -0.01])
                cylinder(h=cap_top_h + 0.04, r=led_hole_r);

            // Engraved exterior button designations, B/R by default.
            button_label_cutters();

            // Full clearance around each flex button. The thin actuator islands are added back below.
            translate([0,0,-0.01])
                linear_extrude(height=cap_top_h + 0.04) {
                    cap_button_outer_clearance_2d(btn_left_x);
                    cap_button_outer_clearance_2d(btn_right_x);
                }
        }
        cap_button_actuator(btn_left_x);
        cap_button_actuator(btn_right_x);
    }
}

module cap_underside_rim() {
    // Downward rim, modeled above the top plate for easy printing/export orientation.
    // In the actual assembled case this rim faces downward into the tray.
    // v27: the rear/back rim segment can be removed so it does not press/catch at the back of the PCB.
    rim_w = cap_rim_x1 - cap_rim_x0;
    rim_d = cap_rim_y1 - cap_rim_y0;
    translate([cap_rim_x0, cap_rim_y0, cap_top_h])
        difference() {
            difference() {
                linear_extrude(height=cap_rim_h)
                    rounded_rect_2d(rim_w, rim_d, cap_rim_r);
                translate([cap_rim_wall, cap_rim_wall, -0.01])
                    linear_extrude(height=cap_rim_h + 0.02)
                        rounded_rect_2d(rim_w - 2*cap_rim_wall, rim_d - 2*cap_rim_wall, max(cap_rim_r-cap_rim_wall, 0.05));
            }

            // Remove only the back/rear wall of the underside rim.
            // Front/USB rim and side rims remain for alignment and snap engagement.
            if (!cap_back_rim)
                translate([-cap_back_rim_cut_extra, rim_d - cap_rim_wall - cap_back_rim_cut_extra, -0.02])
                    cube([rim_w + 2*cap_back_rim_cut_extra, cap_rim_wall + 2*cap_back_rim_cut_extra, cap_rim_h + 0.04]);
        }
}

module cap_top_stiffening_rib() {
    // Low raised rib on the outside/top surface. Disabled by default in v31.
    if (use_cap_top_rib)
    translate([cap_top_rib_inset, cap_top_rib_inset, cap_top_h])
        difference() {
            linear_extrude(height=cap_top_rib_h)
                rounded_rect_2d(cap_w - 2*cap_top_rib_inset, cap_d - 2*cap_top_rib_inset, max(cap_corner_r-cap_top_rib_inset,0.2));
            translate([cap_top_rib_w, cap_top_rib_w, -0.01])
                linear_extrude(height=cap_top_rib_h + 0.02)
                    rounded_rect_2d(cap_w - 2*cap_top_rib_inset - 2*cap_top_rib_w,
                                    cap_d - 2*cap_top_rib_inset - 2*cap_top_rib_w,
                                    max(cap_corner_r-cap_top_rib_inset-cap_top_rib_w,0.05));
        }
}

module cap_snap_tabs() {
    // Tabs protrude outward from the underside rim into receiver pockets in the bottom half.
    for (yy = snap_positions_y) {
        // left tab
        translate([cap_rim_x0 - snap_tab_out, yy - snap_tab_w/2, cap_top_h + snap_tab_z])
            cube([snap_tab_out, snap_tab_w, snap_tab_h]);
        // right tab
        translate([cap_rim_x1, yy - snap_tab_w/2, cap_top_h + snap_tab_z])
            cube([snap_tab_out, snap_tab_w, snap_tab_h]);
    }
}

module cap_board_pads() {
    // Disabled by default. In v18 these appeared as two detached/floating rectangles.
    // If anti-rattle pads are needed later, make them ribs connected to the cap rim.
    if (use_cap_board_pads) {
        for (xx = [board_x0 + 3.0, board_x0 + board_w - 3.0]) {
            translate([xx - cap_board_pad_w/2, board_y0 + board_d - 2.0, cap_top_h + cap_board_pad_z])
                cube([cap_board_pad_w, cap_board_pad_d, cap_board_pad_h]);
        }
    }
}

module top_clamshell_cap() {
    difference() {
        union() {
            cap_top_plate();
            cap_underside_rim();
            cap_top_stiffening_rib();
            cap_snap_tabs();
            cap_board_pads();
        }
        cap_usb_notch_cutter();
    }
}

module top_clamshell_cap_print_oriented() {
    // For viewing/export, the cap is shown open/rim-up so the flat outside top is on the bed.
    // The rim is on top in this orientation; when installed, flip it over onto the base.
    top_clamshell_cap();
}

// Layout/export behavior:
// For multi-material LED: export/import "top" and "led_insert" as separate parts.
// They share the same XY/Z origin, so when loaded together as an assembly the insert aligns with the top hole.
// Do NOT export "top_with_led_preview" for MMU use, because STL/CSG will merge touching solids.
if (export_part == "all") {
    // LEFT: bottom tray
    bottom_clamshell();
    if (show_board_check) board_preview();

    // MIDDLE: actual snap-on TOP LID / cap, with clear LED pipe shown installed for preview.
    // For MMU export, export/import top and led_insert separately as an assembly.
    translate([base_w + part_gap, 0, 0]) {
        top_clamshell_cap_print_oriented();
        led_window_insert();
    }

    // RIGHT: duplicate clear LED insert laid out alone for visual sanity checking.
    translate([base_w + part_gap + cap_w + part_gap, 0, 0])
        led_window_insert();
}
else if (export_part == "base" || export_part == "bottom") {
    bottom_clamshell();
    if (show_board_check) board_preview();
}
else if (export_part == "led_insert") {
    // Same origin as top_clamshell_cap_print_oriented(); import this WITH "top" as an assembly.
    led_window_insert();
}
else if (export_part == "led_insert_layout") {
    // Just the insert laid out at world origin for printing separately, not aligned assembly import.
    // Use led_insert instead for aligned AMS/MMU assembly import.
    led_window_insert();
}
else if (export_part == "lid" || export_part == "top" || export_part == "cap" || export_part == "top_lid") {
    // Top only. LED insert intentionally omitted so it remains a true separate object.
    top_clamshell_cap_print_oriented();
}
else if (export_part == "top_with_led_preview") {
    // Preview only. Not recommended for export to a multi-material slicer because it may merge into one mesh.
    top_clamshell_cap_print_oriented();
    if (show_led_insert) led_window_insert();
}
else if (export_part == "assembled_preview") {
    bottom_clamshell();
    if (show_board_check) board_preview();
    // Ghosted installed cap approximation: cap shown above base for visual relation.
    color([0.0, 0.8, 1.0, 0.45]) translate([0,0,base_h]) {
        top_clamshell_cap_print_oriented();
        if (show_led_insert) led_window_insert();
    }
}
