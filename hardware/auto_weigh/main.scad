use <gears.scad>;
use <polyround.scad>;

$fn = $preview ? 50 : 150;

//TODO: scoop thing at the part guide exit making sure parts drop down vertically

print = !$preview;
part = [ "_dummy"

    , "frame_bottom"
    , "frame_side"
    , "bottom_gear"
    , "top_gear"
    , "side_gear"
    , "stirrer"
    , "bottom_gear_shaft"
    , "plate"
    , "plate_cup"
    , "part_guide_left"
    , "part_guide_top"
    , "motor_cup"
];

debug_viz = $preview && false;
cross_section = false;
hide_gears = $preview && false;

epsilon     = 0.01;

crush_rib_n = 6;
crush_rib_w = 0;//1.2;
crush_rib_id_factor = 0.6;
crush_rib_od_factor = 0.75;

min_floor_h = 1.0;
min_wall_w  = 1.5;
clearance   = 0.6;

bearing_od  = 22;
bearing_id  = 8 + 0.1;
bearing_id_ring_d = 12;
bearing_h   = 7;

plate_d = 200;
plate_h = min_floor_h * 2;
plate_clearance = clearance;

stirrer_d = plate_d / 3;
stirrer_h = plate_h * 5;
stirrer_dent_n = 5;
stirrer_dent_depth = stirrer_d * 0.022;

stepper_shaft_d = 5;
stepper_shaft_cutout = 0.5;

center_shaft_square_w = 6;

motor_face_plate_w = 41;
motor_shaft_l      = 25;
min_shaft_mesh     = 16;

taper_top_bore = stepper_shaft_d * 1.05;
taper_top_d = bearing_od + crush_rib_w * crush_rib_od_factor + 2 * min_wall_w;
taper_bottom_d = 25 + 4*min_wall_w;
// We want a 45 degree (min) slope
taper_h = max((taper_bottom_d - taper_top_d) / 2, motor_shaft_l-min_shaft_mesh);

bottom_bearing_h = taper_h + min_floor_h * 2;

top_gear_extra_mesh = -0.5;

bottom_gear_z = bottom_bearing_h + bearing_h + min_floor_h;

modul = 1.2;
gear_teeth = 30;
pinion_teeth = 11;
axis_angle = 90;
tooth_width = 5;
pressure_angle = 20;
helix_angle = 30;
herringbone = true;

gear_gap = modul * pinion_teeth * 1.4;
top_bearing_z = bottom_bearing_h + gear_gap + 6.5;

side_gear_z = bottom_gear_z + gear_gap / 2;

coupling_side = (bearing_id - crush_rib_w * crush_rib_id_factor) / sqrt(2);

// Gear lib includes some clearance but it's not always enough
side_gear_scale = 0.97;

heatset_d = 4.5;
heatset_h = 6;

part_guide_h = min_floor_h * 6;
guide_frame_w = min_wall_w;
exit_guide_w = 20 / 2; // 20 is real world, /2 for POC
exit_guide_protrusion = 20;

screw_head_h = 2.2;
plate_cup_floor_h = min_floor_h * 2 + screw_head_h;
frame_side_h = side_gear_z + bearing_od/2 + min_wall_w * 2;
plate_z = frame_side_h
        + plate_cup_floor_h
        + plate_clearance;

stirrer_z = plate_z
        + plate_clearance
        + plate_h;

plate_cup_d = plate_d + 2 * (clearance + min_wall_w);
plate_cup_h = plate_cup_floor_h + plate_h + plate_clearance + part_guide_h;

entrance_slope_angle = 18;
exit_chute_left = 10;
exit_chute_width = 25;
exit_chute_right = exit_chute_left + exit_chute_width;
exit_chute_height = 10;

screw_hole_d = 3.5;
screw_head_d = 5.7;

guide_curve_samples = 100;
guide_curve_factor = 1.5;
guide_curve_cutoff = 0.6;

module part_enabled(part_name) {
    assert(is_string(part_name), "part_enabled param must be a string");

    if (is_string(part) && (part_name == part || part == "all")) {
        children();
    }

    if (is_list(part) && !is_list(search([part_name], part)[0])) {
        children();
    }
}

// Translates and rotates a part if we aren't printing (i.e. for visualizing the assembly)
module layout_part(part_name, t=[0,0,0], r=[0,0,0]) {
    part_enabled(part_name) {
        if (!print) {
            translate(t)
            rotate(r)
            children();
        } else {
            children();
        }
    }
}

function reverse(list) = [for (i = [len(list)-1:-1:0]) list[i]];

function curve_tween(alpha) = sqrt(1 - pow(alpha - 1, 2));
function out_circ_tween(x) = 1 - pow(1 - x, 4);

function tween_point(p0, p1, alpha, tween) = [
    p0[0] + (p1[0] - p0[0]) * alpha,
    p0[1] + (p1[1] - p0[1]) *
        (tween == 0 ? curve_tween(alpha) : out_circ_tween(alpha)),
];
function tween_points(p0, p1, samples, cutoff=1, tween=0) =
    [ for (i=[0:samples*cutoff]) tween_point(p0, p1, i * (1 / samples), tween) ];

// Works out the y distance from the centerline of a circle at a
// given x offset from the center
function y_to_rim_for_x(r, x) = sqrt(pow(r, 2) - pow(x, 2));


function radius_point(r, angle, flip=[1, 1]) = [
    r * cos(angle) * flip.x,
    r * sin(angle) * flip.y,
];

// Gives a point along the entrance slope where l=0 is exactly the point of tangency to the
// circle given by r and slope given by angle.
function entrance_slope_point(l, r, angle) = [
    // X offset due to intercept not being at x=0
    radius_point(r, 90-angle, [-1, 1]).x
    // X offset due to angle of slope
    - radius_point(r, 90-angle, [-1, 1]).y / tan(angle)
    // X along slope
    + l * cos(angle),
    // Y along slope
    l * sin(angle),
];

