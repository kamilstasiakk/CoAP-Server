/*
  CoapServer.ino
  Created by Krzysztof Kossowski, 2017.
  Implements only selected parts of the CoAP protocol.

  Including: 
  - wireless connection via nRF24L01 module with OBJECT;
  - wired connection via ETHERNET W5100 with INTERNET via router; 
*/

// include traditionall librares;
#include <SPI.h>

#include <RF24Network.h> 
#include <RF24.h> 

#include <Ethernet.h>
#include <EthernetUdp.h>

// include our librares; 
#include CoapParser_h

// constant variables neccessary for RF24 wireless connection;
const uint16_t THIS_NODE_ID = 1;
const uint16_t REMOTE_NODE_ID = 0;
const uint8_t RF_CHANNEL = 60;

// variable connected with wireless object
RF24 radio(7,8);  // pin CE=7, CSN=8
RF24Network network(radio);
const uint8_t LAMP = 0;
const uint8_t BUTTON = 1;
uint8_t lampState;  //0 - ON, 1 - OFF
uint32_t buttonState; // incomming time marker  


// variable connected with wired connection
EthernetUDP Udp;
byte mac[] = {00,0xaa,0xbb,0xcc,0xde,0xf3};
short localPort=1237;
const uint8_t MAX_BUFFER = 100; //do zastanowienia
char ethMessage[MAX_BUFFER];

void setup() {  
  initializeRF24Communication();
  initializeEthernetCommunication();
}

void loop() {
  // checking the network object regularly;
  network.update();
  
}



// RF24 Methodes--------------------------------
// initiation of wireless variables and network connections
void initializeRF24Communication(){
  SPI.begin();
  radio.begin();
  network.begin(RF_CHANNEL, THIS_NODE_ID); 
}

void receiveRF24Message() {
  while ( network.available() )
  {
    RF24NetworkHeader header;
    byte rf24Message;
    network.read(header, rf24Message, sizeof(rf24Message));

    processRF24Message(byte rf24Message);
  }
}
void processRF24Message(byte message){ 
    if ( ((rf24Message & 0x02) >> 1) == LAMP ){
      lampState = (rf24Message & 0x01);
    }
    else {
      buttonState = millis();
    }
}
void sendRF24Message(byte message) {
    RF24NetworkHeader header;
    network.write(header, &message, sizeof(message));
}
// END RF23_Methodes----------------------------

// Ethernet_Methodes----------------------------
void initializeEthernetCommunication(){
  Ethernet.begin(mac);
  Udp.begin(localPort);
}
void receiveEthernetMessage() {
  int packetSize = Udp.parsePacket(); //the size of a received UDP packet, 0 oznacza nieodebranie pakietu
  if(packetSize) {
    if(packetSize >= 4) {
      Udp.read(ethMessage, MAX_BUFFER) //do zastanowienia 
      processEthernetMessage();  
    }
}
void processEthernetMessage(){ 
}
void sendEthernetMessage(byte message){
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
}
// END Ethernet_Methodes------------------------

