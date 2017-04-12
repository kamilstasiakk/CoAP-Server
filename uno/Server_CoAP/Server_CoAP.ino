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
short localPort = 1237;
const uint8_t MAX_BUFFER = 100; //do zastanowienia
char ethMessage[MAX_BUFFER];

// other variables:
Resource resources[RESOURCES_COUNT];
Session sessions[MAX_SESSIONS_COUNT];
Etag etags[20];
Observator observators[5];
uint32_t observCounter;
Parser parser = Parser();
Builder builder = Builder();
uint16_t messageId;

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
  resources[0].flags = B00000010;

  // przycisk
  resources[1].uri = "/sensor/btn";
  resources[1].rt = "Button";
  resources[1].if = "state";
  resources[1].value = 0;
  resources[1].flags = B00000101;

  // metryka PacketLossRate
  resources[0].uri = "/metric/PLR";
  resources[0].rt = "PacketLossRate";
  resources[0].if = "value";
  resources[0].value = 0; //OFF
  resources[0].flags = B00000000;

  // metryka ByteLossRate
  resources[0].uri = "/metric/BLR";
  resources[0].rt = "ByteLossRate";
  resources[0].if = "value";
  resources[0].value = 0;
  resources[0].flags = B00000000;

  // metryka MeanAckDelay
  resources[0].uri = "/metric/MAD";
  resources[0].rt = "Lamp";
  resources[0].if = "value";
  resources[0].value = 0;
  resources[0].flags = B00000000;
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
    if ( ((rf24Message & 0x02) >> 1) == LAMP ){
      lampState = (rf24Message & 0x01);
    }
    else {
      buttonState = millis();
    }

    // jeżeli obiekt ma możliwośc bycia obserwowanym, to uruchom procedurę ObservationProcess()
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
  Ethernet.begin(mac);
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
      if (parser.parseVersion(ethMessage) !=1 || parser.parseCodeClass(ethMessage) != CLASS_REQ)
        sendErrorMessage(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
      switch (parser.parseCodeDetail(ethMessage)) {
        case DETAIL_EMPTY:
          receiveEmptyRequest(ethMessage);
          break;
        case DETAIL_GET:
          receiveGetRequest(ethMessage);
          break;
        case DETAIL_POST:
          receivePostRequest(ethMessage);
          break;
        default:
          sendErrorMessage(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
          break;
      }
    }
}

void sendErrorMessage(IPAddress ip, uint16_t portNumber, char* message, uint8_t errorType) {
  
}
void receiveGetRequest(char* message, IPAddress ip, uint16_t portNumber) {
  
}

void receiveEmptyRequest(char* message, IPAddress ip, uint16_t portNumber) {
  
}

void receivePostRequest(char* message, IPAddress ip, uint16_t portNumber) {
  uint8_t optionNumber = parser.getFirstOption(message);
  if (firstOption != URI_PATH) {
    if (firstOption > URI_PATH) {
      sendErrorMessage(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
      return;
    } else {
      while (optionNumber != URI_PATH)  {
        //nie ma uri! BLAD
        if (optionNumber > URI_PATH || optionNumber == NO_OPTION){
          sendErrorMessage(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
          return;
        }
        optionNumber = parser.getNextOption(message);
      }
    }
  }
  //sparsowaliśmy uri 
  for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
    if (strcmp(resources[resourceNumber].uri, parser.fieldValue) == 0) {
      if((resources[resourceNumber].flags & 0x02) == 2)
      {
        for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
          if(sessions[sessionNumber].contentFormat > 127) {
            sessions[sessionNumber].ipAddress = ip;
            sessions[sessionNumber].portNumber = portNumber;
            optionNumber = parser.getNextOption(message);
            while (optionNumber != CONTENT-FORMAT)  {
              //nie ma content-format! BLAD
              if (optionNumber > CONTENT-FORMAT || optionNumber == NO_OPTION){
                sendErrorMessage(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
                return;
              }
              optionNumber = parser.getNextOption(message);
            }// jak wyszlismy z petli to znaczy ze znalezlismy conntent format
            if(strlen(parser.fieldValue) > 0) {
              if (parser.fieldValue[0] == PLAIN_TEXT) {
                sendMessageToThing((resources[resourceNumber].flags & 0x06) >>2,
                                    parser.parsePayload(message)); 
              }
            } else {
              sendErrorMessage(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
            }
          }
        }

      }
    }
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