// Applies mx+c for x where m and c are calculated from the slope
// given by entrance_slope_point with radius r and given angle
function tangent_mx_f(x, r, angle) =
    mx(entrance_slope_point(0, r, angle), entrance_slope_point(10, r, angle))[0] * x
    + mx(entrance_slope_point(0, r, angle), entrance_slope_point(10, r, angle))[1];

function mx(p0, p1) = [
    // m
    (p1.y - p0.y) / (p1.x - p0.x),
    // c
    p1.y - ((p1.y - p0.y) / (p1.x - p0.x)) * p1.x,
];

module heatset() {
    cylinder(
        d = heatset_d,
        h = heatset_h
    );
}

module bearing() {
    od = bearing_od;
    id = bearing_id;

    difference() {
        cylinder(
            h = bearing_h,
            d = od
        );

        translate([0, 0, -epsilon])
        cylinder(
            h = bearing_h
                + epsilon * 2,
            d = bearing_id
        );
    };
}

module bearing_od_cup() {
    od = bearing_od + crush_rib_w * crush_rib_od_factor;
    floor_h = min_floor_h;
    wall_w = min_wall_w;

    module rib() {
        module slope() {
            intersection() {
                linear_extrude(
                    crush_rib_w * 0.9,
                    scale = 0.001
                )
                square(
                    [crush_rib_w * 1, crush_rib_w * 20],
                    center = true
                );

                cylinder(
                    h = crush_rib_w * 2,
                    d = crush_rib_w
                );
            }

        }
        union() {
            translate([0, 0, crush_rib_w])
            cylinder(
                h = bearing_h
                    - 2 * crush_rib_w,
                d = crush_rib_w
            );

            translate([0, 0, crush_rib_w])
            mirror([0, 0, 1])
                slope();

            translate([0, 0, bearing_h - crush_rib_w])
                slope();
        };
    }

    translate([0, 0, -floor_h])
    union() {
        difference() {
            cylinder(
                h = bearing_h
                    + floor_h,
                d = od
                    + 2 * wall_w
            );

            translate([0, 0, floor_h])
            cylinder(
                h = bearing_h
                    + epsilon,
                d = od
            );

            translate([0, 0, -epsilon])
            cylinder(
                h = floor_h
                    + epsilon * 2,
                d = od - wall_w * 2
            );
        };

        for(i = [1:crush_rib_n]) {
            r = i * (360/crush_rib_n);

            rotate([0, 0, r])
            translate([od / 2, 0, floor_h])
            rib();
        }
    }
}

module bearing_id_cup(foot=true, bore=true) {
    od = bearing_id - crush_rib_w * crush_rib_id_factor;
    floor_h = foot ? min_floor_h * 2 : 0;
    wall_w = min_wall_w;

    module rib() {
        module slope() {
            intersection() {
                linear_extrude(
                    crush_rib_w * 0.9,
                    scale = 0.001
                )
                square(
                    [crush_rib_w * 1, crush_rib_w * 20],
                    center = true
                );

                cylinder(
                    h = crush_rib_w * 2,
                    d = crush_rib_w
                );
            }

        }
        union() {
            translate([0, 0, 0])
            cylinder(
                h = bearing_h
                    // - 2 * crush_rib_w,
                    - crush_rib_w,
                d = crush_rib_w
            );

            // This would be a slope at the bottom, which is not needed
            // Commented out to improve strength on frame to bearing interface
            // translate([0, 0, crush_rib_w])
            // mirror([0, 0, 1])
            //     slope();

            translate([0, 0, bearing_h - crush_rib_w])
                slope();
        };
    }

    translate([0, 0, -floor_h])
    union() {
        difference() {
            union() {
                cylinder(
                    h = bearing_h
                        + floor_h,
                    d = od
                );

                if (foot) {
                    linear_extrude(
                        floor_h
                        + epsilon * 2,
                        scale = (bearing_id - 2 * wall_w) / bearing_id
                    )
                    circle(d = bearing_id + floor_h * 2);
                }
            }

            if (bore) {
                translate([0, 0, -epsilon])
                cylinder(
                    h = bearing_h
                        + floor_h
                        + epsilon * 2,
                    d = od
                        - 2 * wall_w
                );
            }
        };

        for(i = [1:crush_rib_n]) {
            r = i * (360/crush_rib_n);

            rotate([0, 0, r])
            translate([od / 2, 0, floor_h])
            rib();
        }
    };
}

module bearing_id_cup_clearance_rod() {
    wall_w = min_wall_w;
    od = bearing_id
        - crush_rib_w * 0.75
        - wall_w * 2
        - clearance;
    floor_h = min_floor_h;

    union() {
        cylinder(
            h = bearing_h * 2
                + floor_h,
            d = od
        );

        cylinder(
            h = floor_h,
            d = od * 1.5
        );
    }
}

module shaft_gear(pinion=false, support=true) {
    outer_d = modul * gear_teeth;
    cup_d = bearing_od + 2 * min_wall_w;
    delta = outer_d - cup_d;
    support_h = delta / sqrt(2);

    translate([0, 0, support ? - (support_h) : 0]) {
        translate([0, 0, (support ? support_h : 0) + min_floor_h])
        if (hide_gears) {
        } else if (!herringbone) {
            bevel_gear_pair(
                modul=modul,
                gear_teeth=gear_teeth,
                pinion_teeth=pinion_teeth,
                axis_angle=axis_angle,
                tooth_width=tooth_width,
                gear_bore=0,
                pinion_bore=2,
                pressure_angle=pressure_angle,
                helix_angle=helix_angle,
                together_built=true,
                pinion=pinion,
                bevel=!pinion
            );
        } else {
            bevel_herringbone_gear_pair(
                modul=modul,
                gear_teeth=gear_teeth,
                pinion_teeth=pinion_teeth,
                axis_angle=axis_angle,
                tooth_width=tooth_width,
                gear_bore=0,
                pinion_bore=0,
                pressure_angle=pressure_angle,
                helix_angle=helix_angle,
                together_built=true,
                pinion=pinion,
                bevel=!pinion
            );
        }

        if (!pinion) {
            translate([0, 0, support ? support_h: 0])
            cylinder(
                h = min_floor_h,
                d = outer_d
            );

            if (support) {
                difference() {
                    linear_extrude(
                        support_h,
                        scale = outer_d / cup_d
                    )
                    circle(d = cup_d);

                    // translate([0, 0, support_h - (bearing_h + min_floor_h)])
                    // cylinder(
                    //     d = bearing_od + 2 * min_wall_w,
                    //     h = bearing_h + min_floor_h
                    // );
                };

            }
        }
    }
}

