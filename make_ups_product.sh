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

flavor=NULL

INCS="BasicTypesProxy.h BasicTypesProxy.cxx"
BINS='parse_xml.py'

dest=$prodname_lower/$version

mkdir -p $dest || exit 1

mkdir -p ${dest}/$flavor/include/${prodname_mixed} || exit 1
echo "Copying files to ${dest}/${flavor}/include"
cp $INCS $dest/$flavor/include/${prodname_mixed}

mkdir -p ${dest}/$flavor/bin || exit 1
echo "Copying files to ${dest}/${flavor}/bin"
cp $BINS $dest/$flavor/bin


echo "Creating table file.."
mkdir -p ${dest}/${flavor}/ups || exit 1

cat >$dest/${flavor}/ups/${prodname_lower}.table <<EOF
 FILE=TABLE
 PRODUCT=${prodname_lower}
 VERSION=${version}

 FLAVOR=${flavor}
 QUALIFIERS = ""

 ACTION=SETUP
   setupEnv()
   proddir()
   EnvSet(${prodname_upper}_VERSION, \${UPS_PROD_VERSION} )
   EnvSet(${prodname_upper}_INCLUDE, \${UPS_PROD_DIR}/include )
   pathPrepend(PATH, \${UPS_PROD_DIR}/bin )
EOF

echo  "Declaring product ${prodname_lower} with version ${version} to UPS."

# This creates the .version directory
ups declare -f ${flavor} -z `pwd`/ \
     -r `pwd`/${prodname_lower}/${version}/${flavor}/ \
     -m ${prodname_lower}.table \
     ${prodname_lower} ${version} || exit 1

echo You can set up this product with:
echo setup srproxy $version -z `pwd`/
