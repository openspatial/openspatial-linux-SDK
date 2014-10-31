**************************************************************************
This build system has been based on Ankidrive's opensource SDK code,
available at: https://github.com/anki/drive-sdk

Instructions to build and run the Nod's test-bench:

  a) to build bluez and test-bench

      $ mkdir build && cd build
      $ cmake .. -DBUILD_FRAMEWORK=ON
      $ make
      $ make install

  b) To run the test
      $ sudo build/dist/bin/test-bench

NOTE:
    1) Before running the test script, you will have to stop the
       bluetooth service running in the system using the command $sudo
       service bluetooth stop
    2) This build system has been tested on Ubuntu 14.04 x86_64 system
       There is a pre-requisite to install glib2.0 library on this platform.

***************************************************************************
Some known errors (which will be dealt in subsequent releases):

Error 1:
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Disconnected

disconnect_io()

(process:6353): GLib-CRITICAL **: g_io_channel_write_chars: assertion 'channel->is_writeable' failed
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Solution: Do the following on host:
          $ sudo service bluetooth start
          $ sudo service bluetooth stop
***************************************************************************
Error 2:
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Scanning BTLE devices in proximity...Press ^C to stop scanning
hci_send_req failed
Setting scan parameters failed -1
Scanning failed!! Quitting 1
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Solution: Do the following on the host: $ sudo hciconfig hci0 reset
***************************************************************************
Error 3:
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Characteristic value/descriptor read failed: Invalid handle

- OR -

Writing value for handle 0x003d failed

- OR -

Setting the notification enable failed 1
Device is not connected
Failed in reading/writing notification value. Quitting
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Solution: Kill the process and
          restart the hardware as well
***************************************************************************
