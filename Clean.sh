#!/bin/sh
rm -rf Final

cd SimpleSocketServer
make clean
cd ../SimpleSocketClient
make clean
cd ../ServerPacketHandlerLibrary
make clean
