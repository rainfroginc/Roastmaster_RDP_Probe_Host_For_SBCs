
/*
  ======================================================== Roastmaster_RDP_Probe_Host_SBC =====================================================
  ================================================================ Version 0.9.0 ==============================================================

  IMPORTANT NOTICE: This is "pre-release" software for beta testers of Roastmaster for iOS10, due out in Fall 2016. 
  This will NOT working with existing versions of Roastmaster (9 and below).

  Roastmaster_RDP_Probe_Host_SBC is a customizable application to send thermocouple readings via the Roastmaster Datagram Protocol (RDP)
  to Roastmaster iOS over a WiFi Network.

  Roastmaster is coffee roasting toolkit and data logging software, in which users can log temperature data during their coffee roasting sessions.
  This logging can be done either manually or via separate electronic thermocouple reading "clients".

  The RDP Protocol is an OpenSource communications protocol created by Rainfrog, Inc. for the purpose of standardizing the transmission of
  roasting information to Roastmaster.

  Roastmaster_RDP_Probe_Host_SBC and the RDP protocol can function either alone, or alongside other hosts. Each host has a unique Serial Number string to
  identify itself to the server, which can negotiate simple SYN/ACK handshaking. So, we (the client) need only perform a multicast with our
  Serial Number and a synch (SYN) request, and await a response from Roastmaster (the server) in the form of an acknowledgement (ACK).

  Once the ACK has been received, we commence sending our thermocouple data to the server's (Roastmaster's) IP address.

  Roastmaster_RDP_Probe_Host_SBC Features:
    • Handles handshaking (SYN/ACK) with Roastmaster
    • Hosts an unlimited number of thermocouple, each sending on a unique channel (RDP supports 16)
    • Easy to setup and customize for those with limited coding knowledge

  RDP Protocol Features:
    • Operates over the easy to user User Datagram Protocol (UDP) protocol
    • Lightweight, and consumes very little network bandwidth
    • Server is multicast discoverable
    • Supports basic handshaking (SYN/ACK), simulating a "connection" despite the "connectionless" nature of UDP
    • Lack of transmission within 5 seconds will result in a "drop" in Roastmaster, again simulating a "connection"
    • Datagram format is compact, human-readable JSON
    • Supports packet ordering, overcoming the inherit "orderless/best effort" design of UPD
    • Supports multiple Roastmaster "Host->Server "connections" via unique Serial Number strings
    • Support up to 16 individual channels per Host->Server connection

  ================================================================ CONFIGURATION =============================================================

  Just 6 steps - half as easy as quitting drinking or smoking.

    1) Enter your WiFi SSID
    2) Enter your WiFi Password
    3) Enter the Serial number string for this host, as defined in your Roastmaster probe definition
    4) Enter the Network Port for this host, as defined in your Roastmaster probe definition
    5) Modifiy the probes[] array to contain one entry per hardware probe
    6) Set the SBC Board type and Amp Board usage flags

  Other variables that affect execution can be altered in the User Options Section

  ================================================================== RESOURCES ===============================================================

  Thermocouple and Amp Boards Reference:
    Adafruit MAX31855: https://www.adafruit.com/products/269 , Datasheet: https://cdn-shop.adafruit.com/datasheets/MAX31855.pdf
    Adafruit MAX31850: https://www.adafruit.com/products/1727 , Datasheet: https://cdn-shop.adafruit.com/datasheets/MAX31850-MAX31851.pdf
    Using a thermocouple: https://learn.adafruit.com/thermocouple/using-a-thermocouple
    Signal Calibration: https://learn.adafruit.com/calibrating-sensors/maxim-31855-linearization

  Arduino IDE
    Installing Libraries in the Arduino IDE: https://www.arduino.cc/en/Guide/Libraries

  Arduino Additional Board Manager URLs:
    Feather Huzzah URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json

  =============================================================== ACKNOWLEDGEMENTS ============================================================

  Special Thanks:
    Robert Swift - Impetus, Vision and Code Prototyping

  =============================================================================================================================================
*/

//MIT License
//
//Copyright (c) 2016] Rainfrog, Inc.
//Written by Danny Hall, for Rainfrog, Inc.
//Based on the prototyping of Robert Swift and input from countless Roastmaster users.
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.


#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <float.h> // provides min/max double values
extern "C" {
#include "user_interface.h" // needed for the timer
}
//Thermocouple Breakout Board Libraries will be imported later, based on user #define's as to which chips they are using

