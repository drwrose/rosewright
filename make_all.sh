python config_watch.py -sa -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_a.pbw

python config_watch.py -sa -F au -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_au.pbw

python config_watch.py -sb -S -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_b.pbw

python config_watch.py -sc -c || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c2.pbw

python config_watch.py -sd -S -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_d.pbw

python config_watch.py -se -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_e.pbw