module side_mount_drills() {
    radius = 5;
    ml = 31/2;

    union() {
        translate([0, -radius, -epsilon])
        children();

        translate([ml-radius*1.5, -ml+min_floor_h+radius/2.5, -epsilon])
        children();

        translate([-(ml-radius*1.5), -ml+min_floor_h+radius/2.5, -epsilon])
        children();
    };
}

module top_mount_drills() {
    x = bearing_h + min_floor_h;
    y = motor_face_plate_w;

    union() {
        translate([x / 2, y / 2 - heatset_d, 0])
        children();
        translate([x / 2, - y / 2 + heatset_d, 0])
        children();
    };
}

module screw_hole() {
    translate([0, 0, -epsilon])
    union() {
        cylinder(
            h = min_floor_h * 2 + 3 * epsilon,
            d = screw_hole_d
        );

        translate([0, 0, min_floor_h * 2 + 2 * epsilon])
        cylinder(
            h = min_floor_h * 10,
            d = screw_head_d
        );
    }
}

module frame_bottom() {
    wall_w = min_wall_w;
    floor_h = min_floor_h * 2;

    radius = 5;
    l = motor_face_plate_w;
    hl = l / 2;
    ml = 31/2;

    difference() {
        union() {
            // Cup
            translate([0, 0, taper_h + floor_h])
            difference() {
                bearing_od_cup();
                translate([0, 0, -epsilon])
                cylinder(
                    d = taper_top_bore,
                    h = bearing_h + 2 * epsilon
                );
            };

            // Taper
            translate([0, 0, floor_h])
            difference() {
                linear_extrude(
                    taper_h,
                    scale = taper_top_d  / taper_bottom_d
                )
                circle(d = taper_bottom_d);

                translate([0, 0, -epsilon])
                linear_extrude(
                    taper_h + 2 * epsilon,
                    scale = (taper_top_d - wall_w*4) / taper_bottom_d
                )
                circle(d = taper_bottom_d - wall_w*4);

                translate([0, 0, taper_h - bearing_h * 0.5])
                cylinder(
                    d = taper_top_bore,
                    h = bearing_h + 2 * epsilon
                );
            };

            points = [
                // x, y, radius
                 [ -hl, -hl, radius]
                ,[ -hl, hl, radius]
                ,[ hl, hl, radius]
                ,[ hl, -hl, radius]
            ];

            // Plate
            difference() {
                linear_extrude(floor_h)
                    polygon(polyRound(points, $fn));

                translate([0, 0, -epsilon])
                cylinder(
                    h = floor_h + 2 * epsilon,
                    d = taper_bottom_d - wall_w*4
                );
            };

            // Side mount
            // translate([-31/2+3.5, hl - (heatset_h + min_wall_w), 0])
            // linear_extrude(
            // cube([
            //     31-3.5*2, heatset_h + min_wall_w, 10
            // ]);


            translate([0, hl, ml+3])
            rotate([90, 0, 0])
            linear_extrude(heatset_h + min_wall_w)
            polygon(polyRound([
                // x, y, radius
                 [ -ml, -ml-3, radius/2.5]
                // ,[ -hl, hl, radius]
                ,[ 0, 0, radius]
                ,[ ml, -ml-3, radius/2.5]
            ], $fn));


            translate([0, -hl+heatset_h + min_wall_w, ml+3])
            rotate([90, 0, 0])
            linear_extrude(heatset_h + min_wall_w)
            polygon(polyRound([
                // x, y, radius
                 [ -ml, -ml-3, radius/2.5]
                // ,[ -hl, hl, radius]
                ,[ 0, 0, radius]
                ,[ ml, -ml-3, radius/2.5]
            ], $fn));

        };

        translate([0, hl, ml])
        rotate([90, 0, 0])
            side_mount_drills()
            heatset();

        translate([0, -hl+heatset_h-epsilon*2, ml])
        rotate([90, 0, 0])
            side_mount_drills()
            heatset();

        for (i = [1:4]) {
            r = (i + 0.5) * (360/4);

            rotate([0, 0, r])
            translate([31*sqrt(2)/2, 0, 0])
            screw_hole();
        }
    };
}

module bottom_gear_shaft() {
    module d_shaft() {
        translate([0, 0, -bearing_h-epsilon])
        intersection() {
            cylinder(
                d = stepper_shaft_d,
                h = bearing_h * 3
            );
            translate([stepper_shaft_cutout, 0, 0])
            cube(
                [stepper_shaft_d, stepper_shaft_d, bearing_h * 10],
                center = true
            );
        };
    }

    center_shaft_z = 10;

    shaft_clearance_d = bearing_id - crush_rib_w * crush_rib_id_factor;

    difference() {
        union() {
            // Bearing id `cup` + stepper shaft coupling
            mirror([0, 0, 1])
                bearing_id_cup(foot=false, bore=false);

            // Top shaft coupling
            // Bearing id cup
            translate([0, 0, top_bearing_z-bottom_gear_z+min_floor_h])
                bearing_id_cup(foot=false, bore=false);

            // Smooth shaft
            linear_extrude(top_bearing_z-bottom_gear_z+min_floor_h)
            circle(d = bearing_id + min_wall_w);

            linear_extrude(min_floor_h)
            circle(d = (bearing_id + crush_rib_w * crush_rib_id_factor) * sqrt(2));

            translate([0, 0, min_floor_h])
            linear_extrude(coupling_side)
            square(
                [
                    bearing_id + crush_rib_w * crush_rib_id_factor,
                    bearing_id + crush_rib_w * crush_rib_id_factor,
                ],
                center = true);

            // Top coupling
            translate([0, 0,
                top_bearing_z-bottom_gear_z+bearing_h+min_floor_h
            ]) {
                cube([
                    coupling_side,
                    coupling_side,
                    coupling_side,
                ], center = true);
            };



        };
        translate([0, 0, top_bearing_z-bottom_gear_z+bearing_h - heatset_h / 2 + epsilon+min_floor_h])
        heatset();

        d_shaft();
    };
}