typedef enum {
  max31850AmpType = 1,
  max31855AmpType,
} AmpType;

struct Probe {
  int serverChannel;
  int ampType;
  int pinCLK;
  int pinCS;
  int pinDO;
  void *probeHandle;
  double temp;
  int readCount;
  bool readError;
};

//==========================================================================================================
//=========================================== Begin User Options ===========================================
//==========================================================================================================

/*
   Different SBCs have different optimal Baud Rates (data speeds)
   Set the optimal rate for you SBC.
*/
const unsigned int baudRate = 115200;

/*
   Comment out to disable logging, conserving SBC RAM and clock cycles
*/
#define LOGGING

/*
   The SBC GPIO pins for Status and Error LEDs
   Comment out to disable LED status indication for a given LED
   STATUS_LED:
   3 short blinks: Connecting to WiFi
   2 short blinks: Connected, and sending SYN–waiting for a reply
   1 long blink: Reading probe and sending temp datagram (lit for the duration of time the code executes)
*/
#define STATUS_LED 12
#define ERROR_LED 13

/*
   Step 1 and Step 2:
   The SSID (name) of your WiFi network, and its associated password
*/
const char wifiSSID[] = "YourSSID";
const char wifiPassword[] = "YourPassword";

/*
   Step 3:
   The serial number set in the Roastmaster Probe definition.
   Roastmaster will spawn as many "Servers" as needed for active probes based on the number of UNIQUE server IDs in the definitions of those probes.
   Each server will only respond to its serial number, and is limited to 16 unique Channels.
*/
const char serverSerial[] = "My Probe Server";

/*
   Step 4:
   The port set in the Roastmaster Probe definition
*/
const int serverPort = 5050;


/*
   Step 5
   Define the probe structure of each TC breakout board, assigning a unique channel to each.
   This channel should match the corresponding Probe definition in Roastmaster.
   RDC supports 16 channels.

   Modify the first annotated entry. Uncomment the next un-annoted entry, and enter values.
   Copy and paste as many probes as there are amp boards on your SBC
*/
struct Probe probes[] = {
  { //First Probe
    1, /*Channel: The channel defined in the Roastmaster Probe.*/
    max31855AmpType, /*Amp Type: Once of the AmpType enum constants defined at top.*/
    5, /*CLK (Clock): Your SBC's GPIO pin wired to the TC board's CLK pin.*/
    4, /*CS (Ship Select): Your SBC's GPIO pin wired to the TC board's CLK pin.*/
    2, /*DO (Data Out): Your SBC's GPIO pin wired to the TC board's CLK pin.*/
  },
  //{Channel, AmpType, CLK, CS, DO}, //Next Probe
  //{Channel, AmpType, CLK, CS, DO}, //Next Probe
  //…and so on. RDP supports 16 channels
};

typedef enum {
  featherHuzzahSBC = 1,
  raspberryPiSBC,
} SBCType; //(Single Board Computer)

/*
   Step 6:
   Which SBC and amp boards are you using?
   Setting the SBC value and Amp board flags prevent unecessary #includes,
   preventing large libraries from being written to SBC, saving space and resources
*/
#define boardType featherHuzzahSBC
#define usingMax31855Amp true
#define usingMax31850Amp false

/*
   Rates (in seconds) at which synch (SYN) and temperature datagrams will be sent.
*/
#define syncSendRate 2
#define tempSendRate 1

/*
   The multicast address to use when searching for a server via SYN datagrams.
   224.0.0.1 is the standard ipV4 "All Hosts" multicast. Probably no reason to change this.
*/
IPAddress multicastAddress(224, 0, 0, 1);

/*
   Whether or not to transmit a probe's temp when it could not be read. (Will send null in that case if true)
*/
bool transmitOnReadError = true;

//==========================================================================================================
//===========================================  End User Options  ===========================================
//==========================================================================================================

//File/Global Scope Variables
//This is mostly runtime mechanics, and should not be altered

IPAddress serverAddress; //Will be populated with the server address when it responds to our SYN with an ACK
WiFiUDP udp; //Initialize udp object with which to send and receive packets

char packetBuffer[UDP_TX_PACKET_MAX_SIZE];  //buffer to hold incoming packet,
unsigned long sendCount; //This supplies datagrams with the Epoch value, i.e. a means of enforcing packet ordering, since RDP (UDP) is a "best effort" protocol.

