python config_watch.py -sa -x || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_a.pbw

python config_watch.py -sb -x || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_b.pbw

python config_watch.py -sc -c -x || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c2.pbw

python config_watch.py -sd -x || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_d.pbw

python config_watch.py -se -x || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_e.pbw
