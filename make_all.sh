
python config_watch.py -sa "$@" || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_a.pbw

python config_watch.py -sb "$@" || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_b.pbw

python config_watch.py -sc "$@" || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c.pbw

python config_watch.py -sc2 "$@" || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_c2.pbw

python config_watch.py -sd "$@" || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_d.pbw

python config_watch.py -se -x "$@" || exit
pebble build || exit
mv build/rosewright.pbw build/rosewright_e.pbw
