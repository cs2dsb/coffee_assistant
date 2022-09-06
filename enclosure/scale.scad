use <hull.scad>;
use <metric_threads.scad>;

// Number of fragments in an arc
$fn = $preview ? 0 : 50;

// Little float used to fix overlaps
// i.e. difference where cutout is exactly the same as parent shape
epsilon = 0.01;

part = "base";
print = true;

case_wall_thickness = 2;
case_corner_radius = 3;

// PCB features
// All of these are relative to the corner of the PCB
//pcb_size   = [100, 100];
pcb_holes  = [
    //[4,96],
    [32,95.5],
    [64.0125,95.5],
    [94.5,95.5],
    [7.40000000000001,78.2],
    [32,55.5],
    [62.487499,55.5],
    [94.5,55.5],
    [7.03,18.21],
    [32,20.5],
    [69.5,20.5],
    [94.5,20.5],
    [4,4]];

pcb_hole_d = 3.2;
pcb_standoff_d = pcb_hole_d + 1;
pcb_standoff_h = 3;
pcb_post_d = pcb_hole_d - 0.3;
pcb_post_h = 3;
// Gap between edge of pcb and case walls
pcb_gap = 2;
pcb_offset = pcb_gap + case_wall_thickness;
pcb_origin = [pcb_offset, pcb_offset, case_wall_thickness];
pcb_cutout = [26.5, 18.04];
// This is the max size that will fit on the printer bed in X
pcb_size   = [215-(case_wall_thickness + pcb_gap) * 2, 100];

// Center of the usb socket
// '3' includes the pcb thickness and the usb socket
usb_pos  = [case_wall_thickness + pcb_gap + 15.9258, pcb_size.y + case_wall_thickness * 1.5 + pcb_gap * 2 - pcb_cutout.y, pcb_standoff_h + 3];
usb_size = [13, case_wall_thickness * 2, 4];

base_h = usb_pos.z - case_wall_thickness;// + usb_size.z / 2;
base_size = [
    pcb_size.x + (case_wall_thickness + pcb_gap) * 2,
    pcb_size.y + (case_wall_thickness + pcb_gap) * 2,
    base_h];

load_cell_hole_spacing = 7;
load_cell_pos    = [43.5, 77, case_wall_thickness + pcb_gap + 1.6];
load_cell_size   = [45, 9.2, 5];
load_cell_holes  = [[34, load_cell_size.y/2], [34+load_cell_hole_spacing, load_cell_size.y/2]];
load_cell_hole_d = 3; //it's m3


platform_size = [80, 80, 2];
platform_base_size = [15, 9, 12];
platform_screw_d = load_cell_hole_d * 1.13;
platform_screw_head_d = load_cell_hole_d * (1 + 0.13/2) * 2;
// 0.5 would be middle of the base, less is closer to the lc
platform_screw_pos = 0.4;

module pcb_holes(centers, standoff_d, standoff_h, post_d, post_h) {
    for (p=centers) {
        translate(p) {
            translate([0, 0, standoff_h/2])
                cylinder(d=standoff_d, h=standoff_h, center=true);
                translate([0, 0, standoff_h+post_h/2])
                    cylinder(d=post_d, h=post_h, center=true);
        }
    }
}

module pcb_features(origin = [0, 0, 0]) {
    translate(origin) {
        if (part == "all" || part == "posts" || part == "base") {
            pcb_holes(pcb_holes, pcb_standoff_d, pcb_standoff_h, pcb_post_d, pcb_post_h);
        }
    }
}

module usb_cutout() {
    translate(usb_pos)
        cube(usb_size, center=true);
}

module load_cell() {
    translate(load_cell_pos)
        difference() {
            cube(load_cell_size);
            for (h = load_cell_holes) {
                translate([0, 0, load_cell_size.z / 2])
                    translate(h)
                        cylinder(h=load_cell_size.z+epsilon, d=load_cell_hole_d, center=true);
            }
        }
}

module case_bottom() {
    if (part == "all" || part == "base") {
        c_x = pcb_cutout.x;
        c_y = pcb_cutout.y;

        a_points = [
            [0, 0, 0],
            [base_size.x, 0, 0],
            [base_size.x, base_size.y - pcb_cutout.y, 0],
            [0, base_size.y - pcb_cutout.y, 0],
        ];

        b_points = [
            [pcb_cutout.x, base_size.y - pcb_cutout.y - case_corner_radius * 3, 0],
            [base_size.x, base_size.y - pcb_cutout.y - case_corner_radius * 3, 0],
            [base_size.x, base_size.y, 0],
            [pcb_cutout.x, base_size.y, 0],
        ];

        difference() {
            union() {
                rounded_box2(
                    shrink(a_points, case_corner_radius),
                    case_corner_radius,
                    // Doubled because we don't want the top curve
                    base_size.z * 2);
                rounded_box2(
                    shrink(b_points, case_corner_radius),
                    case_corner_radius,
                    // Doubled because we don't want the top curve
                    base_size.z * 2);
            }

            union() {
                rounded_box2(
                    shrink(a_points, case_corner_radius+case_wall_thickness),
                    case_corner_radius,
                    // Doubled because we don't want the top curve
                    base_size.z * 2);
                rounded_box2(
                    shrink(b_points, case_corner_radius+case_wall_thickness),
                    case_corner_radius,
                    // Doubled because we don't want the top curve
                    base_size.z * 2);

                // Chop off the top
                translate([-1, -1, base_size.z * 2])
                    cube([base_size.x+10, base_size.y+10, base_size.z*10]);

                usb_cutout();
            }

        };

    }
}

module case_features() {
    case_bottom();
}

module platform() {
    module hole() {
        screw_drill_h = platform_base_size.z + platform_size.z + 0.2;
        bevel_h = (platform_screw_head_d-platform_screw_d)/2;
        union() {
            translate([0, 0, -platform_size.z-0.1])
                cylinder(d=platform_screw_d, h=screw_drill_h);

            translate([0, 0, -platform_size.z-0.1-screw_drill_h+(platform_size.z+platform_base_size.z)*(1-platform_screw_pos)])
                cylinder(d=platform_screw_head_d, h=screw_drill_h);

            translate([0, 0, -platform_size.z-0.2-screw_drill_h+(platform_size.z+platform_base_size.z)*(1-platform_screw_pos)+screw_drill_h])
                linear_extrude(bevel_h, scale=platform_screw_d/platform_screw_head_d)
                circle(d=platform_screw_head_d);

        }
    }

    if (part == "all" || part == "platform") {
        t = print ? [0, 0, 0] : [81.2, 81.5, case_wall_thickness + pcb_gap + 1.6 + load_cell_size.z + platform_base_size.z];
        r = print ? [0, 0, 0] : [0, 180, 0];


        translate(t)
        rotate(r)
            difference() {
                union() {
                    translate([-platform_base_size.x/2, -platform_base_size.y/2, 0])
                        cube(platform_base_size);

                    translate([-platform_size.x/2, -platform_size.y/2, -platform_size.z])
                        cube(platform_size);
                }


                translate([load_cell_hole_spacing/2, 0, 0])
                hole();
                translate([-load_cell_hole_spacing/2, 0, 0])
                hole();


            // load_cell_hole_spacing
            // platform_screw_d
            // platform_screw_head_d
            // platform_screw_pos
        }
    }
}

pcb_features(pcb_origin);
case_features();
platform();

//% load_cell();


