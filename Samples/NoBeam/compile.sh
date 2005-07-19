pilrc -q -ro Rsc/NoBeam.rcp
arm-palmos-gcc -Wall -Wno-multichar -fshort-enums -DMY_CRID=\'BEAM\' -nostartfiles -D_ARM_HACK_ -c -o code03e8.o Src/code03e8.c
arm-palmos-gcc -nostartfiles -o code03e8 code03e8.o ../../../lib/libarmboot.a
build-prc --no-check-resources -o NoBeam.prc -n "NoBeam" -c BEAM -t 'HACK' Rsc/NoBeam.ro code03e8
