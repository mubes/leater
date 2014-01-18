# Use: gnuplot < plotout.cmd > a.pdf
set datafile separator ","
set terminal "pdf" enhanced font 'Verdana,9'

set tmargin 0
set bmargin 0
set lmargin 10
set rmargin 3

set style line 1 lc rgb '#8b1a0e' pt 1 ps 1 lt 1 lw 2 # --- red
set style line 2 lc rgb '#5e9c36' pt 6 ps 1 lt 1 lw 2 # --- green
set style line 11 lc rgb '#808080' lt 1
set border 3 back ls 11
set tics nomirror
set style line 12 lc rgb '#808080' lt 0 lw 1
set grid back ls 12

set xtics 60 font ",8"
set xtics nomirror out
set grid xtics ytics
set key bottom right font ",7"
set multiplot layout 2,1
set size ratio 0
set title "PID Controller"
set bmargin at screen 0.55
set ytics nomirror
set ytics out font ",8"
set tmargin 2
set yrange [0:275]
set ylabel "Temp (°C)"
plot "e1.csv" using ($1):($2/10) with lines title "Commanded Temp"  ls 1, \
     "e1.csv" using 1:($3/10) with lines title "Actual Temp" ls 2
#
unset title
set key top right
unset ylabel
set yzeroaxis
set yrange [-1100:1100]
set ytics nomirror out
set xtics nomirror out
set y2range [-5:105]
set ylabel " "
set bmargin at screen 0.08
plot "e1.csv" using ($1):($4) with lines title "P" lt 4, \
"e1.csv" using ($1):($5) with lines title "I" lt 5, \
"e1.csv" using ($1):($6) with lines title "D" lt 7, \
"e1.csv" using ($1):($7/10) axes x1y2 with lines title "Output" lt 3

unset multiplot
set output