//Safer to figure out the count here, rather than force congruency on user to match their probe definitions
//This is safe as long as it occurs in the global scope
int probeCount = sizeof(probes) / sizeof(Probe);

//Initialize our timers - one for handshake SYN datagrams, and another for temp datagrams
os_timer_t tempTimer;
os_timer_t syncTimer;

bool shouldSendTemps = false;
bool shouldSendSync = true;

typedef enum {
  hostJoiningWiFi = 0,
  hostSearchingForServer,
  hostConnectedToServer,
} HostState;

HostState hostState = hostJoiningWiFi;

//RDP Keys
#define RDPKey_Version "RPVersion"
#define RDPKey_Serial "RPSerial"
#define RDPKey_Epoch "RPEpoch"
#define RDPKey_Payload "RPPayload"
#define RDPKey_EventType "RPEventType"
#define RDPKey_Channel "RPChannel"
#define RDPKey_Value "RPValue"
#define RDPKey_Meta "RPMetaType"

//RDP String Constants
#define RDPValue_Version_1_0 "RDP_1.0"

//RDP Event Type Integer Constants
typedef enum {
    RDPEventType_SYN = 1,
    RDPEventType_ACK,
    RDPEventType_Temperature,
    RDPEventType_Control,
    RDPEventType_Pressure,
    RDPEventType_Remote,
    //Future Values
} RDPEventType;

//RDP Meta Type Integer Constants
typedef enum {

    //Temperature meta constants
    //Valid with event type RDPEventType_Temperature
    RDPMetaType_BTTemp = 1000,
    RDPMetaType_ETTemp,
    RDPMetaType_METTemp,
    RDPMetaType_HeatBoxTemp,
    RDPMetaType_ExhaustTemp,
    RDPMetaType_AmbientTemp,
    RDPMetaType_BTCoolingTemp,
    //Future Values up to and including 1999

    //The constants for remaining types are unpublished for now :(

} RDPMetaType;


#if usingMax31850Amp
//TODO: #include 31850 library
#endif
#if usingMax31855Amp
#include "Adafruit_MAX31855.h"
#endif

#if (boardType == featherHuzzahSBC)
#include <ESP8266WiFi.h>
#elif (boardType == raspberryPiSBC)
//TODO: Import Raspberry stuff
#endif

void setup() {

  // setup() is called at boot or reset, and runs only once, immediately before loop() is begun
  // Perform 1 time board setup, setup/initialization of file scope variables, and other preparations

  // Define the baud rate of the board from our var
  Serial.begin(baudRate);
  delay(100);

#ifdef LOGGING
  Serial.println(F("Board booted."));
#endif

  //Give our probe structures starting values, and create the probe handles we'll use for reading.
  //We could have done all of this at declaration, but the code would be ugly and hard-to-understand for some folks.
  //So, we let the magic of Compound Literals create the basics of our structs, giving the user an easy-to-grasp syntax
  //And abstract away the harder or redundant logic here…
  for (int i = 0; i < probeCount; i++) {

#ifdef LOGGING
    Serial.print(F("Initializing probe on channel "));
    Serial.print(probes[i].serverChannel);
    Serial.print(F(", CLK:"));
    Serial.print(probes[i].pinCLK);
    Serial.print(F(", CS:"));
    Serial.print(probes[i].pinCS);
    Serial.print(F(", DO:"));
    Serial.println(probes[i].pinDO);
#endif

    probes[i].temp = DBL_MAX; //DBL_MAX signifies missing reading
    probes[i].readCount = 0;
    probes[i].readError = false;

    switch (probes[i].ampType) {

      case max31850AmpType:

        probes[i].probeHandle = NULL;
        //TODO: Replace NULL with actual 31850 Amp handle
        break;

      case max31855AmpType:

        probes[i].probeHandle = new Adafruit_MAX31855(probes[i].pinCLK, probes[i].pinCS, probes[i].pinDO);
        break;

      default:

        probes[i].probeHandle = NULL;
        break;
    }
  }

#ifdef LOGGING
  Serial.print(F("Initialized "));
  Serial.print(probeCount);
  Serial.println(F(" probe(s)."));
#endif

#ifdef STATUS_LED
  pinMode(STATUS_LED, OUTPUT);
#endif
#ifdef ERROR_LED
  pinMode(ERROR_LED, OUTPUT);
#endif

  // Initiate a WiFi connection from our SSID and Password vars
  WiFi.begin(wifiSSID, wifiPassword);

#ifdef LOGGING
  Serial.print(F("Attempting to connect to SSID "));
  Serial.print(wifiSSID);
#endif

  while (WiFi.status() != WL_CONNECTED) {
    // Loop while we're waiting for a WiFi connection
#ifdef STATUS_LED
    blinkLED(STATUS_LED, 3, 80, 40);
#endif
#ifdef LOGGING
    Serial.print(".");
#endif
    delay(500);
  }

#ifdef LOGGING
  Serial.print(F("\nWiFiconnected with IP Address: "));
  Serial.print(WiFi.localIP());
  Serial.print(F(" Mask: "));
  Serial.println(WiFi.subnetMask());
#endif

  // We have successfully connected to WiFi, so set our state to clientSynching
  // This controls what timers are active, and what send/receive actions take place (SYN, ACK or Temps)
  hostState = hostSearchingForServer;
  sendCount = 0;

  //We need listening for ACK packets from the Roastmaster server object, so start our UDP object
  udp.begin(serverPort);

  //Link timer function callbacks to our timer objects
  os_timer_setfn(&syncTimer, syncTimerCallback, NULL);
  os_timer_setfn(&tempTimer, tempTimerCallback, NULL);

  //Arm our syncTimer – this will fire, transmitting SYN packets until the server sends us a reply.
  //Once we receive a reply, we'll note the server's IP address, then switch to temp sending
  os_timer_arm(&syncTimer, (syncSendRate * 1000), true);

}

