set terminal wxt size 1000,600

set style fill solid 0.2

#set yrange [-5:5]

set size 1,1
set multiplot

set origin 0,0
set size 1,1

plot "DATA" using 1:5:4 with filledcurve lc 4 title "abs", \
     "DATA" using 1:2 with lines lc 1 title "real", \
     "DATA" using 1:3 with lines lc 3 title "imag"

set origin 0.6, 0.1
set size 0.4, 0.4
#set xrange [-100:100]
#set yrange [-100:100]
set xrange [-5:5]
set yrange [-1:5]
set grid x2tics
set grid y2tics
set x2tics (0) format "" scale 0
set y2tics (0) format "" scale 0

#set arrow from -80,0 to 80,0
plot "<echo '0 1'" with points notitle, \
     "CONTOUR" using 1:2 with lines lc rgb 'blue' notitle

set origin 0.6, 0.6
set size 0.3, 0.3
set xrange [-110:110]
set yrange [-10:110]
set grid x2tics
set grid y2tics
set x2tics (0) format "" scale 0
set y2tics (0) format "" scale 0

plot "<echo '0 1'" with points notitle, \
     "CONTOUR" using 1:2 with lines lc rgb 'blue' notitle

unset multiplot

pause -1
