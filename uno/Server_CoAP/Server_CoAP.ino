/*
  CoapServer.ino
  Created in 2017 by:
    Krzysztof Kossowski,
    Kamil Stasiak;
    Piotr Kucharski;
    
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

// variables connected with our protocol between Uno and Mini Pro
const uint8_t RESPONSE_TYPE = 0;
const uint8_t GET_TYPE = 1;
const uint8_t POST_TYPE = 2; 

// variable connected with wireless object
RF24 radio(7,8);  // pin CE=7, CSN=8
RF24Network network(radio);

// variable connected with wired connection
EthernetUDP Udp;
byte mac[] = {00,0xaa,0xbb,0xcc,0xde,0xf3};
IPAddress ip = (192,168,1,1);
short localPort = 1237;
const uint8_t MAX_BUFFER = 100; //do zastanowienia
char ethMessage[MAX_BUFFER];

// CoAP variables:
Resource resources[RESOURCES_COUNT];
Session sessions[MAX_SESSIONS_COUNT];
Etag etags[MAX_ETAG_COUNT];

Observator observators[MAX_OBSERVATORS_COUNT];
uint32_t observCounter = 0; //globalny licznik związany z opcją observe

Parser parser = Parser();
Builder builder = Builder();
uint16_t messageId = 0; //globalny licznik - korzystamy z niego, gdy serwer generuje wiaodmość

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
  resources[4].rt = "MeanACKDelay";
  resources[4].if = "value";
  resources[4].value = 0;
  resources[4].flags = B00000000;

  // wysyłamy wiadomości żądające podania aktualnego stanu zapisanych zasobów
  // TO_DO: trzeba tutaj dorobić pętlę, jeżeli zaczyna sie od /sensor/ to wyslij geta
  sendMessageToThing(GET_TYPE, ( (resources[0].flags & 0x1c) >> 2), 0);
  sendMessageToThing(GET_TYPE, ( (resources[1].flags & 0x1c) >> 2), 0);
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
    getMessageFromThing(byte rf24Message);
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
 *  - przypisujemy adres MAC, adres IP oraz numer portu;
*/
void initializeEthernetCommunication(){
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
}
/* 
 *  Metoda odpowiedzialna za odbieranie wiadomości z interfejsu ethernetowego
 *  - jeżeli pakiet ma mniej niż 4 bajty (minimalną wartośc nagłówka) to odrzucamy go;
 *  - jeżeli pakiet ma przynajmniej 4 bajty, zostaje poddany dalszej analizie;
*/
void receiveEthernetMessage() {
  int packetSize = Udp.parsePacket(); //the size of a received UDP packet, 0 oznacza nieodebranie pakietu
  if(packetSize) {
    if(packetSize >= 4) {
      Udp.read(ethMessage, MAX_BUFFER)
      getCoapClienMessage(ethMessage);
    }
  }
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


// START:CoAP_Methodes---------------------------
/* 
 *  Metoda odpowiedzialna za analizę wiadomości CoAP od klienta:
 *  - jeżeli wersja wiadomości jest różna od 01, wyślij błąd BAD_REQUEST;
 *  - jeżeli wersja wiadomości jest zgodna, uruchom odpowiednią metodę zależnie od pola CODE detail;
 *  - obsługiwane są tylko wiadomości CODE = EMPTY, GET lub POST (inaczej wyślij błąd BAD_REQUEST);
 *  - EMPTY: taki kod może miec tylko wiadomośc typu ACK lub RST (inaczej wyślij bład BAD_REQUEST);
 *  - GET, POST: taki kod może miec tylko wiadomość typu CON lub NON (inaczej wyślij błąd BAD_REQUEST);
*/
void getCoapClienMessage(char* message){
  if (parser.parseVersion(message) !=1 || parser.parseCodeClass(message) != CLASS_REQ) {
    sendErrorResponse(udp.remoteIP(), udp.remotePort(), message, BAD_REQUEST);
    return;
  }
  switch (parser.parseCodeDetail(message)) {
    case DETAIL_EMPTY:
      if ( (parser.parseType(message) == TYPE_ACK) || (parser.parseType(message) == TYPE_RST) )
        receiveEmptyRequest(message);
        return;
      else  
        break;
    case DETAIL_GET:
      if ( (parser.parseType(message) == TYPE_CON) || (parser.parseType(message) == TYPE_NON) )
        receiveGetRequest(message);
        return;
      else
        break;
    case DETAIL_POST:
      if ( (parser.parseType(message) == TYPE_CON) || (parser.parseType(message) == TYPE_NON) )
        receivePostRequest(message);
        return;
      else 
        break;
    default:
      sendErrorMessage(udp.remoteIP(), udp.remotePort(), message, BAD_REQUEST);
      return;
  }

  // jeżeli wyszliśmy to znaczy, że był po drodze bład;
  sendErrorMessage(udp.remoteIP(), udp.remotePort(), message, BAD_REQUEST);    
}


void receiveGetRequest(char* message, IPAddress ip, uint16_t portNumber) {
  
}

void receiveEmptyRequest(char* message, IPAddress ip, uint16_t portNumber) {
  
}


/* 
 *  Metoda odpowiedzialna za analizę wiadomości typu POST:
 *  Wiadomość zawiera opcje: URI-PATH, CONTENT-FORMAT oraz payload z nową wartością;
 *  - odczytujemy opcję URI-PATH (jak nie ma, wysyłamy błąd BAD_REQUEST);
 *  - szukamy zasobu na serwerze (jak nie ma, wysyłmy bład NOT_FOUND);
 *  - jeżeli zasób nie ma możliwości zmiany stanu, wtedy wysyłamy bład METHOD_NOT_ALLOWED;
 *  - szukamy wolnej sesji (jeżeli brak, odrzucamy wiadomość zbłędem serwera INTERNAL_SERVER_ERROR;
 *  - odczytujemy format zasobu (jak nie ma, wysyłamy bład BAD_REQUEST);
 *  - jeżeli format zasobu jest inny niż PlainText, Json, XMl to zwróc bład METHOD_NOT_ALLOWED;
 *  - odczytujemy zawartość PAYLOADu (jeśli długość zerowa, wysyłamy bład BAD_REQUEST);
 *  - sprawdzamy wartość pola Type, jeżeli jest to CON to wysyłamy puste ack, jeżeli NON to kontynuujemy;
 *  - wyślij żądanie zmiany stanu zasobu do obiektu IoT;
 *  
 *  -TO_DO: obsługa XML i JSONa
*/
void receivePostRequest(char* message, IPAddress ip, uint16_t portNumber) {
  uint8_t optionNumber = parser.getFirstOption(message);
  if (firstOption != URI_PATH) {
    if (firstOption > URI_PATH) {
      sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
      return;
    } else {
      while (optionNumber != URI_PATH)  {
        //nie ma uri! BLAD
        if (optionNumber > URI_PATH || optionNumber == NO_OPTION){
          sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
          return;
        }
        optionNumber = parser.getNextOption(message);
      } // end of while loop
    } 
  } // end of if (firstOption != URI_PATH)
  
  //sparsowaliśmy uri - szukamy zasobu na serwerze
  for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
    if (strcmp(resources[resourceNumber].uri, parser.fieldValue) == 0) {
      // sprawdzamy, czy na danym zasobie można zmienić stan
      if ((resources[resourceNumber].flags & 0x02) == 2) {
        // szukamy wolnej sesji
        for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
          if ((sessions[sessionNumber].details & 0x80) == 128) {
            sessions[sessionNumber].ipAddress = ip;
            sessions[sessionNumber].portNumber = portNumber;
            sessions[sessionNumber].token = parse.parseToken(message, parse.parseTokenLen(message));
            sessions[sessionNumber].id = ((resources[resourceNumber].flags & 0x0c) >> 2 ); 

            // szukamy opcji ContentFormat
            optionNumber = parser.getNextOption(message);
            while (optionNumber != CONTENT-FORMAT)  {
              //nie ma content-format! BLAD
              if (optionNumber > CONTENT-FORMAT || optionNumber == NO_OPTION){
                sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
                return;
              }
              optionNumber = parser.getNextOption(message);
            }
            
            // sprawdzamy zawartość opcji ContentFormat
            // zerowanie bitów content_type
            sessions[sessionNumber].details = (sessions[sessionNumber].details & 0xe0);
            if (strlen(parser.fieldValue) > 0) {
              if (parser.fieldValue[0] == PLAIN_TEXT) {
                sessions[sessionNumber].details = (sessions[sessionNumber].details);
              }
              else if (parser.fieldValue[0] == XML) {
                sessions[sessionNumber].details = (sessions[sessionNumber].details | 41);
              }
              else if (parser.fieldValue[0] == JSON) {
                sessions[sessionNumber].details = (sessions[sessionNumber].details | 50);
              }
              else {
                sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, METHOD_NOT_ALLOWED);
                return;
              }
            } else {
              sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, BAD_REQUEST);
              return;
            }

            // sprawdzamy wartość pola Type, jeżeli jest to CON to wysyłamy puste ack, jeżeli NON to kontynuujemy
            if ( parser.parseType(message) == TYPE_CON ){
              sendAckResponse(sessions[sessionNumber], message);
            }
            
            // wysyłamy żądanie zmiany zasobu do obiektu IoT wskazanego przez uri
            sendMessageToThing(POST_TYPE, sessions[sessionNumber].id, parser.parsePayload(message));
            return;
          } // end of (sessions[sessionNumber].contentFormat > 127)

          // brak wolnej sesji - bład INTERNAL_SERVER_ERROR
          sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, INTERNAL_SERVER_ERROR);
          return;  
        } // end of for loop (przeszukiwanie sesji)
      } // end of if((resources[resourceNumber].flags & 0x02) == 2
      
      // jeżeli otrzymaliśmy wiadomośc POST dotyczącą obiektu, którego stanu nie możemy zmienić to wysyłamy błąd
      sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, METHOD_NOT_ALLOWED); 
      
    } // end of if (strcmp(resources[resourceNumber].uri, parser.fieldValue) == 0)
  } // end of for loop (przeszukiwanie zasobów)
  
  // błędne uri - brak takiego zasobu na serwerze
  sendErrorResponse(udp.remoteIP(), udp.remotePort(), ethMessage, NOT_FOUND); 
}