module bottom_gear() {
    difference() {
        union() {
            // Gear
            shaft_gear(support=false);

            // Coupling housing
            translate([0, 0, min_floor_h])
            linear_extrude(coupling_side-min_floor_h)
            circle(d = (bearing_id + crush_rib_w * crush_rib_id_factor + min_wall_w + clearance) * sqrt(2));
        };

        side = bearing_id + crush_rib_w * crush_rib_id_factor + clearance/4;
        linear_extrude(coupling_side+min_floor_h+epsilon)
        square([side, side], center = true);
    };
}

module top_gear() {
    side_top = bearing_od + crush_rib_w * crush_rib_od_factor + min_wall_w * 2;
    side_bottom = modul * gear_teeth;

    module gear() {
        // The gear, adjusted relative to this part to give extra gear meshing if necessary
        translate([0, 0, -top_gear_extra_mesh])
        rotate([180, 0, 0])
        shaft_gear(support=false);
    }

    module key() {
        // total height from the bottom of the bearing up to the top plane of the cup
        tot_h = frame_side_h
            + plate_cup_floor_h
            + plate_clearance
            + plate_h
            - top_bearing_z;

        square_h = plate_h;

        intersection() {
            // Over-sized cylinder
            cylinder(
                d = side_bottom,
                h = bearing_h * 10
            );

            union() {
                // Square on top
                translate([0, 0, tot_h - square_h])
                linear_extrude(square_h)
                square([side_top, side_top], center = true);

                // Taper
                translate([0, 0, (plate_h + plate_clearance)])
                linear_extrude(
                    tot_h - square_h - (plate_h + plate_clearance),
                    scale = side_top / side_bottom
                )
                square([side_bottom, side_bottom], center = true);

                // This offset before the start of the taper is just
                // to retain the old angle so i don't have to reprint
                // the plate...
                cylinder(d = side_bottom, h = plate_h + plate_clearance);
            }
        }
    }


    union() {
        bearing_od_cup();

        difference() {
            union() {
                gear();
                key();
            };

            // Full length cutout to make sure everything is clear for bearing
            translate([0, 0, -bearing_h])
            cylinder(
                d = bearing_od + crush_rib_w * crush_rib_od_factor,
                h = bearing_h * 3
            );
        };
    };
}

module plate() {
    meat_h = plate_cup_floor_h + plate_clearance;

    // TODO: FIX
    rotate([0, 180, 0])
    // Plate
    translate([0, 0, 0])
    difference() {
        union() {
            // The plate
            cylinder(
                d = plate_d,
                h = plate_h
            );

            // The meat to cut the key out of
            translate([0, 0, -meat_h ])
            cylinder(
                d = modul * gear_teeth + 2 * min_wall_w,
                h = meat_h
            );
        };


        // Required hole for bearing to be installed
        translate([0, 0, -epsilon-meat_h])
        cylinder(
            d = bearing_od + 2 * min_wall_w,
            h = meat_h + plate_h + 2*epsilon
        );

        translate([0, 0, -(frame_side_h+plate_cup_floor_h-top_bearing_z-epsilon)])
        scale([1.005, 1.005, 1])
        top_gear();

    };
}

module plate_cup() {
    cup_h = plate_cup_h;
    cup_d = plate_cup_d;

    holder_x = stirrer_d / 3 - heatset_d;
    holder_w = cup_d/2 - holder_x + heatset_d;

    // TODO: maths too hard to get protrusion right
    holder_y = plate_d / 2;
    holder_l = exit_guide_protrusion * 2;

    module heatsink_boss(angle=0) {
        d = min_wall_w * 2 + heatset_d;

        rotate([0, 0, angle])
        translate([cup_d/2 + d / 2 - min_wall_w, 0, 0])
        linear_extrude(plate_cup_h)
        difference() {
            circle(d = d);
            circle(d = heatset_d);
        }
    }

    module bosses() {
        // Bottom of the fixed left hand guide
        heatsink_boss(70.6);

        // Top/side of the fixed left hand guide
        heatsink_boss(-31.89);

        // Bottom of the movable guide
        heatsink_boss(166.7);

        // Top of the movable guide
        heatsink_boss(-72);
    }

    // Plate
    difference() {
        union() {
            // Cup floor
            cylinder(
                d = cup_d,
                h = plate_cup_floor_h
            );

            bosses();

            // Cup wall
            translate([0, 0, plate_cup_floor_h])
            rotate([0, 0, 65])
            rotate_extrude(
                angle = 267,
                convexity = 2
            )
            translate([cup_d/2-min_wall_w*5, 0, 0])
            square([min_wall_w*5, cup_h - plate_cup_floor_h]);
        };


        // Plate cutout
        translate([0, 0, plate_cup_floor_h + epsilon])
        cylinder(
            d = plate_d + 2 * clearance,
            h = cup_h * 2
        );

        // Clearance under the exit chute
        translate([0, 0, -epsilon])
        rotate([0, 0, 10])
        rotate_extrude(
            angle = 55,
            convexity = 2
        )
        translate([cup_d/2-min_wall_w*4, 0, 0])
        square([min_wall_w*5, cup_h - plate_cup_floor_h]);

        // Big hole for plate shaft to pass through
        cube(motor_face_plate_w, center = true);

        // Screw holes
        for (m = [0, 1]) {
            mirror([m, 0, 0])
            translate([
                motor_face_plate_w / 2,
                0,
                0, //- screw_head_h,
            ])
            top_mount_drills()
                screw_hole();
        }
    };
}


module frame_side() {
    x = bearing_h + min_floor_h;
    y = motor_face_plate_w;
    z = frame_side_h;

