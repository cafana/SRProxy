#!/bin/bash

if [ $# != 1 ]
then
    echo Usage: make_ups_product.sh VERSION
    exit 2
fi

version=$1

prodname_lower=srproxy
prodname_mixed=SRProxy
prodname_upper=SRPROXY

INCS="BasicTypesProxy.h BasicTypesProxy.cxx FlatBasicTypes.h IBranchPolicy.h"
BINS='gen_srproxy'

dest=$prodname_lower/$version

mkdir -p $dest || exit 1

mkdir -p ${dest}/include/${prodname_mixed} || exit 1
echo "Copying files to ${dest}/include"
cp $INCS $dest/include/${prodname_mixed}

mkdir -p ${dest}/bin || exit 1
echo "Copying files to ${dest}/bin"
cp $BINS $dest/bin


echo "Creating table file.."
mkdir -p ${dest}/ups || exit 1

cat >$dest/ups/${prodname_lower}.table <<EOF
FILE=TABLE
PRODUCT=${prodname_lower}
VERSION=${version}

FLAVOR=ANY
QUALIFIERS=py2

ACTION=SETUP
  setupEnv()
  proddir()
  EnvSet(${prodname_upper}_VERSION, \${UPS_PROD_VERSION} )
  EnvSet(${prodname_upper}_INC, \${UPS_PROD_DIR}/include )
  pathPrepend(PATH, \${UPS_PROD_DIR}/bin )

  setupRequired(castxml v0_00_00_f20180122)
  setupRequired(pygccxml v1_9_1a -q p2715a)

FLAVOR=ANY
QUALIFIERS=py3

ACTION=SETUP
  setupEnv()
  proddir()
  EnvSet(${prodname_upper}_VERSION, \${UPS_PROD_VERSION} )
  EnvSet(${prodname_upper}_INC, \${UPS_PROD_DIR}/include )
  pathPrepend(PATH, \${UPS_PROD_DIR}/bin )

  setupRequired(castxml v0_4_2)
  setupRequired(pygccxml v2_1_0c -q p392)

FLAVOR=ANY
QUALIFIERS=py3913

ACTION=SETUP
  setupEnv()
  proddir()
  EnvSet(${prodname_upper}_VERSION, \${UPS_PROD_VERSION} )
  EnvSet(${prodname_upper}_INC, \${UPS_PROD_DIR}/include )
  pathPrepend(PATH, \${UPS_PROD_DIR}/bin )

  setupRequired(castxml v0_4_2)
  setupRequired(pygccxml v2_2_1b -q p3913)

EOF

echo ${dest}.version
mkdir ${dest}.version || exit 1

for qual in py2 py3 py3913
do
cat > ${dest}.version/NULL_$qual <<EOF
FILE = version
PRODUCT = srproxy
VERSION = $version

FLAVOR = NULL
QUALIFIERS = "$qual"
  PROD_DIR = $prodname_lower/$version/
  UPS_DIR = ups
  TABLE_FILE = srproxy.table
EOF
done

echo You can set up this product with:
echo "setup $prodname_lower $version -z `pwd`:\$PRODUCTS -q py2/py3/py3913"
