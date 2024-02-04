#!/bin/bash
########################################
NF="sparkaggr"
########################################
THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

cd $SDE/pkgsrc/p4-build
make clean

./autogen.sh
./configure --prefix=$SDE_INSTALL --with-tofino P4_NAME=${NF} P4_PATH=${THIS_DIR}/main.p4 --enable-thrift
make -j8
make install

sed -e "s/TOFINO_SINGLE_DEVICE/${NF}/" $SDE/pkgsrc/p4-examples/tofino/tofino_single_device.conf.in > $SDE_INSTALL/share/p4/targets/tofino/${NF}.conf