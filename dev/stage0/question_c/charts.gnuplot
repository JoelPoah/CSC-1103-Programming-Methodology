# Stage 0 question c charts. Data = MEDIANS of 5 runs from dev/stage0/question_b/output.txt (n = 10000).
# Laptop: pinned to CPU 4. Pi: pinned with taskset -c 2, performance governor.
# Run from this folder:  gnuplot charts.gnuplot

# columns: flag, laptop loop, laptop recursion, pi loop, pi recursion
$data << EOD
-O0   4700      332400   26715    76446
-O1   2700      345500   8386     62931
-O2   0         0        55       55
-O3   100       0        56       56
EOD

BLUE   = "#2a78d6"
ORANGE = "#eb6834"
INK    = "#0b0b0b"
MUTED  = "#52514e"
GRID   = "#e4e3df"

fmt(x) = x == 0 ? "0" :x >= 100000 ? sprintf("%.0fk", x/1000.0) : x >= 1000 ? sprintf("%.1fk", x/1000.0) : sprintf("%.0f", x)
# log axis has no 0: a 0 ns median (below the 100 ns laptop timer step) gets no bar,
# and its label sits just above the bottom of the axis instead
v(x)  = x > 0 ? x : 1/0
ly(x) = x > 0 ? x*1.35 : 14

set terminal pngcairo size 900,560 font "Arial,11" background "#fcfcfb"
set style data histogram
set style histogram clustered gap 1
set style fill solid 1.0 border rgb "#fcfcfb"
set boxwidth 0.9
set logscale y 10
set yrange [10:3000000]
set format y "10^{%L}"
set ytics nomirror textcolor rgb MUTED
set xtics nomirror scale 0 textcolor rgb INK
set border 3 lc rgb MUTED
set grid ytics lc rgb GRID lw 1 dt 1
set key top right textcolor rgb INK samplen 1.5 spacing 1.2
set ylabel "Median time (ns, log scale)" textcolor rgb MUTED
set label 99 "-O2/-O3: n is fixed, so gcc computes the sum at compile time. Times are timer resolution, not work. 0 = under the laptop timer's 100 ns step." \
    at screen 0.5, screen 0.03 center textcolor rgb MUTED font "Arial,9"
set bmargin 4.5

# label positions: 2 bars per group, each 1/3 wide
L = -1.0/6
R =  1.0/6

# ---------- 1. Laptop (Windows) ----------
set output "windows_chart_b.png"
set title "Laptop (Windows): Loop vs Recursion by GCC optimisation level" textcolor rgb INK font "Arial,13"
plot $data using (v($2)):xtic(1) title "Loop" lc rgb BLUE, \
     ''    using (v($3)) title "Recursion" lc rgb ORANGE, \
     ''    using ($0+L):(ly($2)):(fmt($2)) with labels notitle textcolor rgb INK font "Arial,9", \
     ''    using ($0+R):(ly($3)):(fmt($3)) with labels notitle textcolor rgb INK font "Arial,9"

# ---------- 2. Raspberry Pi ----------
set output "rpi_chart_b.png"
set label 99 "-O2/-O3: n is fixed, so gcc computes the sum at compile time. Times are timer resolution, not work." \
    at screen 0.5, screen 0.03 center textcolor rgb MUTED font "Arial,9"
set title "Raspberry Pi: Loop vs Recursion by GCC optimisation level"
plot $data using (v($4)):xtic(1) title "Loop" lc rgb BLUE, \
     ''    using (v($5)) title "Recursion" lc rgb ORANGE, \
     ''    using ($0+L):(ly($4)):(fmt($4)) with labels notitle textcolor rgb INK font "Arial,9", \
     ''    using ($0+R):(ly($5)):(fmt($5)) with labels notitle textcolor rgb INK font "Arial,9"

# ---------- 3. Laptop vs Raspberry Pi ----------
set terminal pngcairo size 1200,560 font "Arial,11" background "#fcfcfb"
set output "compare_chart_b.png"
unset label 99
set multiplot layout 1,2 title "Laptop (Windows) vs Raspberry Pi" font "Arial,14" \
    margins 0.07,0.98,0.16,0.86 spacing 0.07,0
set label 99 "-O2/-O3: sum computed at compile time; times are timer resolution, not work. 0 = under the laptop timer's 100 ns step." \
    at screen 0.5, screen 0.03 center textcolor rgb MUTED font "Arial,9"

set title "Loop"
plot $data using (v($2)):xtic(1) title "Laptop" lc rgb BLUE, \
     ''    using (v($4)) title "Raspberry Pi" lc rgb ORANGE, \
     ''    using ($0+L):(ly($2)):(fmt($2)) with labels notitle textcolor rgb INK font "Arial,9", \
     ''    using ($0+R):(ly($4)):(fmt($4)) with labels notitle textcolor rgb INK font "Arial,9"

unset label 99
set title "Recursion"
plot $data using (v($3)):xtic(1) title "Laptop" lc rgb BLUE, \
     ''    using (v($5)) title "Raspberry Pi" lc rgb ORANGE, \
     ''    using ($0+L):(ly($3)):(fmt($3)) with labels notitle textcolor rgb INK font "Arial,9", \
     ''    using ($0+R):(ly($5)):(fmt($5)) with labels notitle textcolor rgb INK font "Arial,9"
unset multiplot
