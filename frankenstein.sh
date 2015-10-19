# This script is meant to assemble pbw's from two separate builds into
# a set of universal pbw's.  It's designed for the specific purpose of
# the 4.1 release which must be this kind of Frankenstein build, at
# least until the official SDK 3.6 is released.

pbws=`(cd build_basalt && echo *.pbw)`
echo $pbws

stage=frankenstein_stage
rm -rf $stage

for pbw in $pbws; do
    echo $pbw
    mkdir $stage

    (cd $stage && unzip ../build_basalt/$pbw) || exit
    (cd $stage && unzip -o ../build_chalk/$pbw) || exit
    sed '/targetPlatforms/s/"chalk"/"aplite", "basalt", "chalk"/' <$stage/appinfo.json >$stage/t || exit
    mv $stage/t $stage/appinfo.json || exit
    rm -f build_combined/$pbw
    (cd $stage && zip -r ../build_combined/$pbw .) || exit
    
    rm -rf $stage
done