    rotate([0, -90, 0])
    union() {
        difference() {
            // Square wall
            translate([0, -y / 2, 0])
            cube([x, y, z]);

            // Cutout for the bearing
            translate([- epsilon, 0, side_gear_z])
            rotate([0, 90, 0])
            cylinder(
                h = bearing_h + min_floor_h + 2 * epsilon,
                d = bearing_od + 2 * min_wall_w - 2 * epsilon
            );

            // Screw holes to attach to bottom
            translate([- epsilon, 0,  + 31/2])
            rotate([90, 0, 90])
            side_mount_drills() {
                screw_hole();
                translate([0, 0, min_floor_h * 2])
                scale([2, 2, 3])
                screw_hole();
            }

            //Heatset holes to attach the next part
            translate([0, 0, z - heatset_h + epsilon])
            top_mount_drills()
                heatset();
        };

        // Bearing holder
        translate([min_floor_h, 0, side_gear_z])
        rotate([0, 90, 0])
        rotate([0, 0, 360/crush_rib_n/2])
        bearing_od_cup();
    }

}

module side_pinion() {
    outer_d = modul * gear_teeth;
    pinion_d = modul * pinion_teeth;
    shaft_l = (motor_face_plate_w - outer_d) / 2 + min_floor_h;

    t = print ?
        [0, 0, shaft_l + bearing_h - 1] :
        [outer_d/2, 0, side_gear_z];
    r = print ?
        [0, 0, 0] :
        [0, -90, 0];

    delta_pinion = atan(sin(axis_angle)/(gear_teeth/pinion_teeth+cos(axis_angle)));// Cone Angle of the Pinion

    cup_od = bearing_id + crush_rib_w * crush_rib_id_factor;

    translate(t)
    rotate(r)
    union() {
        if (!hide_gears) {
            // Gear
            translate([0, 0, -1.5])
            rotate([0, 0, 26])
            scale([side_gear_scale, side_gear_scale, 1])
            difference() {
                if (!herringbone) {
                    bevel_gear(
                        modul=modul,
                        tooth_number=pinion_teeth,
                        tooth_width=tooth_width+2,
                        bore=0,
                        pressure_angle=pressure_angle,
                        helix_angle=-helix_angle,
                        partial_cone_angle=delta_pinion
                    );
                } else {
                    bevel_herringbone_gear(
                        modul=modul,
                        tooth_number=pinion_teeth,
                        partial_cone_angle=delta_pinion,
                        tooth_width=tooth_width+2,
                        bore=0,
                        pressure_angle=pressure_angle,
                        helix_angle=-helix_angle
                    );
                }

                // Chop off the tips of the teeth (the gear is longer to allow this)
                // Flat so it's support free
                translate([0, 0, tooth_width+2 - modul/3.5])
                cylinder(
                    d = pinion_d*2,
                    h = pinion_d
                );
            }
        }

        // Shaft
        translate([0, 0, -shaft_l])
        // Taper for printing gear side up
        // linear_extrude(
        //     shaft_l-min_floor_h,
        //     scale = (pinion_d +modul *2) / cup_od
        // )
        // circle(d = cup_od);
        // Flat for printing gear down
        linear_extrude(shaft_l-1.5)
        circle(d = pinion_d - modul * 2 - 1.5/2);

        // Cup
        rotate([180, 0, 0])
        translate([0, 0, shaft_l])
        bearing_id_cup(foot=false, bore=false);
    };
}

module stirrer() {
    // bottom of stirrer is here
    stirrer_0 = frame_side_h + plate_cup_floor_h + plate_clearance * 2 + plate_h;

    // top of top bearing is here
    boss_0 = top_bearing_z + bearing_h;

    // therefore boss shaft h needs to be...
    race_boss_h = stirrer_0 - boss_0;

    // TODO: FIX
    rotate([0, 180, 0])
    difference() {
        union() {
            // Plate
            linear_extrude(
                stirrer_h,
                scale = 0.95
            )
            difference() {
                union() {
                    circle(d = stirrer_d);
                };

                for(i = [1:stirrer_dent_n]) {
                    r = i * (360/stirrer_dent_n);

                    rotate([0, 0, r])
                    translate([stirrer_d - pow(stirrer_dent_depth, i), 0, -epsilon])

                    circle(d = stirrer_d);
                }
            };


            // Support that contacts bearing race
            translate([0, 0, -race_boss_h])
            cylinder(
                h = race_boss_h,
                d = bearing_id_ring_d
            );
        };

        translate([0, 0,
            - race_boss_h
            + stirrer_h
            + bearing_id * 0.5
            - stirrer_h/2
            - heatset_h/2
            -coupling_side*1.05/2
        ])
        screw_hole();

        // Just to make sure it pokes through
        cylinder(d = screw_head_d, h =stirrer_h + epsilon);

        translate([-coupling_side * 1.05 / 2, -coupling_side * 1.05 / 2, -epsilon - race_boss_h])
        cube([
            coupling_side * 1.05,
            coupling_side * 1.05,
            heatset_h / 2 * 1.1,
        ]);
    };

}

module part_guide_left() {
    exit_chute_overhang = 1;

    // If this is adjusted the angle of the guide close to the spinner
    // may also need to be tweaked because it isn't calculated properly
    wall_w = min_wall_w * 2;
    floor_h = min_floor_h * 1.5;
    plate_r = plate_d/2;
    cup_r = plate_cup_d/2;
    stirrer_r = stirrer_d/2;

    cup_wall_h = plate_cup_h
                - plate_cup_floor_h
                - plate_clearance
                - plate_h
                - plate_clearance;
    heatset_boss_d = min_wall_w * 2 + heatset_d;

    exit_chute_left_len = y_to_rim_for_x(plate_r, plate_r - exit_chute_left);
    exit_chute_right_len = y_to_rim_for_x(plate_r, plate_r - exit_chute_right);

    module exit_chute() {
        module chute_shape(extra_overhang=-epsilon*10, y0=epsilon, x_inset=0) {
            points = [
                [x_inset, y0],
                for (x = [x_inset: exit_chute_width-x_inset])
                    [x, - y_to_rim_for_x(plate_r - extra_overhang, plate_r - (exit_chute_left + x))
                        - exit_chute_overhang
                    ],
                [exit_chute_width-x_inset, y0],
            ];