void loop() {

  // After the initial setup(), loop() is called continuously while unit is powered on

  if (WiFi.status() == WL_CONNECTED) {

    switch (hostState) {

      case hostSearchingForServer:   //We're waiting on an ACK packet from Server (Roastmaster)

        if (readACK()) {   //Check for an ACK packet on our UDP object. True denotes a valid RDP ACK packet

#ifdef LOGGING
          Serial.println(F("Beginning temperature transmission."));
#endif

          hostState = hostConnectedToServer;
          os_timer_disarm(&syncTimer); //
          os_timer_arm(&tempTimer, (tempSendRate * 1000), true); //
          shouldSendTemps = true; //Send once right away

        } else {   //There's no ACK packet waiting for us, so send SYN packet. When Roastmaster receives a SYN packet, it will reply back with an ACK

          if (shouldSendSync) {
            sendSYN();
            shouldSendSync = false;
          }
        }

        break;

      case hostConnectedToServer:   //We are in an Acknowledged state with our server (we've receive an ACK packet at some point in the past)

        if (shouldSendTemps) {   //Our sendTemps timer has fired, so it's time to send some temp data

#ifdef STATUS_LED
          //Turn on the LED while doing the work
          digitalWrite(STATUS_LED, HIGH);
#endif
          readProbes(); //Reads the temp for each defined probe, storing it in our probes[] array of structs
          sendProbesDatagram(); //Sends the temp from each probes[] struct with its corresponding channel, to our server

#ifdef STATUS_LED
          //Turn the LED back off
          digitalWrite(STATUS_LED, LOW);
#endif

          //Reset our flag, so we throttle our packet sending to the timer's interval
          shouldSendTemps = false;
        }
        break;

    }

  } else {   //Wifi is not connected, so turn the error LED on

#ifdef STATUS_LED
    blinkLED(STATUS_LED, 3, 80, 40); //Signalling wifi searching
#endif
  }
}

// call back functions kept as simple as possible...
void tempTimerCallback(void *pArg) {
  shouldSendTemps = true;
}

void syncTimerCallback(void *pArg) {
  shouldSendSync = true;
}

