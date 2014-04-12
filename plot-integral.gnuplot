set style fill solid 0.2

#set yrange [-5:5]

plot "DATA" using 1:5:4 with filledcurve lc 4 title "abs", \
     "DATA" using 1:2 with lines lc 1 title "real", \
     "DATA" using 1:3 with lines lc 3 title "imag"

pause -1
