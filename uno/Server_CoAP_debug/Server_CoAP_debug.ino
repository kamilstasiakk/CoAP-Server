/*
  CoapServer.ino
  Created by Krzysztof Kossowski, 2017.
  Implements only selected parts of the CoAP protocol.

  Including: 
  - wireless connection via nRF24L01 module with OBJECT;
  - wired connection via ETHERNET with INTERNET via router; 
*/

// include traditionall librares;
#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h>

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
uint8_t lampState = 3;  //0 - ON, 1 - OFF
uint32_t buttonState = 0; // incomming time marker  


void setup() {  
  Serial.begin(9600);
  
  initializeWirelessCommunication(); 
}

void loop() {
  
  // checking the network object regularly;
  network.update();
  // if there is a message for us, process it;
  processWirelessMessage();

}



// RF24 Methodes--------------------------------
// initiation of wireless variables and network connections
void initializeWirelessCommunication(){
  SPI.begin();
  radio.begin();
  network.begin(RF_CHANNEL, THIS_NODE_ID); 
}

// processing RF24_message
void processWirelessMessage(){ 
  while ( network.available() )
  {
    // get rf24Message
    RF24NetworkHeader header;
    byte rf24Message;
    network.read(header, rf24Message, sizeof(rf24Message));

    // process rf24Message
    if ( ((rf24Message & 0x02) >> 1) == LAMP ){
      lampState = (rf24Message & 0x01);
    }
    else {
      buttonState = millis();
    }

    // debugging
      // debugging
    Serial.println(rf24Message);
    Serial.println("Stan lampki" + lampState);
    Serial.println("Stan guzika" + buttonState);
  } 
}
// END RF23_Methodes----------------------------

// Ethernet_Methodes----------------------------
// END Ethernet_Methodes------------------------

