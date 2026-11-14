# groups.gnuplot - grouped bars: one group per row of dat, one bar (colour) per column.
# dat: a header row ("group" "series 1" "series 2" ...), then "row label" v1 v2 ...
#      NaN = no bar.
# Also needs ncols (number of series), fmt (0 = times, 1 = speedups "1.23x"),
# cidx (colour number of each series, e.g. '1 3' when series 2 is left out)
# and the style.gnuplot variables.

load 'benchmark/charts/style.gnuplot'
set style data histogram
set style histogram cluster gap 1
set boxwidth 0.9 relative
set xtics rotate by 20 right
set bmargin 7

N = ncols + 1                                   # data columns 2 .. N
lab(v) = fmt == 1 ? sprintf("%.2fx", v) : (v >= 1000 ? sprintf("%.2fs", v/1000.0) : sprintf("%.0fms", v))

# bars; the header row names the series in the legend
# labels: "skip 1" jumps the header so row 0 is the first group, and each label
# sits over its bar: group centre + offset of bar i inside the group
plot for [i=2:N] dat using i:xtic(1) title columnheader(i) lc rgb word(colours, int(word(cidx, i-1))), \
     for [i=2:N] dat skip 1 using ($0 + (i-2-(N-2)/2.0)/N):i:(lab(column(i))) \
         with labels offset 0,0.7 font ",8" textcolor rgb "#0b0b0b" notitle
