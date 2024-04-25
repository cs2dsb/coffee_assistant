use <gears.scad>;
use <polyround.scad>;
use <metric_threads.scad>;
use <revolve2.scad>;

$fn = $preview ? 50 : 150;

//TODO: scoop thing at the part guide exit making sure parts drop down vertically

print = !$preview;
part = [ "_dummy"
    // , "cup_test"
    // , "screw_conveyor"
    , "screw_conveyor_tube"
    // , "frame_bottom"
    // , "hopper"
    // , "motor_plate"

    // , "frame_side"
    // , "bottom_gear"
    // , "top_gear"
    // , "side_gear"
    // , "stirrer"
    // , "bottom_gear_shaft"
    // , "plate"
    // , "plate_cup"
    // , "part_guide_left"
    // , "part_guide_top"
    // , "motor_cup"

];

debug_viz = $preview && true;
cross_section = false;
hide_gears = $preview && false;

epsilon     = 0.01;

crush_rib_n = 6;
crush_rib_w = 0;//1.2;
crush_rib_id_factor = 0.6;
crush_rib_od_factor = 0.75;

min_floor_h = 1.0;
min_wall_w  = 1.5;
clearance   = 0.8;

bearing_od  = 21.95;
// Tweak to adjust fit. Should be tightish press fit
bearing_id_clearance = 0.2; //0.1;
bearing_id  = 8 + bearing_id_clearance;
bearing_id_ring_d = 12;
bearing_h   = 7;

plate_d = 200;
plate_h = min_floor_h * 2;
plate_clearance = clearance;

stirrer_d = plate_d / 3;
stirrer_h = plate_h * 5;
stirrer_dent_n = 5;
stirrer_dent_depth = stirrer_d * 0.022;

// stepper_shaft_d = 5.2;
// stepper_shaft_cutout = 0.9;
stepper_shaft_l = 10;

stepper_shaft_d = 5.1;
stepper_shaft_cutout = 0.4;

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

part_dims = [15, 10, 6];
part_avg_dim = floor((max(part_dims.x, max(part_dims.y, part_dims.z)) +
               min(part_dims.x, min(part_dims.y, part_dims.z))) / 2);
part_clearance_scale = 1.5;
screw_conveyor_od = max(bearing_od+2*clearance, part_avg_dim * part_clearance_scale * 2);
screw_conveyor_n = 3;
screw_conveyor_l = screw_conveyor_od * screw_conveyor_n;
// How many threads there are per rotation
thread_period = 1.5;



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

module heatset_boss(support=true) {
    d = heatset_d + min_wall_w * 2;
    h = heatset_h * 10;
    difference() {
        union() {
            cylinder(
                d = d,
                h = h
            );

            if (support) {
                difference() {
                    translate([-d/sqrt(2), 0, 0])
                    rotate([0, 0, 45])
                    translate([-d, -d, 0])
                    cube([d*2, d*2, h], center = false);

                    for (i=[d/sqrt(2)/2, -d*4.5])
                        translate([i, -d*2, -epsilon])
                        cube([d*4, d*4, h+epsilon*2], center = false);
                };
            }
        };

        translate([0, 0, -epsilon])
        heatset();
    }
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
    // Base plate
    x = screw_conveyor_od + 2 * heatset_d;
    y = screw_conveyor_l;
    z = heatset_h * 15 + screw_conveyor_od/2;
    translate([-x/2, -screw_conveyor_od*5/3, -z])
    cube([x, y, 5]);

    translate([-x, -screw_conveyor_od*5/3, -z])
    cube([x*2, y/2, 5]);


    // Interface plate
    x1 = screw_conveyor_od + 2.5 * heatset_d;
    y1 = screw_conveyor_od*4/3 + heatset_d;

    angle = 0;

    translate([0, -5, 0])
    rotate([angle, 0, 0]) {
        rotate([-90, 0, 0])
        layout_joints()
            scale([0.95, 0.95, 1])
            heatset();

        // translate([-x1/2, min_wall_w * 1 + bearing_h, 5+heatset_h/2])
        // cube([x1, y1, 5]);
    }

    y2 = cos(angle) * y1;
    difference() {
        translate([-x1/2, -y1/2 + screw_conveyor_od*2/3 + heatset_d - sin(25), -z])
        cube([x1, y2, z+12]);


        color("red")
        rotate([angle, 0, 0])
        translate([-x1, min_wall_w * 1 + bearing_h -y1, 11+heatset_h/4])
        cube([x1*2, y1*3, 600]);
    }

}

module hopper() {
    angle = 0;