            linear_extrude(exit_chute_height)
            polygon(points);
        }

        module chute_support() {
            l = plate_r
                - exit_chute_right
                - stirrer_r
                - clearance / 2;

            module shape(x_inset=0, y_inset=0, y_origin=0) {
                points = [
                    [0, y_origin],
                    for (x = [0: l - x_inset])
                        [x, - y_to_rim_for_x(plate_r - y_inset,  plate_r - (exit_chute_right + x))
                            - exit_chute_overhang],
                    [l - x_inset, y_origin],
                ];

                eps = x_inset != 0 || y_inset != 0? epsilon : 0;
                translate([0, eps, -eps])
                linear_extrude(exit_chute_height + eps * 2)
                polygon(points);
            }

            union() {
                difference() {
                    shape();
                    shape(wall_w, wall_w * 1);
                    translate([0, 0, -(exit_chute_height - cup_wall_h)])
                    shape(0, -epsilon*2, y_origin=-exit_chute_right_len+wall_w*2);
                }

                boss_pos = [
                    l - 1,
                    -y_to_rim_for_x(plate_r + (heatset_boss_d - min_wall_w + clearance) / 2, l)
                ];

                translate([0, 0, cup_wall_h])
                difference() {
                    linear_extrude(exit_chute_height - cup_wall_h)
                    translate(boss_pos)
                    circle(d = heatset_boss_d);

                    translate(boss_pos)
                    screw_hole();
                }
            }
        }

        module chute_support_top() {
            x0 = -cup_r+exit_chute_left+(cup_r-plate_r);
            r = cup_r + (heatset_boss_d - min_wall_w + clearance) / 2 + 0.1;
            curve_l = (heatset_boss_d * 2 - min_wall_w);

            // Straight bit, full height
            linear_extrude(exit_chute_height)
            polygon([
                [0, -epsilon],
                [0, y_to_rim_for_x(cup_r - wall_w - clearance, x0)],
                [wall_w, y_to_rim_for_x(cup_r - wall_w - clearance, x0 + wall_w)],
                [wall_w, -epsilon],
            ]);


            translate([0, 0, cup_wall_h])
            difference() {
                linear_extrude(exit_chute_height - cup_wall_h)
                union() {
                    polygon([
                        [0, -epsilon],
                        [0, y_to_rim_for_x(r, x0)],
                        [heatset_boss_d / 2, y_to_rim_for_x(r, x0)],
                        [wall_w, y_to_rim_for_x(r, x0 + wall_w / 2)],
                        [wall_w, -epsilon],
                    ]);

                    translate([wall_w / 2, y_to_rim_for_x(r, x0)])
                    circle(d = heatset_boss_d);
                }

                translate([wall_w / 2, y_to_rim_for_x(r, x0)])
                screw_hole();
            }
        }

        translate([-plate_r + exit_chute_left, 0, 0])
        difference() {
            union() {
                chute_shape(extra_overhang=0, y0=0, x_inset=0);

                // Right chute_support
                translate([exit_chute_width, 0, 0])
                chute_support();

                // Top chute_support
                chute_support_top();
            };

            translate([0, 0, -floor_h-epsilon])
            chute_shape(x_inset=wall_w);

            translate([0, 0, wall_w+epsilon])
            chute_shape(wall_w*1, - wall_w * 2, wall_w);
        };
    }

    module curve() {
        tangent_angle = 90-entrance_slope_angle;
        function radius_point_flipped(l) = radius_point(l, tangent_angle, [-1, 1]);

        tangent_intercept_point_outer = radius_point_flipped(stirrer_r);
        tangent_intercept_point_inner = radius_point_flipped(stirrer_r-wall_w);
        tangent_offset = - tangent_intercept_point_outer.x + tangent_intercept_point_outer.y / tan(entrance_slope_angle);

        if (debug_viz) {
            translate(tangent_intercept_point_outer)
                color("cyan")
                cylinder(h = 11, d = 1);

            for (i = [0: stirrer_r])
                translate(radius_point_flipped(i))
                color("blue", 0.5)
                cylinder(h = 10, d = 1);

            for (i = [0: plate_r * 1.5])
                translate(entrance_slope_point(i, stirrer_r, entrance_slope_angle))
                color("pink", 0.5)
                cylinder(h = 10, d = 1);

            color("orange", 0.3)
            cylinder(h = 9, r = stirrer_r);
        }


        curve_x0 = -plate_r + exit_chute_right - wall_w;

        tangent_mx = mx(entrance_slope_point(0, stirrer_r, entrance_slope_angle), entrance_slope_point(10, stirrer_r, entrance_slope_angle));
        tangent_m = tangent_mx[0];
        tangent_c = tangent_mx[1];


        // Work out the height the curve needs to cover between
        // the exit chute and the tangent
        exit_chute_right_tangent_intercept_y =
            tangent_m * curve_x0 + tangent_c;

        // Add half y distance to x to make space for curve
        curve_x = curve_x0 + exit_chute_right_tangent_intercept_y * guide_curve_factor;
        curve_y_inner = tangent_mx_f(curve_x, stirrer_r - wall_w, entrance_slope_angle);
        curve_y_outer = tangent_mx_f(curve_x, stirrer_r, entrance_slope_angle);

        if (debug_viz) {
            translate([curve_x, curve_y_outer])
                color("black")
                cylinder(h = 11, d = 1);
            translate([curve_x, curve_y_inner])
                color("white")
                cylinder(h = 11, d = 1);
        }

        points = [
            // Curve from inside of chute to top of slope
            each tween_points([curve_x0, 0], [curve_x, curve_y_outer], guide_curve_samples, guide_curve_cutoff),
            // Slope to intercept point
            tangent_intercept_point_outer,
            // Somewhat inside the stirrer (differenced away later)
            tangent_intercept_point_inner,
            // Curve from bottom of slope to outside of chute
            each reverse(tween_points([curve_x0+wall_w, 0], [curve_x, curve_y_inner], guide_curve_samples, guide_curve_cutoff)),
        ];

