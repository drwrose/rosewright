python config_watch.py -sa -S -w || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_a.pbw

python config_watch.py -sa -F au -S -w || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_au.pbw

python config_watch.py -sb -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_b.pbw

python config_watch.py -sc -c -w -x || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c2.pbw

python config_watch.py -sd -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_d.pbw

python config_watch.py -se -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_e.pbw
