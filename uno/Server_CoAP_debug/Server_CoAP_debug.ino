

#include <Resources.h>
#include <CoapParser.h>
#include <CoapBuilder.h>

/*
  CoapServer.ino
  Created in 2017 by:value
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

// constant variables neccessary for RF24 wireless connection;
const uint16_t THIS_NODE_ID = 1;
const uint16_t REMOTE_NODE_ID = 0;
const uint8_t RF_CHANNEL = 60;

// variables connected with our protocol between Uno and Mini Pro
const uint8_t RESPONSE_TYPE = 0;
const uint8_t GET_TYPE = 1;
const uint8_t PUT_TYPE = 2;

// variable connected with wireless object
RF24 radio(7, 8); // pin CE=7, CSN=8
RF24Network network(radio);

// variable connected with wired connection
EthernetUDP Udp;
byte mac[] = {00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
IPAddress ip(192, 168, 0, 33);
const short localPort = 5683;
const uint8_t MAX_BUFFER = 50; //do zastanowienia
char ethMessage[MAX_BUFFER];
uint8_t messageLen = 0;
IPAddress remoteIp;
uint16_t remotePortNumber;
uint8_t currentBlock = 0;

// CoAP variables:
Resource resources[RESOURCES_COUNT];

CoapParser parser = CoapParser();
CoapBuilder builder = CoapBuilder();
uint16_t messageId = 0; //globalny licznik - korzystamy z niego, gdy serwer generuje wiaodmość

void setup() {
  SPI.begin();
  Serial.begin(9600);

  radio.begin();
  network.begin(RF_CHANNEL, THIS_NODE_ID);
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);

  initializeResourceList();

  Serial.println(F("[setup]->READY"));
}

void loop() {
  // checking the network object regularly;
  network.update();
  receiveRF24Message();

  receiveEthernetMessage();
}



// Resources:Methodes---------------------------
void initializeResourceList() {
  // well-known.core
  resources[0].uri = ".well-known/core";
  resources[0].resourceType = "";
  resources[0].interfaceDescription = "";
  resources[0].value = 0;
  resources[0].size = 269;
  resources[0].flags = B00000000;

  // lampka
  resources[1].uri = "sensor/lamp";
  resources[1].resourceType = "Lamp";
  resources[1].interfaceDescription = "state";
  resources[1].value = 12 ; //OFF
  resources[1].flags = B00000010;

  // przycisk
  resources[2].uri = "sensor/button";
  resources[2].resourceType = "Button";
  resources[2].interfaceDescription = "state";
  resources[2].value = 0;
  resources[2].flags = B00000101;

  // metryka PacketLossRate
  resources[3].uri = "metric/PLR";
  resources[3].resourceType = "PacketLossRate";
  resources[3].interfaceDescription = "value";
  resources[3].value = 0 ;
  resources[3].flags = B00000000;

  // metryka ByteLossRate
  resources[4].uri = "metric/BLR";
  resources[4].resourceType = "ByteLossRate";
  resources[4].interfaceDescription = "value";
  resources[4].value = 0 ;
  resources[4].flags = B00000000;

  // metryka MeanAckDelay
  resources[5].uri = "metric/MAD";
  resources[5].resourceType = "MeanACKDelay";
  resources[5].interfaceDescription = "value";
  resources[5].value = 0 ;
  resources[5].flags = B00000000;

  // wysyłamy wiadomości żądające podania aktualnego stanu zapisanych zasobów
  // TO_DO: trzeba tutaj dorobić pętlę, jeżeli zaczyna sie od /sensor/ to wyslij geta
  //sendMessageToThing(GET_TYPE, ( (resources[1].flags & 0x1c) >> 2), 0);
}

// End:Resources--------------------------------


// RF24:Methodes--------------------------------

/*
    Metoda odpowiedzialna za odebranie danych dopóki są one dostepne na interfejsie radiowym.
*/
void receiveRF24Message() {
  while ( network.available() )
  {
    RF24NetworkHeader header;
    byte rf24Message;
    network.read(header, rf24Message, sizeof(rf24Message));
    getMessageFromThing(rf24Message);
  }
}
/*
    Metoda odpowiedzialna za wysyłanie danych interfejsem radiowym.
    - parametr message oznacza wiadomość, którą chcemy wysłać;
*/
void sendRF24Message(byte message) {
  RF24NetworkHeader header;
  network.write(header, &message, sizeof(message));
}
// END:RF23_Methodes----------------------------


// START:Ethernet_Methodes----------------------------

