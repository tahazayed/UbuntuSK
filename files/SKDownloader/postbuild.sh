#!/bin/bash
echo "post build"

rm -rf ./build/

. ./.bashrc


PACKAGE_FOLDER="./build/skdownloader-$VERSION"


test -d $PACKAGE_FOLDER || mkdir -p $PACKAGE_FOLDER


chmod +755 ./bin/Release/SKDownloader
cp ./bin/Release/SKDownloader $PACKAGE_FOLDER/
cp ./SKDownloader.conf $PACKAGE_FOLDER/
cp ./SKDownloader.service $PACKAGE_FOLDER/
cp ./skdownloader_1.0.0.orig.tar.xz ./build/
touch $PACKAGE_FOLDER/SKDownloader.log
touch $PACKAGE_FOLDER/SKDownloader.pid
touch $PACKAGE_FOLDER/emptyfile.log
touch $PACKAGE_FOLDER/log.txt
cp ./make_package.sh $PACKAGE_FOLDER/
test -d $PACKAGE_FOLDER/debian || mkdir -p $PACKAGE_FOLDER/debian
cp -r ./debian/* $PACKAGE_FOLDER/debian

sed -i "s/1.0.0-1/$VERSION/g" $PACKAGE_FOLDER/debian/changelog
sed -i "s/Mon, 28 Aug 2017 12:43:06 +0200/$BUILDDATE/g" $PACKAGE_FOLDER/debian/changelog

cd ./build/skdownloader-*
. ./make_package.sh
