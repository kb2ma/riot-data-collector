# CoAP Data Collector

RIOT example app to collect temperature readings and report them via CoAP. Uses a JC-42.4 compliant temperature sensor via RIOT's SAUL sensor/actuator API. Uses the CoAP Observe exension to report the data with RIOT's gcoap implementation.

Below are two sections to help set up the data collector, for networking and for the server that listens for the temperature readings.

# Networking Setup
Below are subsections for the supported networking types. Also, check the make file for required addresses.<br>

| Name | Description |
| ---- | ----------- |
| DATAHEAD_ADDR | Address of listening server |
| DATAHEAD_PORT | CoAP port for listening server |

## SLIP networking
Supports a board connected via serial interface. Build with the SLIP-specific make file, `Makefile.slip`:

    make -f Makefile.slip BOARD="samr21-xpro"

See the RIOT border router example [documentation][1] for setup and use of SLIP. Verify addresses for the SLIP interfaces in the make file.<br>

| Name | Description |
| ---- | ----------- |
| SLIP_ADDR | Address of RIOT gateway |
| SLIP_PEER_ADDR | Address of connected host, for example a Linux laptop |
| BORDER_IF | Serial network interface number |

On my system, I test with the tun networking setup below.

    $ cd ~/dev/riot/repo/dist/tools/tunslip/
    $ sudo ./tunslip6 -s ttyUSB0 -t tun0 bbbb::1/64

The setup above must be run _before_ the data collector tries to use the network. The data collector waits 20 seconds after it boots before attempting to use the network.

## RPL wireless networking
Supports a board connected wirelessly via the RPL routing protocol. Requires a second gateway router to forward traffic to the data server. Build with the standard `Makefile`:

    make BOARD="samr21-xpro" NET_ROUTER=1

To run this configuration, be sure to set up the gateway router first, including establishing it as RPL root. Also, be sure the values below are defined as expected in the make file.<br>

| Name | Description |
| ---- | ----------- |
| HOST_ADDR | Address of host |
| ROUTER_IF | Network interface number |

By default, uses `aaaa` as the prefix for the RPL interface on the host and gateway. See the RIOT RPL [tutorial][2] for more information about use of RPL. See the gcoap SLIP example [documentation][3] for gateway router setup with RPL.

You should see output like below when starting a data collector.

    $ make term BOARD="samr21-xpro" PORT="/dev/ttyACM1"
    pyterm -p "/dev/ttyACM1" -b "115200"
    2017-05-15 05:50:49,250 - INFO # Connect to serial port /dev/ttyACM1
    Welcome to pyterm!
    Type '/exit' to exit.
    2017-05-15 05:51:05,028 - INFO # Initialized router address
    2017-05-15 05:51:05,031 - INFO # Initialized RPL; pause 10 sec.
    2017-05-15 05:51:15,045 - INFO # Initialized router RPL networking
    2017-05-15 05:51:15,048 - INFO # Sending hello message to datahead
    2017-05-15 05:51:15,051 - INFO # temperature: 19.37 C
    2017-05-15 05:51:15,071 - INFO # 'hello' response OK
    2017-05-15 05:51:35,074 - INFO # temperature: 19.37 C
    2017-05-15 05:51:55,097 - INFO # temperature: 19.43 C

## Native networking
This implementation is designed only to test use of CoAP. RIOT does not support use of a physical sensor in this configuration.

Build with the standard `Makefile`. Follow the setup [instructions][4] for the gnrc_networking example. On my system, I test with the tap networking setup below.

    $ sudo ip tuntap add tap0 mode tap user kbee
    $ sudo ip link set tap0 up
    # Data collector sends to DATAHEAD_ADDR in Makefile.
    # Must define this address.
    $ sudo ip address add fe80::bbbb:1/64 dev tap0


# Datahead Server
The RIOT data collector expects to communicate with a CoAP listener interested in the data. The interface between them includes these resources: <br>

| Resource | Location | Description |
| -------- | -------- | ----------- |
| /dh/lo   | Listener | "Hello", registers RIOT host with listener (POST) |
| /dh/tmp  | Host | Listener observes this resource on RIOT host (GET) |

I use [Datahead][5] to provide the server side of the interface.


# Hardware
We connect the Adafruit MCP9808 [breakout board](https://www.adafruit.com/products/1782) to a SAMR21 Xplained Pro board via I2C. The pins are:

| Function | SAMR21 | MCP9808 |
| -------- | ------ | ------- |
| I2C SCL  | PA17   | SCL     |
| I2C SDA  | PA16   | SDA     |
| 3.3V     | VCC    | Vdd     |
| Ground   | GND    | Gnd     |


[1]: https://github.com/RIOT-OS/RIOT/tree/master/examples/gnrc_border_router  "documentation"
[2]: https://github.com/RIOT-OS/RIOT/wiki/Tutorial%3A-RIOT-and-Multi-Hop-Routing-with-RPL    "tutorial"
[3]: https://github.com/RIOT-OS/RIOT/blob/master/examples/gcoap/README-slip.md    "documentation"
[4]: https://github.com/RIOT-OS/RIOT/tree/master/examples/gnrc_networking    "instructions"
[5]: https://github.com/kb2ma/datahead  "Datahead"
 