/*
    Metoda odpowiedzialna za odbieranie wiadomości z interfejsu ethernetowego
    - jeżeli pakiet ma mniej niż 4 bajty (minimalną wartośc nagłówka) to odrzucamy go;
    - jeżeli pakiet ma przynajmniej 4 bajty, zostaje poddany dalszej analizie;
*/
void receiveEthernetMessage() {
  messageLen = Udp.parsePacket();
  if (messageLen >= 4) {//parsePacket zwraca wielkosc pakietu, 0 oznacza nieodebranie pakietu
    Serial.println(F("[RECEIVE][ETH]->message"));
    remoteIp = Udp.remoteIP();
    remotePortNumber = Udp.remotePort();
    Udp.read(ethMessage, MAX_BUFFER);
    //  parserTest();
    getCoapClienMessage();
  }
}

void parserTest() {
  Serial.println(F("[TEST] version"));
  Serial.println(parser.parseVersion(ethMessage), BIN);
  Serial.println(F("[TEST] type"));
  Serial.println(parser.parseType(ethMessage), BIN);
  Serial.println(F("[TEST] tokenLEn"));
  Serial.println(parser.parseTokenLen(ethMessage), BIN);
  Serial.println(F("[TEST] parseCodeClass"));
  Serial.println(parser.parseCodeClass(ethMessage), BIN);
  Serial.println(F("[TEST] parseCodeDetail"));
  Serial.println(parser.parseCodeDetail(ethMessage), BIN);
  Serial.println(F("[TEST] parseMessageId"));
  Serial.println(parser.parseMessageId(ethMessage));
  Serial.println(F("[TEST] parseToken"));
  uint8_t tokenLen = parser.parseTokenLen(ethMessage);
  byte* token = parser.parseToken(ethMessage, tokenLen);
  for (int i = 0; i < tokenLen; i++) {
    Serial.print(token[i], BIN);
  }
  Serial.println(F(""));
  Serial.println(F("[TEST] getFirstOption"));
  Serial.println(parser.getFirstOption(ethMessage, messageLen));
  uint8_t len = parser.getOptionLen();
  Serial.println(len);
  for (int i = 0; i < len; i++) {
    Serial.print(parser.fieldValue[i], BIN);
  }
  Serial.println(F("[TEST] getNextOption"));
  Serial.println(parser.getNextOption(ethMessage, messageLen));
  len = parser.getOptionLen();
  Serial.println(len);
  for (int i = 0; i < len; i++) {
    Serial.print(parser.fieldValue[i], BIN);
  }
  sendContentResponse(1 , false, 255);

}
/*
    Metoda odpowiedzialna za wysyłanie wiadomości poprzez interfejs ethernetowy:
    - message jest wiadomością, którą chcemy wysłać
    - ip jest adresem ip hosta, do którego adresujemy wiadomość np:IPAddress adres(10,10,10,1)
    - port jest numerem portu hosta, do którego adresuemy wiadomość
*/
void sendEthernetMessage(byte* message, size_t messageSize, IPAddress ip, uint16_t port) {
  Udp.beginPacket(ip, port);
  int r = Udp.write(message, messageSize);
  Udp.endPacket();
  //jeżeli liczba danych przyjętych do wysłania przez warstwę niższą jest mniejsza niż rozmiar wiadomości to?
  if ( r < messageSize ) {

  }
}
// END:Ethernet_Methodes------------------------


// START:CoAP_Methodes---------------------------
/*
    Metoda odpowiedzialna za analizę wiadomości CoAP od klienta:
    - jeżeli wersja wiadomości jest różna od 01, wyślij błąd BAD_REQUEST;
    - jeżeli wersja wiadomości jest zgodna, sprawdź klasę wiadomości (jeżeli inna niż request, wyślij bład BAD_REQUEST);
    - uruchom odpowiednią metodę zależnie od pola CODE detail;

    - obsługiwane są tylko wiadomości CODE = EMPTY, GET lub PUT (inaczej wyślij błąd BAD_REQUEST);
    - EMPTY: taki kod może miec tylko wiadomośc typu ACK lub RST (inaczej wyślij bład BAD_REQUEST);
    - GET, PUT: taki kod może miec tylko wiadomość typu CON lub NON (inaczej wyślij błąd BAD_REQUEST);
*/
void getCoapClienMessage() {

  Serial.println(F("[RECEIVE][COAP]->Message:"));
  Serial.println(ethMessage);
  Serial.println(F("[RECEIVE][COAP]->END Message"));

  if (parser.parseVersion(ethMessage) != 1) {
    sendErrorResponse(remoteIp, remotePortNumber, ethMessage, BAD_REQUEST, "WRONG VERSION TYPE");
    return;
  }
  else if ( parser.parseCodeClass(ethMessage) != CLASS_REQ) {
    sendErrorResponse(remoteIp, remotePortNumber, ethMessage, BAD_REQUEST, "WRONG CLASS TYPE");
    return;
  }

  switch (parser.parseCodeDetail(ethMessage)) {
    case DETAIL_EMPTY:
      if ( (parser.parseType(ethMessage) == TYPE_ACK) || (parser.parseType(ethMessage) == TYPE_RST) ) {
        receiveEmptyRequest(ethMessage, remoteIp, remotePortNumber);
        return;
      }
      break;
    case DETAIL_GET:
      if ( (parser.parseType(ethMessage) == TYPE_CON) || (parser.parseType(ethMessage) == TYPE_NON) ) {
        receiveGetRequest();
        return;
      }
      break;
    case DETAIL_PUT:
      if ( (parser.parseType(ethMessage) == TYPE_CON) || (parser.parseType(ethMessage) == TYPE_NON) ) {
        receivePutRequest(ethMessage, remoteIp, remotePortNumber);
        return;
      }
      break;
    default:
      sendErrorResponse(remoteIp, remotePortNumber, ethMessage, BAD_REQUEST, "WRONG DETAIL CODE");
      return;
  }

  /* wyślij błąd oznaczający zły typ widomości do danego kodu detail */
  sendErrorResponse(remoteIp, remotePortNumber, ethMessage, BAD_REQUEST, "WRONG TYPE OF MESSAGE");
}

