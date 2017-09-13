make clean
make
vpr iir1.map4.latren.net k4-n10.xml place.out route.out -nodisp -place_only -seed 0
gprof vpr gmon.out | less
