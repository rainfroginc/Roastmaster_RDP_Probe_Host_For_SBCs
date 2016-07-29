## Important Notice

This is "pre-release" software for beta testers of Roastmaster for iOS10, due out in Fall 2016. This will NOT working with existing versions of Roastmaster (9 and below).

## Roastmaster RDP Probe Host (SBC)

Roastmaster_RDP_Probe_Host is is a customizable Single Board Computer (SBC) application to send thermocouple readings via the Roastmaster Datagram Protocol (RDP) to Roastmaster iOS over a WiFi Network. 

Roastmaster is coffee roasting toolkit and data logging software, in which users can log temperature data during their coffee roasting sessions. This logging can be done either manually or via separate electronic thermocouple reading "clients".

The RDP Protocol is an OpenSource communications protocol created by Rainfrog, Inc. for the purpose of standardizing the transmission of roasting information to Roastmaster.

Roastmaster_RDP_Probe_Host and the RDP protocol can function either alone, or alongside other hosts. Each host has a unique Serial Number string to identify itself to the server, which can negotiate simple SYN/ACK handshaking. So, we (the client) need only perform a multicast with our Serial Number and a synch (SYN) request, and await a response from Roastmaster (the server) in the form of an acknowledgement (ACK).

Once the ACK has been received, we commence sending our thermocouple data to the server's (Roastmaster's) IP address.

## Software Features

• Handles handshaking (SYN/ACK) with Roastmaster
• Hosts an unlimited number of thermocouple, each sending on a unique channel (RDP supports 16)
• Easy to setup and customize for those with limited coding knowledge

## RDP Protocol Features

• Operates over the easy to user User Datagram Protocol (UDP) protocol
• Lightweight, and consumes very little network bandwidth
• Server is multicast discoverable
• Supports basic handshaking (SYN/ACK), simulating a "connection" despite the "connectionless" nature of UDP
• Lack of transmission within 5 seconds will result in a "drop" in Roastamster, again simulating a "connection"
• Datagram format is compact, human-readable JSON
• Supports packet ordering, overcoming the inherit "orderless/best effort" design of UPD
• Supports multiple Roastmaster "Host->Server "connections" via unique Serial Number strings
• Support up to 16 individual channels per Host->Server connection

## Configuration

1) Enter your WiFi SSID
2) Enter your WiFi Password
3) Enter the Serial number string for this host, as defined in your Roastmaster probe definition
4) Enter the Network Port for this host, as defined in your Roastmaster probe definition
5) Modifiy the probes[] array to contain one entry per hardware probe
6) Set the SBC Board type and Amp Board usage flags

Other variables that affect execution can be altered in the User Options Section

## Resources

Thermocouple and Amp Boards Reference:
Adafruit MAX31855: https://www.adafruit.com/products/269 , Datasheet: https://cdn-shop.adafruit.com/datasheets/MAX31855.pdf
Adafruit MAX31850: https://www.adafruit.com/products/1727 , Datasheet: https://cdn-shop.adafruit.com/datasheets/MAX31850-MAX31851.pdf
Using a thermocouple: https://learn.adafruit.com/thermocouple/using-a-thermocouple
Signal Calibration: https://learn.adafruit.com/calibrating-sensors/maxim-31855-linearization

Arduino IDE
Installing Libraries in the Arduino IDE: https://www.arduino.cc/en/Guide/Libraries

Arduino Additional Board Manager URLs:
Feather Huzzah URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json

## Acknowledgements

Robert Swift - Thanks for your impetus, vision and code prototyping

## License

MIT License

Copyright (c) 2016] Rainfrog, Inc.
Written by Danny Hall, for Rainfrog, Inc.
Based on the prototyping of Robert Swift and input from countless Roastmaster users.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