bool readACK() {

  // If there's data available on our UDP port, read the packet
  int packetSize = udp.parsePacket();
  if (packetSize) {

    serverAddress = udp.remoteIP();

    //Read the packet into packetBufffer
    udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);

#ifdef LOGGING
    Serial.print(F("Received packet of size "));
    Serial.print(packetSize);
    Serial.print(F(" from IP Address: "));
    Serial.print(udp.remoteIP());
    Serial.print(F(" Port: "));
    Serial.print(udp.remotePort());
    Serial.print(F(": "));
    Serial.println(packetBuffer);
#endif

    //Once we've read the packet, we need to pilfer around in the JSON a bit.
    //This is a rudimentary solution. We could emply a JSON library, but doesn't seem worth the overhead.
    //It's just as efficient (probably moreso) to simply look for the correct Keys, and the expected Values
    //When we locate a Key, we check to make sure we have an expected Value that FOLLOWS the offset of the key, trickly down the chain until we're satisfied.

    String packetString(packetBuffer);

    String key, value; //The key and value we're hunting for
    int iKey, iValue; //The index position of the found key and value
    int expectedIndex; //We want to make sure the value we're matching IMMEDIATELY follows the key we just matched (or +1 in case user includes spaces)

    //Find the RDP Version
    key = String("\"" + String(RDPKey_Version) + "\"");
    iKey = packetString.indexOf(key);
    if (iKey >= 0) {
      value = RDPValue_Version_1_0;
      expectedIndex = (iKey + key.length() + 1); //Account for colon
      iValue = packetString.indexOf(value, iKey + key.length()); //Search starting at key offset + key length (i.e. after)
      if (iValue == expectedIndex || iValue == (expectedIndex + 1)) { //+1 in case of space

        //Find the RDP Serial Number
        key = String("\"" + String(RDPKey_Serial) + "\"");
        iKey = packetString.indexOf(key);
        if (iKey >= 0) {
          value = serverSerial;
          expectedIndex = (iKey + key.length() + 1); //Account for colon
          iValue = packetString.indexOf(value, iKey + key.length()); //Search starting at key offset + key length (i.e. after)
          if (iValue == expectedIndex || iValue == (expectedIndex + 1)) { //+1 in case of space

            //Find the Handshake ACK
            key = String("\"" + String(RDPKey_EventType) + "\"");
            iKey = packetString.indexOf(key);
            if (iKey >= 0) {
              value = String(RDPEventType_ACK);
              expectedIndex = (iKey + key.length() + 1); //Account for colon
              iValue = packetString.indexOf(value, expectedIndex); //Search starting at key offset + key length (i.e. after)
              if (iValue == expectedIndex || iValue == (expectedIndex + 1)) { //+1 in case of space

#ifdef LOGGING
                Serial.println(F("Packet was an ACK packet from server."));
#endif
                //We signal success by returning true

                return true;
              }
            }
          }
        }
      }
    }
  }
  return false;
}

String JSONStringForDictionaryWithStringValue(String key, String value, bool valueQuotes, bool braces) {
  //Helper function for a String value, to make JSON construction easier to understand
  String keyValueString;
  if (valueQuotes) {
    keyValueString = "\"" + key + "\":\"" + value + "\"";
  } else {
    keyValueString = "\"" + key + "\":" + value + "";
  }
  if (braces) {
    keyValueString = "{" + keyValueString + "}";
  }
  return keyValueString;
}

String JSONStringForDictionaryWithDoubleValue(String key, double value, bool braces) {
  //Helper function for a double value, to make JSON construction easier to understand
  String keyValueString = "\"" + key + "\":" + String(value);
  if (braces) {
    keyValueString = "{" + keyValueString + "}";
  }
  return keyValueString;
}

String JSONStringForDictionaryWithIntValue(String key, int value, bool braces) {
  //Helper function for an int value, to make JSON construction easier to understand
  String keyValueString = "\"" + key + "\":" + String(value);
  if (braces) {
    keyValueString = "{" + keyValueString + "}";
  }
  return keyValueString;
}

void sendSYN() {

  String payloadArrayJSON = JSONStringForDictionaryWithIntValue(RDPKey_EventType, RDPEventType_SYN, true);
  payloadArrayJSON = "[" + payloadArrayJSON + "]";

  String datagramDictionaryJSON = "{" +
                                  JSONStringForDictionaryWithStringValue(RDPKey_Version, RDPValue_Version_1_0, true, false) + "," +
                                  JSONStringForDictionaryWithStringValue(RDPKey_Serial, String(serverSerial), true, false) + "," +
                                  JSONStringForDictionaryWithIntValue(RDPKey_Epoch, sendCount, false) + "," +
                                  JSONStringForDictionaryWithStringValue(RDPKey_Payload, payloadArrayJSON, false, false) +
                                  "}";

  byte datagramBytes[datagramDictionaryJSON.length() + 1];
  datagramDictionaryJSON.getBytes(datagramBytes, datagramDictionaryJSON.length() + 1);

#ifdef LOGGING
  Serial.print(F("Transmitting SYN datagram to IP: "));
  Serial.print(multicastAddress);
  Serial.print(F(", Port: "));
  Serial.print(serverPort);
  Serial.print(F(", JSON: "));
  Serial.println(datagramDictionaryJSON);
#endif

  //udp.beginPacket(multicastAddress, serverPort);
  udp.beginPacketMulticast(multicastAddress, serverPort, WiFi.localIP());
  udp.write(datagramBytes, datagramDictionaryJSON.length() + 0);
  udp.endPacket();

  sendCount++;

#ifdef STATUS_LED
  blinkLED(STATUS_LED, 2, 80, 40);
#endif

}