/*
    Metoda odpowiedzialna za analizę wiadomości typu GET:
    Możliwe opcje: OBSERV(6), URI-PATH(11), ACCEPT(17), ETAG(4);

    - opcja URI-PATH jest obowiązkowa w kazdym żądaniu GET;
    - reszta opcji jest opcjonalna;
    - opcja ETAG może wystepować maksymalnie ETAG_MAX_OPTIONS_NUMBER razy;

    Wczytywanie opcji wiadomości
    - odczytujemy pierwszą opcje wiadomości (jeżeli pierwsza opcja ma numer większy niż URI-PATH to wyslij błąd BAD_REQUEST);
    - przed opcją URI-PATH może być opcja ETAG oraz OBSERV
    - jeżeli dane opcje wystąpią, zapisz je do zmiennych lokalnych
    - odczytujemy zawartość opcji URI-PATH
    - jeżeli występuje opcja ACCEPT, odczytaj ją i zapisz jej zawartość do zmiennej lokalnej;

    Analiza wiadomości
    - szukamy zasobu wskazanego w opcji URI-PATH na serwerze (jeżeli brak, wyslij bład

    - analiza zawartości opcji OBSERVE:
      - 0: jeżeli dany klient nie znajduje się na liście obserwatorów to go dodajemy
        - jeżeli klient znajduje się na liście:
          - jeżeli wartość tokena jest inna to aktualizujemy ją;
        - jeżeli klienta nie ma na liście:
          - jeżeli jest miejsce na liście to dodajemy go;
          - jeżeli nie ma miejsca na liście, wysyłamy błąd serwera INTERNAL_SERVER_ERROR
      - 1: jeżeli dany klienta znajduje sie na liscie obserwatorów to usuwamy go
        - jeżeli klient istnieje na liście to bez względu na wartość tokena zmieniamy status wpisu na wolny;

    - analiza zawartości opcji Etag:
      -
*/
void receiveGetRequest() {

  Serial.println(F("[RECEIVE][COAP][GET]"));

  char etagOptionValues[ETAG_MAX_OPTIONS_COUNT][8];
  int etagValueNumber = 0;
  uint8_t etagCounter = "";  // JAK ZROBIC TO NA LISCIE ABY MOC ROBIC KILKA ETAGOW
  uint8_t observeOptionValue = 2; // klient może wysłac jedynie 0 lub 1
  char uriPath[20] = "";
  uint8_t acceptOptionValue = 0;
  uint8_t blockOptionValue = 255;

  /*---wczytywanie opcji-----------------------------------------------------------------------------*/
  uint8_t optionNumber = parser.getFirstOption(ethMessage, messageLen);

  Serial.println(F("[RECEIVE][COAP][GET]->First Option Number"));
  Serial.println(optionNumber);

  if ( optionNumber != URI_PATH ) {
    if ( optionNumber > URI_PATH ) {
      /* pierwsza opcja ma numer większy niż URI-PATH - brak wskazania zasobu */
      sendErrorResponse(remoteIp, remotePortNumber, ethMessage, BAD_REQUEST, "NO URI");
      return;
    }
    else {
      /* szukamy opcji Etag oraz Observe */
      while ( optionNumber != URI_PATH ) {
        optionNumber = parser.getNextOption(ethMessage, messageLen);
      } // end of while loop
    } // end of else
  } // end of if (firstOption != URI_PATH)

  /* odczytujemy wartość URI-PATH */
  strcpy(uriPath, parser.fieldValue);

  optionNumber = parser.getNextOption(ethMessage, messageLen);
  while (optionNumber == URI_PATH) {
    strcat(uriPath, "/");
    strcat(uriPath, parser.fieldValue);
    optionNumber = parser.getNextOption(ethMessage, messageLen);
  }

  Serial.println(F("[RECEIVE][COAP][GET]->URI_Path option"));
  Serial.println(uriPath);


  /* przeglądamy dalsze opcje wiadomości w celu odnalezienia opcji ACCEPT */
  while ( optionNumber != NO_OPTION ) {
    Serial.println(F("[RECEIVE][COAP][GET]->While"));
    Serial.println(optionNumber);
    if ( optionNumber == ACCEPT ) {
      /* wystąpiła opcja ACCEPT, zapisz jej zawartość */
      acceptOptionValue = parser.fieldValue[0];
      Serial.println(F("[RECEIVE][COAP][GET]->Accpet option"));
      Serial.println(acceptOptionValue);
      if (acceptOptionValue != 0) {
        sendErrorResponse(remoteIp, remotePortNumber, ethMessage, BAD_REQUEST, "BAD accept value: only text/plain allowed");
        return;
      }


    }
    if ( optionNumber == BLOCK2 ) {
      /* wystąpiła opcja BLOCK2, zapisz jej zawartość */
      blockOptionValue = parser.fieldValue[0];

      Serial.println(F("[RECEIVE][COAP][GET]->BLOCK2 option"));
      Serial.println(blockOptionValue);

    }
    optionNumber = parser.getNextOption(ethMessage, messageLen);
  }
  /*---koniec wczytywania opcji-----------------------------------------------------------------------------*/

  /*---zabronione kombinacje opcji---*/
  /*---koniec zabronionych kombinacji opcji---*/

  /* sprawdzamy, czy wskazanym zasobem jest well.known/core*/
  if ( strcmp(resources[0].uri, uriPath ) == 0 ) {
    /* wysyłamy wiadomosc zwrotna z ladunkiem w opcji blokowej */
    if (blockOptionValue == 255) { //brak opcji block
      sendWellKnownContentResponse(0x02);//zadanie 0-wego bloku o dlugosc 64B
    } else {
      sendWellKnownContentResponse(blockOptionValue);
    }
    
  } else {

    /*----analiza wiadomości---- */
    /* szukamy wskazanego zasobu na serwerze */
    for (uint8_t resourceNumber = 1; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
      if ( strcmp(resources[resourceNumber].uri, uriPath) == 0) {
        /* wskazany zasób znajduje się na serwerze */

        Serial.println(F("[RECEIVE][COAP][GET]->Resource Number:"));
        Serial.println(resourceNumber);

        /* zmienne mówiące o pozycji w listach */
        uint8_t observatorIndex = MAX_OBSERVATORS_COUNT + 1;
        uint8_t etagIndex = MAX_ETAG_COUNT + 1;



        /*-------analiza wiadomości z opcją acccept------------------------------------------------------------------------------*/
        if ( acceptOptionValue == 0 ) {
          Serial.println(F("[RECEIVE][COAP][GET]->Accept:"));
          switch (acceptOptionValue) {
            case 0:
              /* plain-text */
              break;
            case 50:
              /* j-son */
              // DOROBIĆ
              break;
            default:
              /* wyślij odpowiedz z kodem błedu 4.06 Not acceptable */
              sendErrorResponse(remoteIp, remotePortNumber, ethMessage, NOT_ACCEPTABLE, "UNKNOWN ACCEPT VALUE");
              break;
          }

          if ( observeOptionValue != 2 ) {
            /* analiza wiadomości z opcjami accept + observe + URI_PATH */
          }

          /* analiza wiadomości z opcją accept + URI_PATH */
        }
        /*-------koniec analizy wiadomości z opcją acccept-----------------------------------------------------------------------*/

        /*-----analiza wiadomości z opcją observe i URI_PATH----------------------------------------------------------*/
        if ( observeOptionValue != 2 ) {
          /* wyslij wiadomosc 2.05 NON content wraz z opcją observe */
          //sendContentResponse(remoteIp, remotePortNumber, observators[observatorIndex].token, getCharValueFromResourceValue(resources[resourceNumber].value, 0), true);
          // return;
        }
        /*-----koniec analizy wiadomości z opcją observe i URI_PATH-------------------------------------------------- */


        /*-----analiza wiadomości jedynie z opcją URI-PATH----------------------------------------------------------*/
        /* wyslij wiadomość NON 2.05 content bez opcji, z samym paylodem w dowolnej dostępnej formie */
        Serial.println(F("[RECEIVE][COAP][GET]->Only Uri Path"));
        Serial.println(resources[resourceNumber].value);
        /*
           Block value processing
        */
        if (blockOptionValue != 255) { // inne wartosci sa krótsze od 16B
          sendContentResponse(resourceNumber , false, (blockOptionValue & 0x07));
        } else {
          sendContentResponse(resourceNumber , false, 255);
        }

        return;
        /*-----koniec analizy wiadomości jedynie z opcją URI-PATH------------------------------------------------- */

      }  //if (strcmp(resources[resourceNumber].uri, uriPath) == 0)
    } //for loop (przeszukiwanie zasobu)

    /* nie znaleziono zasobu na serwerze */
    /* wiadomość zwrotna zawierająca kod błedu 4.04 NOT_FOUND */
    Serial.println(F("[RECEIVE][COAP][GET]->Resource not found"));

    sendErrorResponse(remoteIp, remotePortNumber, ethMessage, NOT_FOUND, "RESOURCE NOT FOUND");
  }
}


