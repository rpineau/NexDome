#!/bin/bash

TheSkyX_Install=~/Library/Application\ Support/Software\ Bisque/TheSkyX\ Professional\ Edition/TheSkyXInstallPath.txt
echo "TheSkyX_Install = $TheSkyX_Install"

if [ ! -f "$TheSkyX_Install" ]; then
    echo TheSkyXInstallPath.txt not found
    TheSkyX_Path=`/usr/bin/find ~/ -maxdepth 3 -name TheSkyX`
    if [ ! -d "$TheSkyX_Path" ]; then
	   echo TheSkyX application was not found.
    	exit 1
	 fi
else
	TheSkyX_Path=$(<"$TheSkyX_Install")
fi

echo "Installing to $TheSkyX_Path"


if [ ! -d "$TheSkyX_Path" ]; then
    echo TheSkyX Install dir not exist
    exit 1
fi

if [ -d "$TheSkyX_Path/Resources/Common/PlugIns64" ]; then
	PLUGINS_DIR="PlugIns64"
elif [ -d "$TheSkyX_Path/Resources/Common/PlugInsARM32" ]; then
	PLUGINS_DIR="PlugInsARM32"
elif [ -d "$TheSkyX_Path/Resources/Common/PlugInsARM64" ]; then
	PLUGINS_DIR="PlugInsARM32"
else
	PLUGINS_DIR="PlugIns"
fi

cp "./domelist NexDome.txt" "$TheSkyX_Path/Resources/Common/Miscellaneous Files/"
cp "./NexDome.ui" "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/"
cp "./NexDome.png" "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/"
cp "./libNexDome.so" "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/"

app_owner=`/usr/bin/stat -c "%u" "$TheSkyX_Path" | xargs id -n -u`
if [ ! -z "$app_owner" ]; then
	chown $app_owner "$TheSkyX_Path/Resources/Common/Miscellaneous Files/domelist NexDome.txt"
	chown $app_owner "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/NexDome.ui"
	chown $app_owner "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/NexDome.png"
	chown $app_owner "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/libNexDome.so"
fi
chmod  755 "$TheSkyX_Path/Resources/Common/$PLUGINS_DIR/DomePlugIns/libNexDome.so"