        difference() {
            union() {
                // The slope and curve
                linear_extrude(exit_chute_height)
                polygon(points);

                // Fence around the stirrer
                angle = atan(tangent_intercept_point_inner.y
                    / tangent_intercept_point_inner.x)
                    // TODO: The maths to work out this fudge
                    + 24.5;
                rotate([0, 0, 180])
                rotate_extrude(
                    angle = angle,
                    convexity = 2
                )
                translate([stirrer_r + clearance/2, 0, 0])
                square([wall_w, exit_chute_height]);
            };

            // The stirrer + clearance
            translate([0, 0, -epsilon])
            cylinder(
                d = stirrer_d + clearance,
                h = exit_chute_height * 2
            );
        }
    }

    rotate([0, 180, 0])
    difference() {
        union() {
            exit_chute();
            curve();
        }

        // This is a lazy hack
        cylinder(
            d = plate_d * 2,
            h = clearance * 1
        );
    }
}

module part_guide_top() {
    exit_chute_overhang = 1;

    // If this is adjusted the angle of the guide close to the spinner
    // may also need to be tweaked because it isn't calculated properly
    wall_w = min_wall_w * 2;
    floor_h = min_floor_h * 1.5;
    plate_r = plate_d/2;
    cup_r = plate_cup_d/2;
    stirrer_r = stirrer_d/2;

    cup_wall_h = plate_cup_h
                - plate_cup_floor_h
                - plate_clearance
                - plate_h
                - plate_clearance;
    heatset_boss_d = min_wall_w * 2 + heatset_d;

    boss_h = exit_chute_height - cup_wall_h;

    // Work out the boss positions first because they are critical
    bosses = [
        radius_point(cup_r + heatset_boss_d/2 - min_wall_w/2, 90-entrance_slope_angle, flip=[-1, 1]),
        radius_point(cup_r + heatset_boss_d/2 - min_wall_w/2 + 10, 90-entrance_slope_angle, flip=[-1, 1]),
    ];

    function offset_angle(p, angle, d) = [
        p.x + cos(angle) * d,
        p.y + sin(angle) * d,
    ];

    boss_slot_cut_length_fraction = 0.5;

    module boss_slot(tail_length=0, blunt=false) {
        // extra is so both boss screws can be installed with part in the 'min' position
        extra_length = 10;
        boss_r0 = cup_r + heatset_boss_d/2 + min_wall_w + extra_length;
        boss_slot_angle = 90-entrance_slope_angle;
        boss_slot_angle_perp = entrance_slope_angle;

        boss_slot_length = -plate_r+stirrer_r+clearance+wall_w-extra_length;

        module slot(h, d, l=1, blunt=false, tail_length=0) {
            linear_extrude(h)
            union() {
                l_ = (boss_slot_length + (blunt ? -d / 2 : 0)) * l;
                polygon([
                    offset_angle(
                        radius_point(boss_r0, boss_slot_angle, flip=[-1, 1]),
                        boss_slot_angle_perp,
                        - d/2
                    ),
                    offset_angle(
                        radius_point(boss_r0 + l_ - tail_length, boss_slot_angle, flip=[-1, 1]),
                        boss_slot_angle_perp,
                        - d/2
                    ),
                    offset_angle(
                        radius_point(boss_r0 + l_ - tail_length, boss_slot_angle, flip=[-1, 1]),
                        boss_slot_angle_perp,
                        d/2
                    ),
                    offset_angle(
                        radius_point(boss_r0, boss_slot_angle, flip=[-1, 1]),
                        boss_slot_angle_perp,
                        d/2
                    ),
                ]);

                translate(radius_point(boss_r0, boss_slot_angle, flip=[-1, 1]))
                circle(d = d);

                if (!blunt) {
                    translate(radius_point(boss_r0 + l_ - tail_length, boss_slot_angle, flip=[-1, 1]))
                    circle(d = d);
                }
            }
        }

        difference() {
            slot(boss_h, heatset_boss_d, blunt=blunt, tail_length=tail_length);

            translate([0, 0, -epsilon])
            slot(boss_h+2*epsilon, screw_hole_d, boss_slot_cut_length_fraction);

            translate([0, 0, min_floor_h * 2])
            slot(boss_h+2*epsilon, screw_head_d, boss_slot_cut_length_fraction);
        }
    }

    module curve() {
        tangent_angle = 90-entrance_slope_angle;
        function radius_point_flipped(l) = radius_point(l, tangent_angle, [-1, 1]);

        tangent_intercept_point_outer = radius_point_flipped(stirrer_r);
        tangent_intercept_point_inner = radius_point_flipped(stirrer_r-wall_w);
        tangent_offset = - tangent_intercept_point_outer.x + tangent_intercept_point_outer.y / tan(entrance_slope_angle);

        if (debug_viz) {
            translate(tangent_intercept_point_outer)
                color("cyan")
                cylinder(h = 11, d = 1);

            for (i = [0: stirrer_r])
                translate(radius_point_flipped(i))
                color("blue", 0.5)
                cylinder(h = 10, d = 1);

            for (i = [0: plate_r * 1.5])
                translate(entrance_slope_point(i, stirrer_r, entrance_slope_angle))
                color("pink", 0.5)
                cylinder(h = 10, d = 1);

            color("orange", 0.3)
            cylinder(h = 9, r = stirrer_r);
        }


        curve_x0 = -plate_r + exit_chute_right - wall_w;

        tangent_mx = mx(entrance_slope_point(0, stirrer_r, entrance_slope_angle), entrance_slope_point(10, stirrer_r, entrance_slope_angle));
        tangent_m = tangent_mx[0];
        tangent_c = tangent_mx[1];


        // Work out the height the curve needs to cover between
        // the exit chute and the tangent
        exit_chute_right_tangent_intercept_y =
            tangent_m * curve_x0 + tangent_c;

        // Add half y distance to x to make space for curve
        curve_x = curve_x0 + exit_chute_right_tangent_intercept_y * guide_curve_factor;
        curve_y_inner = tangent_mx_f(curve_x, stirrer_r - wall_w, entrance_slope_angle);
        curve_y_outer = tangent_mx_f(curve_x, stirrer_r, entrance_slope_angle);