    rotate([angle, 0, 0]) {

        rotate([-90, 0, 0])
        layout_joints()
            scale([0.94, 0.94, 1])
            heatset();

        x = screw_conveyor_od + 2 * heatset_d+2*clearance;
        y = screw_conveyor_od*4/3 +  heatset_d/2;

        dx = x - (screw_conveyor_od + clearance * 2 + 2 * min_wall_w)*2/3;
        dy = heatset_d * 2;

        ix = x - dx;
        iy = y - dy;

        h = screw_conveyor_od * 1.5;
        l = screw_conveyor_od * 2;
        mirror([0, 0, 1]) {

            translate([
                0,
                min_wall_w * 1 + bearing_h + y/2,
                -screw_conveyor_od/2 + heatset_h/2
            ]) {
                difference() {
                    union() {
                        translate([-x/2, -y/2])
                        cube([x, y, min_floor_h]);

                        translate([0, 0, min_floor_h])
                        linear_extrude(20, scale=10/x)
                        translate([-x/2, -y/2])
                        square([x, y]);

                        scale([1.05, 1.05])
                        linear_extrude(h-2*epsilon, scale=4*h/x*0.9)
                        bean_hole();
                    }

                    translate([0, 0, -epsilon])
                    linear_extrude(h, scale=4*h/x*.9)
                    bean_hole();
                }

                // difference() {
                //     translate([-x/2, -y/2])
                //     cube([x, y, screw_conveyor_od]);

                //     rotate([angle, 0, 0])
                //     translate([-x, -y, y*sin(angle)])
                //     cube([x*2, y*2, screw_conveyor_od]);

                //     translate([-ix/2, -iy/2, -epsilon])
                //     cube([ix, iy, screw_conveyor_od*5]);
                // }

                // // 1.4?? fuck knows
                // translate([0, 0, y*sin(angle)+1.4])
                // rotate([angle, 0, 0])
                // difference() {
                //     linear_extrude(l, scale = 2 * sqrt(2))
                //     translate([-x/2, -y / cos(angle)/2])
                //     square([x, y / cos(angle)]);

                //     translate([0, 0, -epsilon])
                //     linear_extrude(l+2*epsilon, scale = [
                //         (x - min_wall_w) / ix * 2 * sqrt(2),
                //         (y - min_wall_w) / iy * 2 * sqrt(2),
                //     ])
                //     translate([-ix/2, -iy / cos(angle)/2])
                //     square([ix, iy / cos(angle)]);
                // }
            }
        }
    }

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

module screw_conveyor() {
    pitch_scale = 5;

    // pitch = od_to_pitch(screw_conveyor_od) * pitch_scale;
    // td = thread_depth(pitch);

    thread_starts = 2;
    thread_stretch = 15;

    function prof_sin(d, z) = [z, d/2-d/4.1+d/4*sin(z*360/thread_period)];
    // ...which becomes a profile vector with the help of linspace
    thread_profile = [for (z=linspace(start=0, stop=thread_period, n=15)) prof_sin(screw_conveyor_od, z)];


    // module stepper_shaft() {
    //     difference() {
    //         cylinder(
    //             d = stepper_shaft_d,
    //             h = stepper_shaft_l
    //         );

    //         for (r = [0, 180])
    //         rotate([0, 0, r])
    //         translate([-stepper_shaft_d/2, stepper_shaft_d/2-stepper_shaft_cutout, -epsilon])
    //         cube([stepper_shaft_d, stepper_shaft_d, stepper_shaft_l+2*epsilon]);
    //     };
    // }

    module stepper_shaft() {
        translate([0, 0, -1])
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

