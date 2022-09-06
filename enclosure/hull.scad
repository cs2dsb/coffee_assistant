module multiHull(){
    for (i = [1 : $children-1])
        hull(){
            children(0);
            children(i);
        }
}

module sequentialHull(){
    for (i = [0: $children-2])
        hull(){
            children(i);
            children(i+1);
        }
}

/* Extended fun hull functions */

module cylinders(points, diameter, thickness){
    for (p=points){
        translate(p) cylinder(d=diameter, h=thickness, center=true);
    }
}

module plate(points, diameter, thickness, hole_diameter){
    difference(){
        hull() cylinders(points, diameter, thickness);
        cylinders(points, hole_diameter, 1000);
    }
}

module bar(length, width, thickness, hole_diameter){
    plate([[0,0,0], [length,0,0]], width, thickness, hole_diameter);
}

module rounded_box(points, radius, height){
    hull(){
        for (p = points){
            translate(p)
                cylinder(r=radius, h=height);
        }
    }
}

module rounded_box2(points, radius, height){
    hull(){
        for (p = points){
            translate(p)
                sphere(r=radius);
            if (height > 0) {
                translate(p + [0, 0, height])
                    sphere(r=radius);
            }
        }
    }
}

module point_cloud(points, radius=1, facets=8){
    for (p=points){
        translate(p)
            sphere(radius/cos(180/facets), $fn=facets);
        // polygon sphere circumscribed on radius
    }
}

module point_hull(points, radius=1, facets=8){
    hull(){
        point_cloud(points, radius, facets);
    }
}

// Shrink the exterior of the points so that the hull fits inside
function select(vector, i) = [ for (p=vector) p[i] ];
function axis_center(v, i) = (max(select(v, i)) + min(select(v,i)))/2;
function center(v) = [ for (i = [0,1,2]) axis_center(v, i) ];
function shrink_point(point, center, radius) = [ for (i = [0,1,2]) point[i] > center[i] ? point[i] - radius : point[i] + radius ];
function shrink(points, radius) = [ for (p = points) shrink_point(p, center(points), radius) ];

module interior_point_hull(points, radius=1){
    point_hull(shrink(points, radius), radius);
}

module interior_rounded_box(points, radius, height){
    rounded_box(shrink(points, radius), radius, height);
}

module interior_rounded_box2(points, radius, height){
    rounded_box2(shrink(points, radius), radius, height);
}


module mirror_copy(v = [1, 0, 0]) {
    children();
    mirror(v) children();
}
