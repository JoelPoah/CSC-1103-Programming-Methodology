# Draws price, short SMA, long SMA and BUY/SELL markers from signals.dat
# Columns: 1 trade  2 price  3 sma_short  4 sma_long  5 signal (+1 BUY, -1 SELL, 0 none)
# Run from the project root:  gnuplot stageB/plot_signals.gnuplot
set terminal pngcairo size 1400,700 enhanced
set output "stageB/signals.png"
set title "Stage B: SMA crossover signals"
set xlabel "trade number"
set ylabel "price (USDT)"
set grid
set key outside top right

plot "stageB/signals.dat" using 1:2 with lines lc rgb "#bbbbbb" title "price", \
     "" using 1:3 with lines lw 2 lc rgb "#1f77b4" title "SMA short", \
     "" using 1:4 with lines lw 2 lc rgb "#ff7f0e" title "SMA long", \
     "" using ($5==1 ? $1 : 1/0):3 with points pt 9 ps 2 lc rgb "#2ca02c" title "BUY", \
     "" using ($5==-1 ? $1 : 1/0):3 with points pt 11 ps 2 lc rgb "#d62728" title "SELL"