    difference() {
        union() {
            translate([0, 0, min_wall_w + bearing_h]) {

                screw_type = "single";
                // screw_type = "double";
                // screw_type = "sine";
                if (screw_type == "sine") {
                    scale([1, 1, thread_stretch])
                    revolve(
                        thread_profile,
                        length=screw_conveyor_l/thread_stretch,
                        nthreads=thread_starts
                    );
                } else if (screw_type == "double") {
                    linear_extrude(
                        height = screw_conveyor_od*screw_conveyor_n,
                        center = false,
                        convexity = 10,
                        twist = -220*screw_conveyor_n
                    )
                    square([min_wall_w*2, screw_conveyor_od], center = true);
                } else if (screw_type == "single") {
                    hole=0.6;
                    gap=0.1;
                    for (i = [0, 180]) {
                        rotate([0, 0, i])
                        linear_extrude(
                            height = screw_conveyor_od*screw_conveyor_n,
                            center = false,
                            convexity = 10,
                            twist = -220*1.5*screw_conveyor_n
                        )
                        translate([0, -screw_conveyor_od/4*(2-hole)*(1-gap), 0])
                        square([min_wall_w*3, screw_conveyor_od/2*hole], center = true);
                    }
                    // // This kind needs a central shaft
                    // linear_extrude(screw_conveyor_od*screw_conveyor_n)
                    // circle(d = d/4);
                }

                //clearance is to stop flat of screw rubbing on outer bearing race
                translate([0, 0, -clearance*2])
                rotate([180, 0, 0])
                bearing_id_cup(true, false);

                // Shaft support
                d = screw_conveyor_od + clearance;
                //d = bearing_id + min_floor_h * 4;
                //translate([0, 0, min_floor_h*0.4])
                // color("red")
                // linear_extrude(max(d/2, 15), scale = 1/d)
                // circle(d = d);

                linear_extrude(min_floor_h*2)
                circle(d = d);

                translate([0, 0, min_floor_h])
                linear_extrude(min_floor_h, scale = 0.33)
                circle(d = d);

                translate([0, 0, min_floor_h*2])
                linear_extrude(min_floor_h*10)
                circle(d = d/4);


                // // Print support mainly, also makes a cap so stuff doesn't shoot off
                // translate([0, 0, screw_conveyor_od*(screw_conveyor_n+0.2)])
                // cylinder(
                //     d = screw_conveyor_od,
                //     h = min_floor_h);

            }

            translate([0, 0, 0])
            rotate([180, 0, 0])
            cylinder(
                d = bearing_id * 0.95,
                h = min_floor_h
            );

        }

        color("blue")
        translate([0, 0, -min_floor_h-epsilon])
        rotate([0, 0, 90])
        stepper_shaft();
    }

}

module layout_joints() {
    id = screw_conveyor_od + clearance * 2;
    od = id + 2 * min_wall_w;

    boss_x = od*1/3+heatset_d+min_wall_w/2;
    boss_z = min_wall_w * 1 + heatset_d/2 +bearing_h;


    for (i=[
          [boss_x, boss_z]
        , [-boss_x, boss_z]
        , [boss_x, boss_z + screw_conveyor_od*4/3 -  heatset_d/2]
        , [-boss_x, boss_z + screw_conveyor_od*4/3 -  heatset_d/2]
    ]) {
        translate([i[0], -od*1/2, i[1]])
        rotate([-90, 0, 0])
        difference() {
            rotate([0, 0, i[0] < 0? 180 : 0])
            children();
        };
    }

}

module motor_plate(thickness=min_floor_h * 2) {
    wall_w = min_wall_w;

    radius = 5;
    l = motor_face_plate_w;
    hl = l / 2;
    ml = 31/2;

    difference() {
        union() {
            points = [
                // x, y, radius
                 [ -hl, -hl, radius]
                ,[ -hl, hl, radius]
                ,[ hl, hl, radius]
                ,[ hl, -hl, radius]
            ];

            // Plate
            difference() {
                linear_extrude(thickness)
                    polygon(polyRound(points, $fn));

                translate([0, 0, -epsilon])
                cylinder(
                    h = thickness + 2 * epsilon,
                    d = taper_bottom_d - wall_w*4
                );
            };

        };

        for (i = [1:4]) {
            r = (i + 0.5) * (360/4);

            rotate([0, 0, r])
            translate([31*sqrt(2)/2, 0, 0])
            screw_hole();
        }
    };
}

module bean_hole() {
    y = screw_conveyor_od*0.9;
    x = screw_conveyor_od*0.9;
    radius = x/5;
    translate([-x/2, -y/2, 0])
    polygon(polyRound([
        // x, y, radius
         [ 0, 0, radius]
        ,[ x, 0, radius]
        ,[ x, y, radius]
        ,[ 0, y, radius]
    ], $fn));
}

module screw_conveyor_tube() {
    id = screw_conveyor_od + clearance * 3;
    od = id + 2 * min_wall_w;

    boss_x = od*1/3+heatset_d+min_wall_w/2;
    boss_z = min_wall_w * 1 + heatset_d/2 +bearing_h;

    extra_height = 0.0;

