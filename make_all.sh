python config_watch.py -sa -S || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_a.pbw

python config_watch.py -sa -F au -S || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_au.pbw

python config_watch.py -sa || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_as.pbw

python config_watch.py -sb -S || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_b.pbw

python config_watch.py -sb || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_bs.pbw

python config_watch.py -sb -b || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_bsb.pbw

python config_watch.py -sc || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_c1.pbw

python config_watch.py -sc -c || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_c2.pbw

python config_watch.py -sd -S || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_d.pbw

python config_watch.py -sd || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_ds.pbw

python config_watch.py -se -S || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_e.pbw

python config_watch.py -se || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_es.pbw

python config_watch.py -se -b || exit
./waf build || exit
mv build/rosewright.pbw build/rosewright_esb.pbw
