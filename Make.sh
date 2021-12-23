#!/bin/sh
rm -rf Final

cd SimpleSocketServer
make
cd ../SimpleSocketClient
make
cd ../ServerPacketHandlerLibrary
make
cd ..

mkdir Final
cd Final
mkdir "Client 1"
mkdir "Client 2"
mkdir "Client 3"
mkdir "Client 4"
mkdir "Client 5"
mkdir "Client 6"
mkdir Server
cd Server
mkdir Data
cd ../..

cd SimpleSocketServer/bin/Release
cp SimpleSocketServer ../../../Final/Server/Server
cd ../../..
cd SimpleSocketClient/bin/Release
cp SimpleSocketClient "../../../Final/Client 1/Client"
cp SimpleSocketClient "../../../Final/Client 2/Client"
cp SimpleSocketClient "../../../Final/Client 3/Client"
cp SimpleSocketClient "../../../Final/Client 4/Client"
cp SimpleSocketClient "../../../Final/Client 5/Client"
cp SimpleSocketClient "../../../Final/Client 6/Client"
cd ../../..
cd ServerPacketHandlerLibrary/bin/Release
cp ServerPacketHandlerLibrary.so ../../../Final/Server/PacketHandler.so
