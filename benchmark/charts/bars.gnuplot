# bars.gnuplot - one bar per version, coloured by program, value printed on top.
# dat columns: index  "label"  value  colour (0xRRGGBB as a number)  "text"
# Also needs n (number of bars) and the style.gnuplot variables.

load 'benchmark/charts/style.gnuplot'
unset key
set boxwidth 0.7
set xrange [-0.6 : n - 0.4]

plot dat using 1:3:(0.7):4:xtic(2) with boxes lc rgb variable, \
     dat using 1:3:5 with labels offset 0,0.8 font ",10" textcolor rgb "#0b0b0b"
