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
#include <CoapParser_h>
#include <Resource.h>

// constant variables neccessary for RF24 wireless connection;
const uint16_t THIS_NODE_ID = 1;
const uint16_t REMOTE_NODE_ID = 0;
const uint8_t RF_CHANNEL = 60;

// variable connected with wireless object
RF24 radio(7,8);  // pin CE=7, CSN=8
RF24Network network(radio);

// variable connected with wired connection
EthernetUDP Udp;
byte mac[] = {00,0xaa,0xbb,0xcc,0xde,0xf3};
IPAddress ip(192, 168, 1, 250);
short localPort = 1237;
const uint8_t MAX_BUFFER = 100; //do zastanowienia
char ethMessage[MAX_BUFFER];

// other variables:
Resource resources[5];
Etag etags[20];
Observator observators[5];
uint32_t observCounter;

void setup() {
  SPI.begin();
  initializeRF24Communication();
  initializeEthernetCommunication();

  initializeResourceList();
}

void loop() {
  // checking the network object regularly;
  network.update();
  receiveRF24Message();
}



// Resources:Methodes---------------------------
void initializeResourceList() {
  // lampka
  resources[0].uri = "/sensor/lamp";
  resources[0].rt = "Lamp";
  resources[0].if = "state";
  resources[0].value = 0; //OFF
  resources[0].flags = B00000000;

  // przycisk
  resources[1].uri = "/sensor/btn";
  resources[1].rt = "Button";
  resources[1].if = "state";
  resources[1].value = 0;
  resources[1].flags = B00000011;

  // metryka PacketLossRate
  resources[2].uri = "/metric/PLR";
  resources[2].rt = "PacketLossRate";
  resources[2].if = "value";
  resources[2].value = 0; //OFF
  resources[2].flags = B00000000;

  // metryka ByteLossRate
  resources[3].uri = "/metric/BLR";
  resources[3].rt = "ByteLossRate";
  resources[3].if = "value";
  resources[3].value = 0;
  resources[3].flags = B00000000;

  // metryka MeanAckDelay
  resources[4].uri = "/metric/MAD";
  resources[4].rt = "Lamp";
  resources[4].if = "value";
  resources[4].value = 0;
  resources[4].flags = B00000000;
}
// End:Resources--------------------------------


// RF24:Methodes--------------------------------
/* 
 *  Metoda odpowiedzialna za inicjalizację parametrów interfejsu radiowego:
 *  - przypisujemy identyfikator interfejsu radiowego oraz numer kanału, na którym będzie pracował; 
*/
void initializeRF24Communication(){
  radio.begin();
  network.begin(RF_CHANNEL, THIS_NODE_ID); 
}

/* 
 *  Metoda odpowiedzialna za odebranie danych dopóki są one dostepne na interfejsie radiowym.
*/
void receiveRF24Message() {
  while ( network.available() )
  {
    RF24NetworkHeader header;
    byte rf24Message;
    network.read(header, rf24Message, sizeof(rf24Message));

    processRF24Message(byte rf24Message);
  }
}

/* 
 *  Metoda odpowiedzialna za przetworzenie odebranych danych o statusie zasobu:
 *  - jeżeli dotyczy zasobu lampka, to odczytujemy stan zasobu zgodnie z dokumentacją; 
 *  - jeżeli dotyczy zasobu przycisk, to zapisujemy czas przyjścia wiadomości;
*/
void processRF24Message(byte message){ 
    uint8_t resourceID = ((rf24Message & 0x02) >> 1);

    for(int i=0; i<resources.length(); i++) {
      if ( (resources[i].flags & 0x06) >> 1 ) == resourceID ){
           if ( resourceID == 1 ) {
            resources[i].value = (uint16_t)millis();
           }
           else {
            resources[i].value = (rf24Message & 0x01);
           }
      }
    }
}

/* 
 *  Metoda odpowiedzialna za wysyłanie danych interfejsem radiowym.
 *  - parametr message oznacza wiadomość, którą chcemy wysłać;
*/
void sendRF24Message(byte message) {
    RF24NetworkHeader header;
    network.write(header, &message, sizeof(message));
}
// END:RF23_Methodes----------------------------

// START:Ethernet_Methodes----------------------------
/* 
 *  Metoda odpowiedzialna za inicjalizację parametrów interfejsu ethernetowego:
 *  - przypisujemy adres MAC oraz numer portu;
*/
void initializeEthernetCommunication(){
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
  //przypisać jakiś konkretny adres ip
}

/* 
 *  Metoda odpowiedzialna za odbieranie wiadomości z interfejsu ethernetowego
*/
void receiveEthernetMessage() {
  int packetSize = Udp.parsePacket(); //the size of a received UDP packet, 0 oznacza nieodebranie pakietu
  if(packetSize) {
    if(packetSize >= 4) {
      Udp.read(ethMessage, MAX_BUFFER) //do zastanowienia 
      processEthernetMessage();  
    }
}

/* 
 *  
*/
void processEthernetMessage(){ 
}

/* 
 *  Metoda odpowiedzialna za wysyłanie wiadomości poprzez interfejs ethernetowy:
 *  - message jest wiadomością, którą chcemy wysłać
 *  - ip jest adresem ip hosta, do którego adresujemy wiadomość np:IPAddress adres(10,10,10,1)
 *  - port jest numerem portu hosta, do którego adresuemy wiadomość
*/
void sendEthernetMessage(char* message, size_t messageSize, IPAddress ip, uint16_t port){
  Udp.beginPacket(ip), port);
  int r = Udp.write(message, messageSize);
  Udp.endPacket();
  
  //jeżeli liczba danych przyjętych do wysłania przez warstwę niższą jest mniejsza niż rozmiar wiadomości to?
  if ( r < messageSize ) {
      
  }
}
// END:Ethernet_Methodes------------------------


// START:StateMethodes---------------------------
// END:StateMethodes-----------------------------