/*  sendEthernetMessage(char* message, size_t messageSize, IPAddress ip, uint16_t port)
 *  Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej kod błedu.
 *  
*/
void sendErrorResponse(IPAddress ip, uint16_t portNumber, char* message, uint8_t errorType) {
  
}
/* 
 *  Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej potwierdzenie odebrania żądania.
 *  - pusta odpowiedź zawiera jedynie nagłówek 4bajtowy;
 *  - TYPE = TYPE_ACK;
 *  - TLK = 0 -> brak tokena
 *  - CODE = 0.00 (CLASS_REQ + DETAIL_EMPTY)
 *  - MessageID = MessageID z wiadomości;
*/
void sendEmptyAckResponse(Session* session, char* message) {
  byte response[4];
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_ACK);
  builder.setCodeClass(CLASS_REQ);
  builder.setCodeDetail(DETAIL_EMPTY);
  builder.setMessageId(parser.parseMessageId(message));
  response = builder.buildAckHeader();
  sendEthernetMessage(response, sizeof(response), session.ipAddress, session.portNumber)
}
/* 
 * Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej potwierdzenie odebrania żądania wraz z ładunkiem.
*/
void sendPiggybackAckResponse(IPAddress ip, uint16_t portNumber, char* message, uint8_t errorType) {
  
}
/* 
 *  Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi do klienta związanej z żądaniem POST.
*/
void sendPostResponse(Session* session) {
  char response;
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  // kod wiadomości 2.04
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(4);
  
  builder.setToken(session.token);
  messageId += 1;
  builder.setMessageId(messageId);
  response = builder.build();

  sendEthernetMessage(response, sizeof(response), session.ipAddress, session.portNumber)
}
/* 
 *  Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi do klienta.
*/
void sendResponse() {
  
}
/*  TO_DO: Trzeba by dorobić sprawdzanie, czy nowa wartość równa się tej żądanej w POST.
 *  Metoda odpowiedzialna za analizę twającej sesji.
 *  - sprawdzamy typ sesji;
 *  - jeżeli POST (1) to:
 *    - wyślij wiadomośc powrotną z kodem 2.04 (success.changed) oraz z tym samym tokenem;
 *    
 *  - jeżeli GET (0) to: ...
*/
void analyseSession(Session* session) {
  if ( ((session.detail & 0x60) >> 5) == 1 ) {
    // POST
    sendPostResponse(session);    
  }
  else if ( ((session.detail & 0x60) >> 5) == 0 ) {
    // GET

    
  }
}
// END:CoAP_Methodes-----------------------------