    union() {
        difference() {
            union() {
                cylinder(
                    d = od,
                    h = screw_conveyor_l+bearing_h+min_floor_h);


                // Square bit
                translate([-od*1/3-min_wall_w*2, -od*(0.5+extra_height), 0])
                linear_extrude(
                    screw_conveyor_l * 2/3,
                    scale = 1)
                square([od*2/3+min_wall_w*4, od*(1+extra_height)]);

                // Taper
                translate([0, 0, screw_conveyor_l * 2/3])
                linear_extrude(
                    screw_conveyor_l * 1/8,
                    scale = 0.5)
                translate([-od*1/3-min_wall_w*2, -od*(0.5+extra_height), 0])
                square([od*2/3+min_wall_w*4, od*(1+extra_height)]);

        // color("blue")
        //         translate([-od*1/3-min_wall_w*2, -od*1, 0])
        //         cube([od*2/3+min_wall_w*4, od*1.5, screw_conveyor_l+bearing_h+min_floor_h]);

                for (m=[0, 1]) mirror([0, m, 0])
                layout_joints()
                    difference() {
                        heatset_boss();

                        translate([-50, -50, (od-clearance/2)*(m == 1 ? (1+extra_height) : 1)])
                        cube([100, 100, 100]);
                    };

                // Motor support
                difference() {
                    translate([0, 0, -8])
                    rotate([0, 0, 45])
                        motor_plate(thickness=8);
                }
            }

            translate([0, 0, min_wall_w+bearing_h])
            cylinder(
                d = id,
                h = screw_conveyor_l+2*epsilon);

            translate([0, 0, -epsilon])
            cylinder(
                d = bearing_od,
                h = screw_conveyor_l+2*epsilon+min_floor_h);

            // y = screw_conveyor_od*4/3;

            // translate([-od*1/3, -od*0.75, min_wall_w * 2+bearing_h])
            // cube([od*2/3, od/2, y]);

            for (m=[0, 1]) mirror([0, m, 0])
            translate([0, (m == 1 ? -clearance : -(od*extra_height+clearance)), 0])
            layout_joints()
                heatset();


            skew = 12;
            // Cutout to let the beans in
            rotate([90, 0, 0])
            translate([0, screw_conveyor_od*2/3+min_wall_w*2 + bearing_h, od*(1+extra_height)/2])
            translate([0, 0, 5])
            mirror([0, 0, 1])

            translate([0, -skew, 0])
            linear_extrude(od*0.6, scale=[1, 2])
            translate([0, skew, 0])
            bean_hole();
        }

        translate([0, 0, min_floor_h])
        bearing_od_cup();

        // spout
        translate([0, 0, screw_conveyor_l+bearing_h+min_floor_h])
        translate([0, -od, 0])
        linear_extrude(od/2, scale = [1, 1.2])
        translate([0, od, 0])
        difference() {
            circle(d = id + min_wall_w * 2);

            circle(d = id);

            translate([-id/2, -id])
            square([id, id]);
        }
    }
}


// Debugging visualizations, not for printing
if (debug_viz && !print) {
    // Motor
    // color("darkgrey", 0.4)
    // rotate([0, 00, 0])
    // translate([-21, -23, -32])
    // rotate([0, 0, 0])
    // import("Stepper_motor_28BYJ-48.stl");

    color("darkgrey", 0.4)
    translate([-2, -42, -2])
    rotate([-90, -45, 0])
    rotate([0, 0, 0])
    import("NEMA_17.stl");

    // Top bearing
    translate([0, min_floor_h+bearing_h, 0])
    rotate([90, 0, 0])
    #bearing();

    // // Bottom bearing
    // translate([0, 0, bottom_bearing_h])
    // #bearing();

    // // Side bearing
    // rotate([0, 0, 90])
    // translate([motor_face_plate_w/2 + min_floor_h, 0, side_gear_z])
    // rotate([0, 90, 0])
    // #bearing();
}

// difference is for cross_section
difference () {
    union() {
        layout_part("cup_test")
        rotate([180, 0, 0])
        bearing_id_cup(true, false);

        layout_part("screw_conveyor", r = [-90, 0, 0])
        screw_conveyor();

        layout_part("screw_conveyor_tube", r = [-90, 0, 0])
        screw_conveyor_tube();

        layout_part("frame_bottom", r = [-25, 0, 0], t = [0, 0, -screw_conveyor_od])
        frame_bottom();

        layout_part("hopper", r = [180, 0, 180], t = [0, 0, screw_conveyor_od])
        hopper();


        // layout_part("frame_side", [motor_face_plate_w/2, 0, 0], [0, 90, 0])
        // frame_side();

        // layout_part("bottom_gear", [0, 0, bottom_bearing_h])
        // bottom_gear_and_shaft();

        // // rotation is so gears mesh in viz
        // layout_part("top_gear", [0, 0, top_bearing_z], [0, 0, 0])
        // top_gear();

        // layout_part("side_gear")
        // side_pinion();


        layout_part("motor_cup", [0, 0, -(motor_face_plate_w*3/4) - min_floor_h*5], [0, 0, 180])
        motor_cup();
    }

    if (cross_section) {
        translate([-100, 0, -100])
        cube([200, 200, 200]);
    }
}
