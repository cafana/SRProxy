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

INCS="BasicTypesProxy.h BasicTypesProxy.cxx"
BINS='parse_xml.py'

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
 QUALIFIERS = ""

 ACTION=SETUP
   setupEnv()
   proddir()
   EnvSet(${prodname_upper}_VERSION, \${UPS_PROD_VERSION} )
   EnvSet(${prodname_upper}_INC, \${UPS_PROD_DIR}/include )
   pathPrepend(PATH, \${UPS_PROD_DIR}/bin )

   # TODO is it possible to be less specific about these requirements?
   setupRequired(castxml v0_00_00_f20180122)
   setupRequired(pygccxml v1_9_1a -q p2715a)
EOF

echo  "Declaring product ${prodname_lower} with version ${version} to UPS."

# This creates the .version directory
ups declare -f NULL -z `pwd`/ \
     -r `pwd`/${prodname_lower}/${version}/ \
     -m ${prodname_lower}.table \
     ${prodname_lower} ${version} || exit 1

echo You can set up this product with:
echo setup srproxy $version -z `pwd`/ -z $EXTERNALS