void getCharValueFromResourceValue(char* charValue, unsigned long value, uint16_t requestedType) {
  switch (requestedType) {
    case 0:
      /* plain-text */
      if (value > 10) {
        charValue[0] = value / 10 + '0';
        charValue[1] = (value % 10) + '0';
        charValue[2] = '\0';
      } else {
        charValue[0] = value + '0';
        charValue[1] = '\0';
      }

      break;
      //          case 50:
      //            /* j-son */
      //            break;
      //          default:
      //            break;
  }
}

/**
   Metoda odpowiedzialna za wyszukiwanie klienta w liście obserwatorów
*/
int checkIfClientIsObserving(char* message, IPAddress ip, uint16_t portNumber, int observatorIndex, int resourceNumber) {
  return -1;
}

/**
   Metoda odpowiedzialna za obsługe wiadomości ACK i RST
   Takie wiaodmości mogą być wysłane jedynie w odpowiedzi na wiadomość aktualizacyjną stan zasobu
   - przeszukujemy listę sesji;
   - jeśeli wiadomość typu ack:
      - zmieniamy status sesji na wolny;

   - jeżeli wiaodmość typu rst:
      - szukamy klienta w liscie obserwatorów
      - zmieniamy stan obserwatora na wolny (usuwamy klienta)
*/
void receiveEmptyRequest(char* message, IPAddress ip, uint16_t portNumber) {

}

