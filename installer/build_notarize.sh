#!/bin/bash

<<<<<<< HEAD
PACKAGE_NAME="NexDome_X2.pkg"
BUNDLE_NAME="org.rti-zone.NexDomeX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libNexDome.dylib
fi

=======
>>>>>>> 4d0ae08f880488544ec4351ca35162db43e2eaa0
mkdir -p ROOT/tmp/NexDome_X2/
cp "../NexDome.ui" ROOT/tmp/NexDome_X2/
cp "../NexDome.png" ROOT/tmp/NexDome_X2/
cp "../domelist NexDome.txt" ROOT/tmp/NexDome_X2/
cp "../build/Release/libNexDome.dylib" ROOT/tmp/NexDome_X2/

<<<<<<< HEAD

=======
PACKAGE_NAME="NexDome_X2.pkg"
BUNDLE_NAME="org.rti-zone.NexDomeX2"
>>>>>>> 4d0ae08f880488544ec4351ca35162db43e2eaa0
if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}
<<<<<<< HEAD
	res=`xcrun altool --notarize-app --primary-bundle-id $BUNDLE_NAME --username "$AC_USERNAME" --password "@keychain:AC_PASSWORD" --file $PACKAGE_NAME`
=======
	res=`xcrun altool --notarize-app --primary-bundle-id $BUNDLE_NAME --username "pineau@rti-zone.org" --password "@keychain:AC_PASSWORD" --file $PACKAGE_NAME`
>>>>>>> 4d0ae08f880488544ec4351ca35162db43e2eaa0
	RequestUUID=`echo $res | grep RequestUUID | cut -d"=" -f 2 | tr -d [:blank:]`
	if [ -z "$RequestUUID" ]; then
		echo "Error notarizing"
		exit 1
	fi
	echo "Notarization RequestUUID $RequestUUID"
	sleep 30
<<<<<<< HEAD
=======
	n=0
>>>>>>> 4d0ae08f880488544ec4351ca35162db43e2eaa0
	while true
	echo "Waiting for notarization"
	do
		res=`xcrun altool --notarization-info $RequestUUID --username "pineau@rti-zone.org" --password "@keychain:AC_PASSWORD"`
		pkg_ok=`echo $res | grep -i "Package Approved"`
<<<<<<< HEAD
		pkg_in_progress=`echo $res | grep -i "in progress"`
		if [ ! -z "$pkg_ok" ]; then
			break
		elif [ ! -z "$pkg_in_progress" ]; then
			sleep 60
		else
=======
		if [ ! -z "$pkg_ok" ]; then
			break
		fi
		sleep 30
		n=$((n+1))
		if [ $n -eq 10 ]; then
>>>>>>> 4d0ae08f880488544ec4351ca35162db43e2eaa0
			echo  "Notarization timeout or error"
			exit 1
		fi
	done
	xcrun stapler staple $PACKAGE_NAME

else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
