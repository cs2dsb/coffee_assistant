use <revolve2.scad>;

// These also work but revolve seems to be faster
// use <threads.scad>;
// use <circlestack_thread3.scad>;

od_pitch_map_course = [
     [1,      0.25]
    ,[1.1,    0.25]
    ,[1.2,    0.25]
    ,[1.4,    0.30]
    ,[1.6,    0.35]
    ,[1.8,    0.35]
    ,[2,      0.40]
    ,[2.2,    0.45]
    ,[2.5,    0.45]
    ,[3,      0.50]
    ,[3.5,    0.60]
    ,[4,      0.70]
    ,[4.5,    0.75]
    ,[5,      0.80]
    ,[6,      1.00]
    ,[7,      1.00]
    ,[8,      1.25]
    ,[9,      1.25]
    ,[10,     1.50]
    ,[11,     1.50]
    ,[12,     1.75]
    ,[14,     2.00]
    ,[16,     2.00]
    ,[18,     2.50]
    ,[20,     2.50]
    ,[22,     2.50]
    ,[24,     3.00]
    ,[27,     3.00]
    ,[30,     3.50]
    ,[33,     3.50]
    ,[36,     4.00]
    ,[39,     4.00]
    ,[42,     4.50]
    ,[45,     4.50]
    ,[48,     5.00]
];

// Finds the correct thread pitch for a given diameter, errors is diameter isn't valid metric course thread
function od_to_pitch(od=3) =
    len(search(od, od_pitch_map_course)) == 0 ?
        assert(false, str("Failed to find a valid metric thread pitch for od=", od)) :
        od_pitch_map_course[search(od, od_pitch_map_course)[0]][1];

// Works out the depth of the thread given the pitch and angle
function thread_depth(pitch, angle=30) =
        5/8*pitch/(2*tan(angle));

// Creates an ISO thread profile for a given diameter and pitch
// relief scales the thread to create a thread gap
function ISO_profile(diameter, pitch, relief=0) =
    let (d = thread_depth(pitch), relief = thread_depth(pitch) * relief, r = diameter/2) [
        [0*pitch, r+relief],
        [1/16*pitch, r+relief],
        [(0.5-1/8)*(pitch+relief), r-d-relief],
        [(0.5+1/8)*(pitch-relief), r-d-relief],
        [(1-1/16)*pitch, r+relief],
        [1*pitch, r+relief]
    ];

module m_thread(
    od=3,
    height=10,
    internal=true,
    leadin=1,
    skip_threads=false,
    taper=0.0,
    starts=1,
    internal_relief=0.05
) {
    pitch = od_to_pitch(od);
    relief = internal ? internal_relief : 0;
    square_profile = ISO_profile(od, pitch, 0);//relief);

    render()
        scale([1+relief, 1+relief, 1])
        union() {
            revolve(square_profile, length=height-taper, nthreads=starts);
            if (taper > 0) {
                translate([0, 0, height-taper])
                    revolve(square_profile, length=taper, nthreads=starts, scale=0.2);
            }
        };


    //metric_thread(od, od_to_pitch(od), height, internal, leadin=leadin, test=skip_threads, leadfac=taper);
}