/*
    Metoda odpowiedzialna za analizę wiadomości typu PUT:
    Wiadomość zawiera opcje: URI-PATH, CONTENT-FORMAT oraz payload z nową wartością;
    - odczytujemy opcję URI-PATH (jak nie ma, wysyłamy błąd BAD_REQUEST);
    - szukamy zasobu na serwerze (jak nie ma, wysyłmy bład NOT_FOUND);
    - jeżeli zasób nie ma możliwości zmiany stanu, wtedy wysyłamy bład METHOD_NOT_ALLOWED;
    - szukamy wolnej sesji (jeżeli brak, odrzucamy wiadomość z błędem serwera INTERNAL_SERVER_ERROR;
    - odczytujemy format zasobu (jak nie ma, wysyłamy bład BAD_REQUEST);
    - jeżeli format zasobu jest inny niż PlainText, Json, XMl to zwróc bład METHOD_NOT_ALLOWED;
    - odczytujemy zawartość PAYLOADu (jeśli długość zerowa, wysyłamy bład BAD_REQUEST);
    - sprawdzamy wartość pola Type, jeżeli jest to CON to wysyłamy puste ack, jeżeli NON to kontynuujemy;
    - wyślij żądanie zmiany stanu zasobu do obiektu IoT;

    -TO_DO: obsługa XML i JSONa
*/
void receivePutRequest(char* message, IPAddress ip, uint16_t portNumber) {

}


/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej kod błedu oraz payload diagnostyczny.
    -jesli żądanie typu CON - odpowiedz typu ACK
    -jesli żądanie typu NON - odpowiedz typu NON
    -jesli w żądaniu jest token to go przepisujemy
*/
void sendErrorResponse(IPAddress ip, uint16_t portNumber, char* message, uint16_t errorType, char * errorMessage) {
  builder.init();
  if (parser.parseType(message) == TYPE_CON)
    builder.setType(TYPE_ACK);
  else
  {
    builder.setType(TYPE_NON);
  }
  if (parser.parseTokenLen(message) > 0) {
    builder.setToken(parser.parseToken(message, parser.parseTokenLen(message)), parser.parseTokenLen(message));
  }

  if (errorType < 500) { //server error
    builder.setCodeClass(CLASS_SERR);

  } else {
    builder.setCodeClass(CLASS_CERR);
  }

  builder.setCodeDetail(errorType);
  builder.setMessageId(messageId++);
  builder.setPayload(errorMessage);

  sendEthernetMessage(builder.build(), builder.getResponseSize(), ip, portNumber);
}
/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej potwierdzenie odebrania żądania.
    (metoda wywoływana w momencie otrzymania wiadomości typu CON)
    - pusta odpowiedź zawiera jedynie nagłówek 4bajtowy;
    - TYPE = TYPE_ACK;
    - TLK = 0 -> brak tokena
    - CODE = 0.00 (CLASS_REQ + DETAIL_EMPTY)
    - MessageID = MessageID z wiadomości;