        scoop_x = 0;
        scoop_outer = [scoop_x, tangent_mx_f(scoop_x, stirrer_r, entrance_slope_angle)];
        scoop_inner = offset_angle(scoop_outer, 90 + entrance_slope_angle, -wall_w);

        boss_join_point = offset_angle(
            [plate_r-exit_chute_right+heatset_boss_d*3, scoop_outer.y - curve_x0 / 4 - heatset_boss_d*2],
            entrance_slope_angle - 90,
            scoop_outer.y - curve_x0 / 4 + heatset_boss_d );
        points = [
            // Curve from inside of chute to top of slope
            each tween_points([curve_x0, 0], [curve_x, curve_y_outer], guide_curve_samples, guide_curve_cutoff),
            // Slope to entrance
            scoop_outer,

            each tween_points(
                scoop_outer,
                [plate_r-exit_chute_right, scoop_outer.y - curve_x0 / 4],
                guide_curve_samples, 1, tween=1),

            each reverse(tween_points(
                [plate_r-exit_chute_right+heatset_boss_d*3, scoop_outer.y - curve_x0 / 4 - heatset_boss_d*2],
                [plate_r-exit_chute_right, scoop_outer.y - curve_x0 / 4],
                guide_curve_samples, 1, tween=1)),


            boss_join_point,
            offset_angle(boss_join_point, entrance_slope_angle, - wall_w),

            each tween_points(
                [plate_r-exit_chute_right+heatset_boss_d*3-wall_w, scoop_outer.y - curve_x0 / 4 - heatset_boss_d*2],
                [plate_r-exit_chute_right, scoop_inner.y - curve_x0 / 4],
                guide_curve_samples, 1, tween=1),

            each reverse(tween_points(
                scoop_inner,
                [plate_r-exit_chute_right, scoop_inner.y - curve_x0 / 4],
                guide_curve_samples, 1, tween=1)),

            // Curve from bottom of slope to outside of chute
            each reverse(tween_points([curve_x0, -wall_w], [curve_x, curve_y_inner], guide_curve_samples, guide_curve_cutoff)),
        ];

        // The slope and curve
        linear_extrude(exit_chute_height)
        polygon([
            for (p = points)
                offset_angle(
                    offset_angle(p, 90 - entrance_slope_angle, 10),
                    entrance_slope_angle, -10),
        ]);

    }

    rom = (plate_r-stirrer_r-clearance-wall_w) * boss_slot_cut_length_fraction -10;
    translate(radius_point(rom * $t, 90 - entrance_slope_angle, flip=[1, 1]))
    rotate([0, 180, 0])
    difference() {
        union() {
            translate([0, 0, cup_wall_h])
            boss_slot(blunt=true);

            curve();

            // TODO: this should be calculated properly/better
            extra_tail = -36;
            translate([
                each offset_angle(
                    offset_angle(
                        [0, 0],
                        entrance_slope_angle,
                        plate_r-exit_chute_left-heatset_boss_d+wall_w*2.2
                    ),
                    entrance_slope_angle+90,
                    34
                ), cup_wall_h]
            )
            rotate([0, 0, 180])
            boss_slot(extra_tail);
        }

        // This is a lazy hack
        cylinder(
            d = plate_d * 2,
            h = clearance * 1
        );
    }
}

// Debugging visualizations, not for printing
    color("darkgrey", 0.4)
    translate([-3, 0, -33])
    import("NEMA_17.stl");
if (debug_viz && !print) {
    // Motor

    // Top bearing
    translate([0, 0, top_bearing_z])
    #bearing();

    // Bottom bearing
    translate([0, 0, bottom_bearing_h])
    #bearing();

    // Side bearing
    rotate([0, 0, 90])
    translate([motor_face_plate_w/2 + min_floor_h, 0, side_gear_z])
    rotate([0, 90, 0])
    #bearing();
}


module bottom_gear_and_shaft() {
    translate([0, 0, bearing_h]) {
        translate([0, 0, min_floor_h])
        bottom_gear();

        bottom_gear_shaft();
    }
}

module motor_cup() {
    ow = motor_face_plate_w + min_wall_w * 4 + clearance * 2;
    iw = motor_face_plate_w + 2 * clearance;
    f = min_floor_h * 3;

    difference() {
        translate([-ow/2, -ow/2, 0])
        cube([ow, ow, motor_face_plate_w / 4]);

        translate([0, 0, f]) {
            translate([-iw/2, -iw/2, 0])
            cube([iw, iw, ow]);

            translate([iw/2, -iw/4, 0])
            cube([iw/2, iw/2, ow]);
        }
    }
}

// difference is for cross_section
difference () {
    union() {

        layout_part("frame_bottom", r = [0, 0, -90])
        frame_bottom();

        layout_part("frame_side", [motor_face_plate_w/2, 0, 0], [0, 90, 0])
        frame_side();

        layout_part("bottom_gear", [0, 0, bottom_bearing_h])
        bottom_gear_and_shaft();

        // rotation is so gears mesh in viz
        layout_part("top_gear", [0, 0, top_bearing_z], [0, 0, 0])
        top_gear();

        layout_part("side_gear")
        side_pinion();

        color("orange", 0.4)
        layout_part("stirrer", [0, 0, stirrer_z], [0, 180, 0])
        stirrer();

        color("beige", 0.4)
        layout_part("part_guide_left", [0, 0, stirrer_z], [0, 180, 0])
        part_guide_left();

        color("red", 0.4)
        layout_part("part_guide_top", [0, 0, stirrer_z], [0, 180, 0])
        part_guide_top();

        color("green", 0.4)
        layout_part("plate", [0, 0, plate_z], [0, 180, 0])
        plate();

        color("yellow", 0.4)
        layout_part("plate_cup", [0, 0, frame_side_h], [0, 0, 180])
        plate_cup();

        layout_part("motor_cup", [0, 0, -(motor_face_plate_w*3/4) - min_floor_h*5], [0, 0, 180])
        motor_cup();
    }

    if (cross_section) {
        translate([-100, 0, -100])
        cube([200, 200, 200]);
    }
}
