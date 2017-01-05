#!/bin/bash

# The Linux VM used by Travis has OpenEXR 1.x. We really want 2.x.

EXRINSTALLDIR=${EXRINSTALLDIR:=${PWD}/openexr-install}
EXRVERSION=${EXRVERSION:=2.2.0}
echo "EXR install dir will be: ${EXRINSTALLDIR}"

if [ ! -e ${EXRINSTALLDIR} ] ; then
    mkdir ${EXRINSTALLDIR}
fi

# Clone OpenEXR project (including IlmBase) from GitHub and build
if [ ! -e ./openexr ] ; then
    git clone -b v${EXRVERSION} https://github.com/openexr/openexr.git ./openexr
fi

pushd ./openexr
git checkout v${EXRVERSION} --force
cd IlmBase
./bootstrap && ./configure --prefix=${EXRINSTALLDIR} && make clean && make -j 4 && make install
cd ../OpenEXR
./bootstrap ; ./configure --prefix=${EXRINSTALLDIR} --with-ilmbase-prefix=${EXRINSTALLDIR} --disable-ilmbasetest && make clean && make -j 4 && make install
popd

ls -R ${EXRINSTALLDIR}

#echo "listing .."
#ls ..