*/
void sendEmptyAckResponse(Session * session) {
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_ACK);
  builder.setCodeClass(CLASS_REQ);
  builder.setCodeDetail(DETAIL_EMPTY);
  builder.setMessageId(session->messageID);

  sendEthernetMessage(builder.build(), builder.getResponseSize(), session->ipAddress, session->portNumber);
}




/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi z kodem 2.05 do klienta z ładunkiem:
    - wersja protokołu: 1;
    - typ wiadomości: NON
    - kod wiadomości: 2.05 content
    - messageID: dowolna watość (ustawiamy jako 0);
    - token: podany jako parametr wywołania;
    - opcja observe: jeżeli parametr addObserveOption= true to dołącz opcję observe z wartością globalną powiększoną o 1;
    - ładunek: przekazany jako parametr;

    - budujemy wiadomość zwrotną;
    - wysyłamy ethernetem przekazując dane do metody sendEthernetMessage

    (wiadomość taka z opcją observe może być stworzona w momencie, gdy serwer nie ma już wolnych zasobó na nowe etagi
    ale zmienił się stan zabosu i chcialby poinformowac o tym obserwatorów danego zasobu )
*/
void sendContentResponse(uint8_t resourceNumber, bool addObserveOption, uint8_t blockValue) {

  builder.init();
  Serial.println(F("[sendContentResponse] build init"));
  Serial.println(blockValue);
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  /* kod wiaodmości 2.05 CONTENT */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(5);
  Serial.println(F("[sendContentResponse] build setCodeDetail"));
  builder.setMessageId(messageId++);
  uint8_t tokenLen = parser.parseTokenLen(ethMessage);
  byte* token = parser.parseToken(ethMessage, tokenLen);
  builder.setToken(token, tokenLen);
  Serial.println(F("[sendContentResponse] build tokenLen"));
  //if (addObserveOption) {
  //  builder.setOption(OBSERVE, ++observeCounter);
  //  }

  builder.setOption(CONTENT_FORMAT, PLAIN_TEXT);
  if ((blockValue & 0x07) != 7) {
    char blockString[2];
    blockString[0] = blockValue;
    blockString[1] = '\0';
    builder.setOption(BLOCK2, blockString);
  }
  char charValue[5];
  getCharValueFromResourceValue(charValue, resources[resourceNumber].value, 0);
  builder.setPayload(charValue);
  Serial.println(F("[sendContentResponse] build setPayload"));
  sendEthernetMessage(builder.build(), builder.getResponseSize(), remoteIp, remotePortNumber);

  //Serial.println(F("[SEND][COAP][CONTENT_RESPONSE]->token/payload/observeOption"));

  /* wyślij utworzoną wiaodmość zwrotną */
  Serial.println(F("[SEND][COAP][CONTENT_RESPONSE]->END"));

  //  sendEthernetMessage(builder.build(), builder.getResponseSize(), ip , portNumber);
}
/*
   tak jak poprzednia funkcja z następującymi wyjątkami:
   nie ma argumentu payload, ponieważ jest on w pewnym sensie stały (well-know-core)
*/
void sendWellKnownContentResponse(uint8_t blockValue) {
  Serial.println(F("[SEND][COAP][WELL_KNOWN]"));
  Serial.println(F("[SEND][COAP][WELL_KNOWN]"));
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  /* kod wiaodmości 2.05 CONTENT */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(5);

  builder.setMessageId(messageId++);
  uint8_t tokenLen = parser.parseTokenLen(ethMessage);
  byte* token = parser.parseToken(ethMessage, tokenLen);
  builder.setToken(token, tokenLen);

  builder.setOption(CONTENT_FORMAT, PLAIN_TEXT);

  Serial.println(F("[SEND][COAP][WELL_KNOWN]option"));
  /*narazie zakładamy ze bloki beda 64bitowe, wiec blokow w naszym przypadku zawsze bedzie mniej niz 16 ((blockNumber & 0x0f) << 4)
     cztery najstrasze bity okreslaja numer bloku,
     3 najmlodsze okreslaja rozmiar bloku
  */
  char blockString[2];
  uint16_t blockSize = 2;
  if ((blockValue & 0x07) == 0) {// nie ma mozliwosc pobrania blokow 16B, minimum 32B
    blockValue += 1;
  }
  for (int i = 1; i < 4 + (blockValue & 0x07); i++) {
    blockSize *= 2;
  }
  Serial.println(blockSize);
  blockString[1] = '\0';
  Serial.println(resources[0].size);
  Serial.println(((blockValue & 0xf0) >> 4));
  Serial.println(blockSize * ((blockValue & 0xf0) >> 4));
  Serial.println(resources[0].size - blockSize * ((blockValue & 0xf0) >> 4));
  if (resources[0].size > blockSize * (((blockValue & 0xf0) >> 4) + 1)) {
    // 8 - ustawiamy 4. najmłodszy bit na 1, jeśli jest jeszcze cos do wysłania
    blockString[0] = (blockValue & 0xf7) + 8;
  } else {
    blockString[0] = (blockValue & 0xf7);
  }
  Serial.println(blockValue & 0xf7);
  Serial.println(blockString[0], BIN);
  builder.setOption(BLOCK2, blockString);


  setWellKnownCorePayload((blockValue & 0xf0) >> 4, blockSize);


  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(builder.build(), builder.getResponseSize(), remoteIp, remotePortNumber);
}
/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi z kodem 2.05 do klienta z ładunkiem oraz opcjami ETAG, OBSERVE:
    - wersja protokołu: 1;
    - typ wiadomości: CON
    - kod wiadomości: 2.05 content
    - messageID: pobrany z sesji;
    - token: pobrany z sesji;
    - opcja etag: pobrany z sesji;
    - opcja observe: globalny licznik zwiększony o 1;
    - ładunek: wartość etaga zapisanego w sesji;

    - budujemy wiadomość zwrotną;
    - wysyłamy ethernetem przekazując dane do metody sendEthernetMessage

    - parametr sesji oznacza sesję nawiązaną miedzy serwerem a klientem, która dotyczy wiadomości typu CON;
      (sesja przestanie być aktywna w momencie otrzymania potwierdzenia ACK z tym samym messageId);
