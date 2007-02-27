pilrc -q -ro Rsc/SamplePatchHack.rcp
arm-palmos-gcc -Wall -Wno-multichar -fshort-enums -DMY_CRID=\'BEAM\' -nostartfiles -D_ARM_HACK_ -c -o code03e8.o Src/code03e8.c
arm-palmos-gcc -nostartfiles -o code03e8 code03e8.o Src/libarmboot.a
./build-prc --no-check-resources -o SamplePatch.prc -n "SamplePatch" -c BEAM -t 'HACK' Rsc/SamplePatchHack.ro code03e8
