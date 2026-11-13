# style.gnuplot - the shared look of every benchmark chart.
# Loaded by bars / groups / stack.gnuplot. ladder_bench passes these variables:
#   dat, out   data file and PNG to write       ttl, sub   title and sub-title ('' = none)
#   ylab       y-axis label                     w, h       picture size in pixels
#   logy       1 = log y axis                   ylo, yhi   smallest / largest value
#   ref        > 0 draws a dashed line there (e.g. 1x = Version 0)

set terminal pngcairo size w,h font "Arial,11" background rgb "#fcfcfb"
set output out
set termoption noenhanced                      # underscores in names stay underscores
set title (strlen(sub) > 0 ? ttl."\n".sub : ttl) textcolor rgb "#0b0b0b" font ",15"
set ylabel ylab textcolor rgb "#52514e"
set border 3 lc rgb "#8a8984"
set tics nomirror textcolor rgb "#52514e"
set grid ytics lc rgb "#e4e3df" lw 1
set key top right textcolor rgb "#0b0b0b" box lc rgb "#e4e3df" opaque
set style fill solid 1.0 border rgb "#fcfcfb"  # thin light gap between bars
set xtics rotate by 30 right
set bmargin 9

# one colour per program / series, always in this order (colour-blind checked)
colours = "#2a78d6 #eb6834 #1baf7a #eda100 #e87ba4 #008300 #6250d6 #e34948"

if (logy) {
    # log axis with readable ticks: 1, 2, 5, 10, 20, 50, ...
    set logscale y
    set yrange [ylo/2.5 : yhi*3]
    ticks = ""
    do for [e=-3:9] {
        do for [k=0:2] {
            t = (k == 0 ? 1 : k == 1 ? 2 : 5) * 10.0**e
            if (t >= ylo/2.5 && t <= yhi*3) {
                ticks = ticks . (strlen(ticks) > 0 ? ", " : "") . sprintf('"%g" %g', t, t)
            }
        }
    }
    eval "set ytics (" . ticks . ")"
} else {
    set yrange [0 : (ref > yhi ? ref : yhi) * 1.18]
}

if (ref > 0) {
    set arrow from graph 0, first ref to graph 1, first ref nohead dt 2 lw 1.5 lc rgb "#52514e" front
}
