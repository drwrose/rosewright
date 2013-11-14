python config_watch.py -sa -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_a.pbw

python config_watch.py -sa -F au -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_au.pbw

python config_watch.py -sa || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_as.pbw

python config_watch.py -sb -S -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_b.pbw

python config_watch.py -sb -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_bs.pbw

python config_watch.py -sb -b -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_bsb.pbw

python config_watch.py -sc || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c1.pbw

python config_watch.py -sc -c || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c2.pbw

python config_watch.py -sd -S -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_d.pbw

python config_watch.py -sd -I || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_ds.pbw

python config_watch.py -se -S || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_e.pbw

python config_watch.py -se || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_es.pbw

python config_watch.py -se -b || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_esb.pbw