// START:Thing_Methodes
/* 
 *  Protokół radiowy:
 *  | 7  6 | 5  4  3  | 2 1 0 |
 *  | type | sensorID | value |
 *  
 *  type:     0-Response; 1-Get; 2-Post 
 *  sensorID: 0-Lamp; 1-Button
 *  value:    Lamp: 0-OFF 1-ON  Button: 1-change state(clicked)
 *  
*/

/* 
 *  Metoda odpowiedzialna za przetworzenie danych zgodnie z protokołem radiowym:
 *  - jeżeli typ wiadomości jest inny niż RESPONSE to porzucam wiadomość;
 *  - odnajdujemy sensorID w liście zasobów serwera (jeżeli brak, to odrzucamy)
 *  - aktualizujemy zawartość value w zasobie zgodnie z wartością znajdującą się w wiadomości
 *  
 *  - jeżeli jest jakaś sesja związana z danym sensorID to przekaż ją do analizy;
*/
void getMessageFromThing(byte message){ 
  if ( ((message & 0xc0) >> 6) == RESPONSE_TYPE ) {
    for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
      // odnajdujemy sensorID w liście zasobów serwera
      if ( ((message & 0x38) >> 3) == ((resources[resourceNumber].flags & 0x1c) >> 2) ) {
        // aktualizujemy zawartość value w zasobie
        resources[resourceNumber].value = (message & 0x07);


        // przeszukujemy tablicę sesji w poszukiwaniu sesji związanej z danym sensorID
        // jeżeli sesja jest aktywna i posiada sensorID równe sensorID z wiadomości to przekazujemy ją do analizy
        for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
          if ( ((sessions[sessionNumber].details & 0x80) == 128) 
                  && ((sessions[sessionNumber].sensorID == ((message & 0x38) >> 3))) ) {
            analyseSession(sessions[sessionNumber]);
          }
      }
    } 
    // jeżeli nie odnaleźliśy zasobu to porzucamy wiadomość
  }
  // porzuć wiadomość innną niż response
}
/*  DO ZWERYFIKOWANIA POPRAWNOŚCI
 *  Metoda odpowiedzialna za stworzenie wiadomości zgodnej z protokołem radiowym;
*/
void sendMessageToThing(uint8_t type, uint8_t sensorID, uint8_t value){
  byte message;
  message = ( message | ((type & 0x03) << 6) );
  message = ( message | ((sensorID & 0x07) << 3) );
  message = ( message | (value & 0x07) ); 
  sendRF24Message(message);
}
// END:Thing_Methodes
