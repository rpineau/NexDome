#!/bin/bash

mkdir -p ROOT/tmp/NexDome_X2/
cp "../NexDome.ui" ROOT/tmp/NexDome_X2/
cp "../domelist NexDome.txt" ROOT/tmp/NexDome_X2/
cp "../build/Release/libNexDome.dylib" ROOT/tmp/NexDome_X2/

pkgbuild --root ROOT --identifier org.rti-zone.NexDome_X2 --sign "Developer ID Installer: Rodolphe PINEAU (PB97QXVYQC)" --scripts Scritps --version 1.0 NexDome_X2.pkg
rm -rf ROOT