*/
void sendContentResponseWithEtag(Session* session) {
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_CON);

  /* kod wiaodmości 2.05 CONTENT */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(5);

  //  builder.setToken(session->token);
  builder.setMessageId(session->messageID);
  builder.setOption(ETAG, session->etag.etagId);

  // builder.setOption(OBSERVE, ++observeCounter);

  //TODO zalezc
  builder.setPayload(session->etag.savedValue);

  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(builder.build(), builder.getResponseSize(), session->ipAddress, session->portNumber);
}
/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi do klienta z kodem 2.03 VALID, opcją Etag, Observe oraz bez payloadu.
    (zakładamy, że kod 2.03 może być tylko wysłany w mechanizmie walidacji opcji Etag podczas działającego mechanizmu Observe)
    - wersja protokołu: 1;
    - typ wiadomości: CON
    - kod wiadomości: 2.03 valid
    - messageID: pobrany z sesji;
    - token: pobrany z sesji;
    - opcja:etag: pobrany z sesji;
    - opcja observe: globalny licznik zwiększony o 1;

    - budujemy wiadomość zwrotną;
    - wysyłamy ethernetem przekazując dane do metody sendEthernetMessage

    - parametr sesji oznacza sesję nawiązaną miedzy serwerem a klientem, która dotyczy wiadomości typu CON;
      (sesja przestanie być aktywna w momencie otrzymania potwierdzenia ACK z tym samym messageId);
*/
void sendValidResponse(Session* session) {
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_CON);

  /* kod wiaodmości 2.03 VALID */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(3);

  //  builder.setToken(session->token);

  builder.setMessageId(session->messageID);

  builder.setOption(ETAG, session->etag.etagId);
  // builder.setOption(OBSERVE, ++observeCounter);

  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(builder.build(), builder.getResponseSize(), session->ipAddress, session->portNumber);
}

void setWellKnownCorePayload(uint8_t blockNumber, uint8_t blockSize) {
  //initializacja pola payload (czyszczenie pola, dodanie znacznika)
  if (blockSize == 16) {
    blockSize = 32;
  }
  if (blockNumber == 0) {
    builder.setPayload("</.well-known/core>,");
  } else {
    builder.setPayload("");
  }
  currentBlock = 0;
  uint8_t freeLen = blockSize - 20;// 20 - dlugosc "</.well-known/core>,"
  Serial.println(F("setWellKnownCorePayload->start"));
  Serial.println(freeLen);
  for (int index = 1; index < RESOURCES_COUNT; index++ ) {
    Serial.println(index);
    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, "</", blockSize)) {
      return;
    }
    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, resources[index].uri, blockSize)) {
      return;
    }

    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, ">;", blockSize)) {
      return;
    }
    if ((resources[index].flags & 0x01) == 1 ) {
      Serial.println(F("OBSERVE"));
      if (setWellKnownCorePartToPayload(&freeLen, blockNumber, "</obs>;", blockSize)) {
        return;
      }
    }
    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, "rt=\"", blockSize)) {
      return;
    }
    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, resources[index].resourceType, blockSize)) {
      return;
    }

    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, "\";title=\"", blockSize)) {
      return;
    }

    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, resources[index].interfaceDescription, blockSize)) {
      return;
    }
    if (setWellKnownCorePartToPayload(&freeLen, blockNumber, "\",", blockSize)) {
      return;
    }
  }
  setWellKnownCorePartToPayload(&freeLen, blockNumber, "</shutdown>", blockSize);
}