void readProbes() {

  bool anyProbeError = false;

  for (int i = 0; i < probeCount; i++) {

    bool thisProbeError = false;

    double coldJunctionTemp = NAN; //Temp of the TC wires at the board junction
    double hotJunctionTemp = NAN; //Temp of the TC wires at the probe tip junction
    double linearizedTemp = NAN; //Linearization of cold and hot temps give the actual temperature of the temp

    switch (probes[i].ampType) {

      case max31850AmpType:

        //TODO: Get the probe handle and cold and hot readings from library

        break;

      case max31855AmpType: {

          Adafruit_MAX31855 *probeHandle = (Adafruit_MAX31855*)probes[i].probeHandle;

          coldJunctionTemp = probeHandle->readInternal();
          hotJunctionTemp = probeHandle->readCelsius();

        }

        break;

      default:

        // Error in user setup config

        break;
    }

    //If cold or hot junctions temps are NAN (invalid), either from bad readings or were never taken due to user settings logic error, we flag and alert

    if (isnan(coldJunctionTemp)) {
      thisProbeError = true;
      Serial.print(F("ERROR reading Probe Cold Junction Temp on Channel "));
      Serial.print(probes[i].serverChannel);
      Serial.println(F("."));
    }
    if (isnan(hotJunctionTemp)) {
      thisProbeError = true;
      Serial.print(F("ERROR reading Probe Hot Junction Temp on Channel "));
      Serial.print(probes[i].serverChannel);
      Serial.println(F("."));
    }

    if (!thisProbeError) {

      //We know cold and hot junction temps are good, so we try and linearize

      linearizedTemp = lineariseTemperature(coldJunctionTemp, hotJunctionTemp);

      if (isnan(linearizedTemp)) {

        thisProbeError = true;

        Serial.print(F("ERROR reading Probe Tip Temp on Channel "));
        Serial.print(probes[i].serverChannel);
        Serial.println(F("."));
      }
    }//Else, we've already handled reporting

    probes[i].readError = thisProbeError;
    probes[i].temp = isnan(linearizedTemp) ? DBL_MAX : linearizedTemp; //DBL_MAX signals missing temp
    probes[i].readCount = probes[i].readCount + 1; //We don't do anything with this at the moment. Maybe we should only increment on successful reads? Depends on future usage I guess.

    if (thisProbeError == true) {
      anyProbeError = true; //Only if true–we are aggregating
    }
  }

#ifdef ERROR_LED
  if (anyProbeError) {
    digitalWrite(ERROR_LED, HIGH); // turn on the error indication LED
  } else {
    digitalWrite(ERROR_LED, LOW); // turn off the error indication LED
  }
#endif
}

void sendProbesDatagram() {

  String payloadArrayJSON = "";

  for (int i = 0; i < probeCount; i++) {

    if (payloadArrayJSON.length() > 0) {
      //Append a comma to delineate the new event
      payloadArrayJSON = payloadArrayJSON + ",";
    }

    if (probes[i].readError == false || transmitOnReadError) {

      //We manually parse our temp double to a string, so that we can pass null JSON-style
      //Roastmaster will parse null first to [NSNull null], then to nil, and register a "missing" temp
      String tempString = probes[i].temp == DBL_MAX ? "null" : String(probes[i].temp, 2);

      //Assemble the JSON string as usual
      String thisProbeString = "{" +
                               JSONStringForDictionaryWithIntValue(RDPKey_EventType, RDPEventType_Temperature, false) + "," +
                               JSONStringForDictionaryWithIntValue(RDPKey_Channel, probes[i].serverChannel, false) + "," +
                               JSONStringForDictionaryWithStringValue(RDPKey_Value, tempString, false, false) +
                               "}";
      payloadArrayJSON = payloadArrayJSON + thisProbeString;
    }
  }

  if (payloadArrayJSON.length() == 0) {
    return;
  }

  payloadArrayJSON = "[" + payloadArrayJSON + "]";

  String datagramDictionaryJSON = "{" +
                                  JSONStringForDictionaryWithStringValue(RDPKey_Version, RDPValue_Version_1_0, true, false) + "," +
                                  JSONStringForDictionaryWithStringValue(RDPKey_Serial, String(serverSerial), true, false) + "," +
                                  JSONStringForDictionaryWithIntValue(RDPKey_Epoch, sendCount, false) + "," +
                                  JSONStringForDictionaryWithStringValue(RDPKey_Payload, payloadArrayJSON, false, false) +
                                  "}";
  byte datagramBytes[datagramDictionaryJSON.length() + 1];
  datagramDictionaryJSON.getBytes(datagramBytes, datagramDictionaryJSON.length() + 1);

#ifdef LOGGING
  Serial.print(F("Transmitting Temps datagram to IP: "));
  Serial.print(serverAddress);
  Serial.print(F(", Port: "));
  Serial.print(serverPort);
  Serial.print(F(", JSON: "));
  Serial.println(datagramDictionaryJSON);
#endif

  //udp.beginPacket(multicastAddress, serverPort);
  udp.beginPacket(serverAddress, serverPort);
  udp.write(datagramBytes, datagramDictionaryJSON.length() + 0);
  udp.endPacket();

  sendCount++;

}

