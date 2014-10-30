# libbluez

This is a minimal library consisting of code from [BlueZ 5.13][bluez] that is required
for Bluetooth LE. This SDK is inspired from Anki drive. This SDK only requires the C
interface to access Bluetooth LE methods supported by the Linux kernel. This small package
simplifies the build procedure, but eliminating the requirement to install dependencies and
build the entire BlueZ 5.x package.

[bluez]: http://www.bluez.org/
[Anki Drive]: http://developer.anki.com/drive-sdk/

## Build
==========
mkdir build
cd build
cmake ..
make install

## Build artifacts

- libbluez.a: Static bluez library.
- include/bluez: Public header files for Bluetooth LE and GATT functions.
- hciconfig: The 'hciconfig' tool for managing the HCI interface(eg: bluetooth USB dongle)


