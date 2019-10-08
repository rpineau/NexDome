#!/bin/bash

mkdir -p ROOT/tmp/NexDome_X2/
cp "../NexDome.ui" ROOT/tmp/NexDome_X2/
cp "../NexDome.png" ROOT/tmp/NexDome_X2/
cp "../domelist NexDome.txt" ROOT/tmp/NexDome_X2/
cp "../build/Release/libNexDome.dylib" ROOT/tmp/NexDome_X2/

PACKAGE_NAME="NexDome_X2.pkg"
BUNDLE_NAME="org.rti-zone.NexDomeX2"
if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}
	res=`xcrun altool --notarize-app --primary-bundle-id $BUNDLE_NAME --username "pineau@rti-zone.org" --password "@keychain:AC_PASSWORD" --file $PACKAGE_NAME`
	RequestUUID=`echo $res | grep RequestUUID | cut -d"=" -f 2 | tr -d [:blank:]`
	if [ -z "$RequestUUID" ]; then
		echo "Error notarizing"
		exit 1
	fi
	echo "Notarization RequestUUID $RequestUUID"
	sleep 30
	n=0
	while true
	echo "Waiting for notarization"
	do
		res=`xcrun altool --notarization-info $RequestUUID --username "pineau@rti-zone.org" --password "@keychain:AC_PASSWORD"`
		pkg_ok=`echo $res | grep -i "Package Approved"`
		if [ ! -z "$pkg_ok" ]; then
			break
		fi
		sleep 30
		n=$((n+1))
		if [ $n -eq 10 ]; then
			echo  "Notarization timeout or error"
			exit 1
		fi
	done
	xcrun stapler staple $PACKAGE_NAME

else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT