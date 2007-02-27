pilrc -allowEditID -q -ro -I Rsc Rsc/SamplePatchApp.rcp
pilrc -allowEditID -q -ro -I ../../Library/Rsc -I ../../Library/Inc ../../Library/Rsc/YahmLibrary.rcp
pilrc -q -ro Rsc/SamplePatchHack.rcp


LIB_SRC_PATH=../../Library/Src

arm-palmos-gcc -Wall -Wno-multichar -fshort-enums -DMY_CRID=\'BEAM\' -nostartfiles -D_ARM_HACK_ -c -o code03e8.o Src/code03e8.c
arm-palmos-gcc -nostartfiles -Wl,-T,myscript.ls -o code03e8 code03e8.o ../../../lib/libarmboot.a

m68k-palmos-gcc -I ./Rsc -I ../../Library/Inc -o SamplePatchApp AppSrc/SamplePatchApp.c $LIB_SRC_PATH/gccrelocate.c $LIB_SRC_PATH/getsettrapaddress.c $LIB_SRC_PATH/initialization.c $LIB_SRC_PATH/lowlevel.c $LIB_SRC_PATH/trapcontrol5.c

./build-prc --no-check-resources -o SamplePatchApp.prc -n "SamplePatchApp" -c BEAM -t 'appl' Rsc/SamplePatchApp.ro code03e8 SamplePatchApp ../../Library/Rsc/YahmLibrary.ro Rsc/SamplePatchHack.ro