//zwraca informacje o tym czy odpowiedni blok został już zapełniony
bool setWellKnownCorePartToPayload(uint8_t* freeLen, uint8_t blockNumber, char* message, uint8_t blockSize) {
  Serial.println(F("setWellKnownCorePartToPayload->start"));
  if (*freeLen == 0) {
    if (currentBlock == blockNumber) {
      return true;
    }
    return false;
  }
  Serial.println(message);
  Serial.println(*freeLen);
  Serial.println(strlen(message));
  Serial.println(builder.getPayloadLen());
  if ((*freeLen) > strlen(message)) {
    Serial.println(F("CALOSC"));
    Serial.println(currentBlock);
    Serial.println(blockNumber);
    Serial.println(currentBlock == blockNumber);
    if (currentBlock == blockNumber) {
      builder.appendPayload(message);
    }
    (*freeLen) -= strlen(message);
  } else { //cały się nie zmiesci
    if (currentBlock == blockNumber) {
      builder.appendPayload(message, (*freeLen));
      Serial.println(F("setWellKnownCorePartToPayload->stop1"));
      return true;
    } else {
      Serial.println(currentBlock);
      currentBlock += 1;
      Serial.println(currentBlock);
      if (currentBlock == blockNumber) {
        builder.appendPayload(message, (*freeLen), strlen(message));
      }
      (*freeLen) = blockSize - (strlen(message) - (*freeLen));
      Serial.println((*freeLen));
    }
    Serial.println(F("setWellKnownCorePartToPayload->stop2"));
  }
  Serial.println(F("setWellKnownCorePartToPayload->stop3"));
  return false;
}
// END:CoAP_Methodes-----------------------------



// START:Thing_Methodes----------------------------------
/*
    Protokół radiowy:
    | 7  6 | 5  4  3  | 2 1 0 |
    | type | sensorID | value |

    type:     0-Response; 1-Get; 2-Put
    sensorID: 0-Lamp; 1-Button
    value:    Lamp: 0-OFF 1-ON  Button: 1-change state(clicked)

*/

/*
    Metoda odpowiedzialna za przetworzenie danych zgodnie z protokołem radiowym:
    - jeżeli typ wiadomości jest inny niż RESPONSE to porzucam wiadomość;
    - odnajdujemy sensorID w liście zasobów serwera (jeżeli brak, to odrzucamy)
    - aktualizujemy zawartość value w zasobie zgodnie z wartością znajdującą się w wiadomości

    - jeżeli jest jakaś sesja związana z danym sensorID to przekaż ją do analizy;
*/
void getMessageFromThing(byte message) {
  Serial.println(F("[RECEIVE][MINI]->Start"));

  if ( ((message & 0xc0) >> 6) == RESPONSE_TYPE ) {
    for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
      // odnajdujemy sensorID w liście zasobów serwera
      if ( ((message & 0x38) >> 3) == ((resources[resourceNumber].flags & 0x1c) >> 2) ) {
        // aktualizujemy zawartość value w zasobie
        strcpy(resources[resourceNumber].value, (message & 0x07));

        // przeszukujemy tablicę sesji w poszukiwaniu sesji związanej z danym sensorID
        // jeżeli sesja jest aktywna i posiada sensorID równe sensorID z wiadomości to przekazujemy ją do analizy

      }
      // jeżeli nie odnaleźliśy zasobu to porzucamy wiadomość
    }
    // porzuć wiadomość innną niż response
  }
}
/*

    Metoda odpowiedzialna za stworzenie wiadomości zgodnej z protokołem radiowym;
*/
void sendMessageToThing(uint8_t type, uint8_t sensorID, uint8_t value) {
  Serial.println(F("[SEND][MINI]->Start"));

  byte message;
  message = ( message | ((type & 0x03) << 6) );
  message = ( message | ((sensorID & 0x07) << 3) );
  message = ( message | (value & 0x07) );
  sendRF24Message(message);
}
// END:Thing_Methodes