double lineariseTemperature(double coldJunctionCelsiusTemperatureReading, double externalCelsiusTemperatureReading) {

  /*
     Function pinched directly from the Adafruit forum http://forums.adafruit.com/viewtopic.php?f=19&t=32086
     Note: there are corrections applied from the forum posts, and this has been tweaked to remove some
     checks made elsewhere and take parameters and provide a return value:

     - internalTemp is the coldJunctionCelsiusTemperatureReading parameter
     - rawTemp is the externalCelsiusTemperatureReading parameter
     - the linearised value is returned, to convert to fahrenheit do a basic (celsius*(9.0/5.0))+32.0 conversion

  */
  // Initialize variables.
  int i = 0; // Counter for arrays
  double thermocoupleVoltage = 0;
  double internalVoltage = 0;
  double correctedTemperature = 0;

  // Steps 1 & 2. Subtract cold junction temperature from the raw thermocouple temperature.
  thermocoupleVoltage = (externalCelsiusTemperatureReading - coldJunctionCelsiusTemperatureReading) * 0.041276; // C * mv/C = mV
  //  #ifdef (LOGGING)
  //    Serial.println();
  //    Serial.print(F("Thermocouple Voltage calculated as "));
  //    Serial.println(String(thermocoupleVoltage, 5));
  //  #endif

  // Step 3. Calculate the cold junction equivalent thermocouple voltage.
  if (coldJunctionCelsiusTemperatureReading >= 0) { // For positive temperatures use appropriate NIST coefficients
    // Coefficients and equations available from http://srdata.nist.gov/its90/download/type_k.tab
    double c[] = { -0.176004136860E-01,  0.389212049750E-01,  0.185587700320E-04, -0.994575928740E-07,  0.318409457190E-09, -0.560728448890E-12,  0.560750590590E-15, -0.320207200030E-18,  0.971511471520E-22, -0.121047212750E-25};

    // Count the the number of coefficients. There are 10 coefficients for positive temperatures (plus three exponential coefficients),
    // but there are 11 coefficients for negative temperatures.
    int cLength = sizeof(c) / sizeof(c[0]);

    // Exponential coefficients. Only used for positive temperatures.
    double a0 =  0.118597600000E+00;
    double a1 = -0.118343200000E-03;
    double a2 =  0.126968600000E+03;

    // From NIST: E = sum(i=0 to n) c_i t^i + a0 exp(a1 (t - a2)^2), where E is the thermocouple voltage in mV and t is the temperature in degrees C.
    // In this case, E is the cold junction equivalent thermocouple voltage.
    // Alternative form: C0 + C1*internalTemp + C2*internalTemp^2 + C3*internalTemp^3 + ... + C10*internaltemp^10 + A0*e^(A1*(internalTemp - A2)^2)
    // This loop sums up the c_i t^i components.
    for (i = 0; i < cLength; i++) {
      internalVoltage += c[i] * pow(coldJunctionCelsiusTemperatureReading, i);
    }

    // This section adds the a0 exp(a1 (t - a2)^2) components.
    internalVoltage += a0 * exp(a1 * pow((coldJunctionCelsiusTemperatureReading - a2), 2));

  } else if (coldJunctionCelsiusTemperatureReading < 0) {
    // for negative temperatures
    double c[] = {0.000000000000E+00,  0.394501280250E-01,  0.236223735980E-04, -0.328589067840E-06, -0.499048287770E-08, -0.675090591730E-10, -0.574103274280E-12, -0.310888728940E-14, -0.104516093650E-16, -0.198892668780E-19, -0.163226974860E-22};

    // Count the number of coefficients.
    int cLength = sizeof(c) / sizeof(c[0]);

    // Below 0 degrees Celsius, the NIST formula is simpler and has no exponential components: E = sum(i=0 to n) c_i t^i
    for (i = 0; i < cLength; i++) {
      internalVoltage += c[i] * pow(coldJunctionCelsiusTemperatureReading, i) ;
    }
  }
  //  #ifdef (LOGGING)
  //    Serial.print(F("Internal Voltage calculated as "));
  //    Serial.println(String(internalVoltage, 5));
  //  #endif

  // Step 4. Add the cold junction equivalent thermocouple voltage calculated in step 3 to the thermocouple voltage calculated in step 2.
  double totalVoltage = thermocoupleVoltage + internalVoltage;

  // Step 5. Use the result of step 4 and the NIST voltage-to-temperature (inverse) coefficients to calculate the cold junction compensated, linearized temperature value.
  // The equation is in the form correctedTemp = d_0 + d_1*E + d_2*E^2 + ... + d_n*E^n, where E is the totalVoltage in mV and correctedTemp is in degrees C.
  // NIST uses different coefficients for different temperature subranges: (-200 to 0C), (0 to 500C) and (500 to 1372C).
  if (totalVoltage < 0) { // Temperature is between -200 and 0C.
    double d[] = {0.0000000E+00, 2.5173462E+01, -1.1662878E+00, -1.0833638E+00, -8.9773540E-01, -3.7342377E-01, -8.6632643E-02, -1.0450598E-02, -5.1920577E-04, 0.0000000E+00};

    int dLength = sizeof(d) / sizeof(d[0]);
    for (i = 0; i < dLength; i++) {
      correctedTemperature += d[i] * pow(totalVoltage, i);
    }
  }
  else if (totalVoltage < 20.644) {
    // Temperature is between 0C and 500C.
    double d[] = {0.000000E+00, 2.508355E+01, 7.860106E-02, -2.503131E-01, 8.315270E-02, -1.228034E-02, 9.804036E-04, -4.413030E-05, 1.057734E-06, -1.052755E-08};
    int dLength = sizeof(d) / sizeof(d[0]);
    for (i = 0; i < dLength; i++) {
      correctedTemperature += d[i] * pow(totalVoltage, i);
    }
  }
  else if (totalVoltage < 54.886 ) {
    // Temperature is between 500C and 1372C.
    double d[] = { -1.318058E+02, 4.830222E+01, -1.646031E+00, 5.464731E-02, -9.650715E-04, 8.802193E-06, -3.110810E-08, 0.000000E+00, 0.000000E+00, 0.000000E+00};
    int dLength = sizeof(d) / sizeof(d[0]);
    for (i = 0; i < dLength; i++) {
      correctedTemperature += d[i] * pow(totalVoltage, i);
    }
  } else {
    // NIST only has data for K-type thermocouples from -200C to +1372C. If the temperature is not in that range, set temp to impossible value.
#ifdef LOGGING
    Serial.println(F("Temperature is out of range, this should never happen!"));
#endif
    correctedTemperature = NAN;
  }

  //  #ifdef (LOGGING)
  //    Serial.print(F("Corrected temperature calculated to be: "));
  //    Serial.print(correctedTemperature, 5);
  //    Serial.println(F("°C"));
  //    Serial.println();
  //  #endif
  return correctedTemperature;
}

String stringFromIPAddress(IPAddress address) {
  return String(address[0]) + "." +
         String(address[1]) + "." +
         String(address[2]) + "." +
         String(address[3]);
}

#if defined(STATUS_LED) || defined(ERROR_LED)
// We try to limit blinking to chunks of code that benefit from a pause, since delay() stops most board functions–not just our own code.
void blinkLED(int ledToBlink, int blinkCount, int blinkOnDuration, int blinkOffDuration) {
  do {
    digitalWrite(ledToBlink, HIGH);
    delay(blinkOnDuration);
    digitalWrite(ledToBlink, LOW);
    delay(blinkOffDuration);
  } while (--blinkCount > 0);
}
#endif
