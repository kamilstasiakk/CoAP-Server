

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
const uint8_t MAX_BUFFER = 50;
char ethMessage[MAX_BUFFER];
uint8_t messageLen = 0;
IPAddress remoteIp;
uint16_t remotePortNumber;
uint8_t currentBlock = 0;

// CoAP variables:
Resource resources[RESOURCES_COUNT];

Observator observators[MAX_OBSERVATORS_COUNT];
uint8_t observeCounter = 2; // globalny licznik związany z opcją observe, zaczynamy od 2

Etag globalEtags[MAX_ETAG_COUNT];
uint8_t globalEtagsCounter = 0; // globalny licznik związany z ilością uzytych etagow
uint8_t etagIdCounter = 1; // globalny licznik potrzebny do generowania id etagow, zaczynamy od 1

Session sessions[MAX_SESSIONS_COUNT];

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

  //testowanie
  testowanie();
}
void loop() {
  // checking the network object regularly;
  network.update();
  receiveRF24Message();

  receiveEthernetMessage();

  for (int sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++ ) {
    if( sessions[sessionNumber].details < 128 && ((millis() - sessions[sessionNumber].sessionTimestamp) > CON_TIMEOUT)) {
      //sendEtagResponse(sessionNumber,sessions[sessionNumber].details & 0x01, 255);
      sessions[sessionNumber].sessionTimestamp = millis();
      sessions[sessionNumber].messageID = messageId++;
    }
  }
}



void testowanie() {
  globalEtagsCounter++;
  globalEtags[0].resource = &resources[2];
  globalEtags[0].savedValue = 0;
  globalEtags[0].etagId = etagIdCounter++;
  globalEtags[0].timestamp = 0;
  globalEtags[0].details = 0;

  //  globalEtagsCounter++;
  //  globalEtags[1].resource = &resources[2];
  //  globalEtags[1].savedValue = 1;
  //  globalEtags[1].etagId = etagIdCounter++;
  //  globalEtags[1].timestamp = 0;
  //  globalEtags[1].details = 0;

}


// Resources:Methodes---------------------------
void initializeResourceList() {
  // well-known.core
  resources[0].uri = ".well-known/core";
  resources[0].resourceType = "";
  resources[0].interfaceDescription = "";
  resources[0].value = 0;
  resources[0].size = 247;
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
  resources[2].value = 1;
  resources[2].flags = B00000101;

  // metryka PacketLossRate
  resources[3].uri = "metric/CC";
  resources[3].resourceType = "ConPacketsCount";
  resources[3].interfaceDescription = "value";
  resources[3].value = 0 ;
  resources[3].flags = B00000000;

  // metryka ByteLossRate
  resources[4].uri = "metric/NC";
  resources[4].resourceType = "ConPacketsCount";
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
    network.read(header, &rf24Message, sizeof(rf24Message));
    getMessageFromThing(rf24Message);
  }
}
/*
    Metoda odpowiedzialna za wysyłanie danych interfejsem radiowym.
    - parametr message oznacza wiadomość, którą chcemy wysłać;
*/
void sendRF24Message(byte message) {
  RF24NetworkHeader header(REMOTE_NODE_ID);
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
    sendErrorResponse(BAD_REQUEST, "WRONG VERSION TYPE");
    return;
  }
  else if ( parser.parseCodeClass(ethMessage) != CLASS_REQ) {
    sendErrorResponse(BAD_REQUEST, "WRONG CLASS TYPE");
    return;
  }

  switch (parser.parseCodeDetail(ethMessage)) {
    case DETAIL_EMPTY:
      if ( (parser.parseType(ethMessage) == TYPE_ACK) || (parser.parseType(ethMessage) == TYPE_RST) ) {
        receiveEmptyRequest();
        return;
      }
      break;
    case DETAIL_GET:
      if ( (parser.parseType(ethMessage) == TYPE_CON) || (parser.parseType(ethMessage) == TYPE_NON) ) {
        if (parser.parseType(ethMessage) == TYPE_CON) {
          resources[3].value++;
        }
        if (parser.parseType(ethMessage) == TYPE_NON) {
          resources[4].value++;
        }
        
        receiveGetRequest();
        return;
      }
      break;
    case DETAIL_PUT:
      if ( (parser.parseType(ethMessage) == TYPE_CON) || (parser.parseType(ethMessage) == TYPE_NON) ) {
        receivePutRequest();
        return;
      }
      break;
    default:
      sendErrorResponse(BAD_REQUEST, "WRONG DETAIL CODE");
      return;
  }

  /* wyślij błąd oznaczający zły typ widomości do danego kodu detail */
  sendErrorResponse(BAD_REQUEST, "WRONG TYPE OF MESSAGE");
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
  Serial.println(F("[RECEIVE][COAP][GET]->START..."));

  uint8_t etagOptionValue = 0; // serwer nadaje wartosci powyzej 1
  uint8_t observeOptionValue = 2; // klient może wysłac jedynie 0 lub 1
  char uriPath[20] = "";
  uint8_t acceptOptionValue = 0;
  uint8_t blockOptionValue = 255;

  /*---wczytywanie opcji-----------------------------------------------------------------------------*/
  uint8_t optionNumber = parser.getFirstOption(ethMessage, messageLen);

  Serial.println(F("[RECEIVE][COAP][GET]->First Option Number"));
  Serial.println(optionNumber);

  if ( optionNumber != URI_PATH ) {
    if ( optionNumber > URI_PATH || optionNumber == NO_OPTION) {
      /* pierwsza opcja ma numer większy niż URI-PATH lub wiadomośc jest bez opcji - brak wskazania zasobu */
      sendErrorResponse(BAD_REQUEST, "NO URI");
      return;
    }
    else {
      /* szukamy opcji Etag oraz Observe */
      while ( optionNumber != URI_PATH && optionNumber != NO_OPTION) {
        if ( optionNumber == ETAG ) {
          /* wystąpiła opcja ETAG, zapisz jej zawartość */
          etagOptionValue = parser.fieldValue[0];
          Serial.println(F("[SEND][COAP][get]->etagValue"));
          Serial.println(etagOptionValue);
        }
        if ( optionNumber == OBSERVE ) {
          /* wystąpiła opcja OBSERVE, zapisz jej zawartość */
          observeOptionValue = parser.fieldValue[0];
        }
        optionNumber = parser.getNextOption(ethMessage, messageLen);
      } // end of while loop
    } // end of else
  } // end of if (firstOption != URI_PATH)

  /* odczytujemy wartość URI-PATH */
  if ( optionNumber != NO_OPTION ) {
    strcpy(uriPath, parser.fieldValue);
  }
  else {
    /* wiadomość nie zawierala opcji URI_PATH, ale zawiera jakies inne opcje */
    sendErrorResponse(BAD_REQUEST, "NO URI");
    return;
  }

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
        sendErrorResponse(BAD_REQUEST, "BAD accept value: only text/plain allowed");
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

  /* sprawdzamy, czy wskazanym zasobem jest well.known/core*/
  if ( strcmp(resources[0].uri, uriPath ) == 0 ) {
    /* wysyłamy wiadomosc zwrotna z ladunkiem w opcji blokowej */
    if (blockOptionValue == 255) { //brak opcji block
      sendWellKnownContentResponse(0x02);//zadanie 0-wego bloku o dlugosc 64B
    } else {
      sendWellKnownContentResponse(blockOptionValue);
    }
  }
  /* szukamy wskazanego zasobu na serwerze */
  else {
    for (uint8_t resourceNumber = 1; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
      if ( strcmp(resources[resourceNumber].uri, uriPath) == 0) {
        /* wskazany zasób znajduje się na serwerze */

        Serial.println(F("[RECEIVE][COAP][GET]->Resource Number:"));
        Serial.println(resourceNumber);


        /*-----analiza zawartości opcji BLOCK-----------------------------------------------------------------------------*/
        if (blockOptionValue != 255) { // inne wartosci sa krótsze od 16B
          blockOptionValue = (blockOptionValue & 0x07);
        }
        /*-----koniec analizy zawartości opcji BLOCK-----------------------------------------------------------------------------*/


        /*-----analiza zawartości opcji OBSERVE-----------------------------------------------------------------------------*/
        /* 0 - jeżeli dany klient nie znajduje się na liście obserwatorów to go dodajemy */
        /* 1 -  jeżeli dany klienta znajduje sie na liscie obserwatorów to usuwamy go */

        uint8_t observatorIndex = MAX_OBSERVATORS_COUNT + 1;
        if ( observeOptionValue != 2 ) {
          /* opcja OBSERVE wystąpiła w żądaniu */
          if ( observeOptionValue == 0 || observeOptionValue == 1) {
            /* sprawdzanie, czy dany zasób może być obserwowany */
            if ( (resources[resourceNumber].flags & 0x01) == 1 ) {
              /* zasób może być obserwowany */

              /* szukam klienta na liście zapisanych obserwatorów observators[] */
              observatorIndex = checkIfClientIsObserving(resourceNumber);

              /* klient jest na liscie obserwatorow */
              if ( observatorIndex != MAX_OBSERVATORS_COUNT + 1) {
                Serial.println(F("[RECEIVE][COAP][GET][observe]->client is on the list"));
                Serial.println(observatorIndex);

                if ( observeOptionValue == 1 ) {
                  /* usuń klienta z listy obserwatorów - zmień status na wolny */
                  Serial.println(F("[RECEIVE][COAP][GET][observe]->delete client from observers"));

                  observators[observatorIndex].details = (observators[observatorIndex].details | 0x80);
                }
                if ( observeOptionValue == 0 ) {
                  /* nic nie robimy - klient podtrzymuje chec obserwowania */
                  Serial.println(F("[RECEIVE][COAP][GET][observe]->update"));
                }
              }
              /* klienta nie ma na liscie obserwatorow */
              else {
                Serial.println(F("[RECEIVE][COAP][GET][observe]->client is not on the list"));

                if ( observeOptionValue == 1 ) {
                  /* wyslij blad - not register z kodem 4.00 BAD_REQUEST*/
                  sendErrorResponse(BAD_REQUEST, "OBSERVER NOT FOUND");
                  return;
                }
                if ( observeOptionValue == 0 ) {
                  /* szukamy pierwszego wolnego miejsca aby go zapisać */
                  for ( uint8_t index = 0; index < MAX_OBSERVATORS_COUNT; index++) {
                    if ( observators[index].details >= 128) {
                      /* znalezlismy wolne miejsce na liscie obserwatorow */
                      observatorIndex = index;

                      Serial.println(F("[RECEIVE][COAP][GET][observe]->Add new observer:"));
                      Serial.println(observatorIndex);

                      observators[observatorIndex].ipAddress = remoteIp;
                      observators[observatorIndex].portNumber = remotePortNumber;
                      observators[observatorIndex].tokenLen = parser.parseTokenLen(ethMessage);
                      tokenCopy(observators[observatorIndex].token, parser.parseToken(ethMessage, parser.parseTokenLen(ethMessage)), observators[observatorIndex].tokenLen);

                      observators[observatorIndex].resource = &resources[resourceNumber];
                      for ( int i = 0; i < MAX_ETAG_CLIENT_COUNT; i++) {
                        observators[observatorIndex].etagsKnownByTheObservator[i] = 0;
                      }
                      observators[observatorIndex].etagCounter = 0;
                      observators[observatorIndex].details = 0;

                      //testowanie
                      observators[observatorIndex].etagCounter++;
                      observators[observatorIndex].etagsKnownByTheObservator[0] = &globalEtags[0];
                      break;
                    }
                  }
                  /*  jeżeli lista jest pełna to wyślij błąd INTERNAL_SERVER_ERROR  */
                  if ( observatorIndex == MAX_OBSERVATORS_COUNT + 1 ) {
                    sendErrorResponse(INTERNAL_SERVER_ERROR, "OBSERV LIST FULL");
                    return;
                  }
                }
              }
            } // sprawdzenie czy obserwowalny
            else {
              /* zasób nie może być obserwowany - wyslij blad kodem 4.00 BAD_REQUEST*/
              sendErrorResponse(BAD_REQUEST, "RESOURCE CAN NOT BE OBSERVED");
              return;
            }
          } // sprawdzenie wartości 0 1
          else {
            /* podano błędną wartość opcji OBSERVE - wyślij bład */
            sendErrorResponse(BAD_REQUEST, "WRONG OBSERVE VALUE");
            return;
          }
        } // sprawdzenie !=2
        /*-----koniec analizy opcji observe----------------------------------------------------------------------------- */


        /*-----analiza zawartości opcji ETAG-----------------------------------------------------------------------------*/
        if (  etagOptionValue != 0 ) {
          uint8_t etagIndex = MAX_ETAG_CLIENT_COUNT + 1;
          uint8_t etagGlobalIndex = MAX_ETAG_COUNT + 1;

          boolean validResponse = false;
          boolean contentWithEtagResponse = false;
          uint8_t createdSessionNumber = 0;

          /* sprawdzamy, czy dany klient jest zapisany na liście obserwatorów - tylko wtedy dzialaja etagi */
          observatorIndex = checkIfClientIsObserving(resourceNumber);

          Serial.println(F("[RECEIVE][COAP][GET]->observatorIndex"));
          Serial.println(observatorIndex);

          /* klient nie jest zapisany na liscie obserwatorow */
          if ( observatorIndex == MAX_OBSERVATORS_COUNT + 1 ) {
            /* wyslij blad bad_request */
            sendErrorResponse(BAD_REQUEST, "CLIENT IS NOT ALLOWED TO USE ETAGS");
            return;
          }

          /* klient jest zapisany na liscie obserwatorow */
          else {
            /* przeszukujemy liste etagow znanych klientowi*/
            for ( uint8_t index = 0; index < observators[observatorIndex].etagCounter; index++ ) {
              if ( observators[observatorIndex].etagsKnownByTheObservator[index]->etagId == etagOptionValue ) {
                /* znaleziono etag pasujący do żądanego */
                etagIndex = index;

                Serial.println(F("[RECEIVE][COAP][GET]->znaleziono etag pasujący do żądanego  "));
                Serial.println(etagIndex);

                /* wartość jest aktualna */
                if ( observators[observatorIndex].etagsKnownByTheObservator[etagIndex]->savedValue == resources[resourceNumber].value ) {
                  Serial.println(F("[RECEIVE][COAP][GET]->wartosc aktualna  "));

                  createdSessionNumber = addNewSessionConnectedWithConServerMessage(observatorIndex);

                  /* znaleziono wolna sesje */
                  if ( createdSessionNumber != MAX_SESSIONS_COUNT + 1 ) {
                    /* uzupełnij sesje */
                    sessions[createdSessionNumber].etag = observators[observatorIndex].etagsKnownByTheObservator[etagIndex];
                    sessions[createdSessionNumber].etag->timestamp = (millis() / 1000);
                    sessions[createdSessionNumber].sessionTimestamp = sessions[createdSessionNumber].etag->timestamp;

                    /* wysyłamy wiadomość 2.03 Valid z opcją Etag, bez payloadu i kończymy wykonywanie funkcji */
                    validResponse = true;
                  }
                  /* brak wolnej sesji */
                  else {
                    sendErrorResponse(INTERNAL_SERVER_ERROR, "TOO MUCH REQUESTS");
                    return;
                  }
                }

                /* wartosc nie jest aktualna */
                else {
                  Serial.println(F("[RECEIVE][COAP][GET]->wartosc nieaktualna  "));
                  /* szukamy, czy dany zasób ma już przypisany etag do takiej wartości jaką ma obecnie */
                  for (uint8_t globalIndex = 0; globalIndex < MAX_ETAG_COUNT; globalIndex++) {
                    if ( globalEtags[globalIndex].details < 128 ) {
                      if ( strcmp(globalEtags[globalIndex].resource->uri, resources[resourceNumber].uri ) == 0) {
                        if ( globalEtags[globalIndex].savedValue == resources[resourceNumber].value ) {
                          Serial.println(F("[RECEIVE][COAP][GET]->jest globalny etag "));
                          Serial.println(globalIndex);
                          etagGlobalIndex = globalIndex;
                        }
                      }
                    }
                  }// koniec fora

                  /* szukany zasob ma przypisany globalny etag z aktualna wartoscia */
                  if ( etagGlobalIndex != MAX_ETAG_COUNT + 1 ) {
                    Serial.println(F("[RECEIVE][COAP][GET]->0"));
                    /* sprawdzamy, czy uzytkownik wie o tym etagu */
                    boolean etagIsKnownByObservator = false;
                    for ( uint8_t index = 0; index < MAX_ETAG_CLIENT_COUNT; index++) {
                      if ( observators[observatorIndex].etagsKnownByTheObservator[index] == &globalEtags[etagGlobalIndex] ) {
                        etagIsKnownByObservator = true;
                        etagIndex = index;
                      }
                    }


                    /* mozemy przypisac etag do etagow znanych klientowi */
                    if ( observators[observatorIndex].etagCounter < MAX_ETAG_CLIENT_COUNT || etagIsKnownByObservator ) {
                      /* szukamy wolnej sesji */
                      createdSessionNumber = addNewSessionConnectedWithConServerMessage(observatorIndex);
                      Serial.println(F("[RECEIVE][COAP][GET]->1"));
                      /* znaleziono wolna sesje */
                      if ( createdSessionNumber != MAX_SESSIONS_COUNT + 1 ) {

                        if (!etagIsKnownByObservator) {
                          /* dodajemy etag do listy etagow znanych klientowi */
                          observators[observatorIndex].etagsKnownByTheObservator[observators[observatorIndex].etagCounter++] = &globalEtags[etagGlobalIndex];
                          etagIndex = observators[observatorIndex].etagCounter - 1;
                        }

                        /* uzupełnij sesje */
                        sessions[createdSessionNumber].etag = observators[observatorIndex].etagsKnownByTheObservator[etagIndex];
                        sessions[createdSessionNumber].etag->timestamp = (millis() / 1000);
                        sessions[createdSessionNumber].sessionTimestamp = sessions[createdSessionNumber].etag->timestamp;

                        /* wysylamy wiadomosc 2.05 z opcja etag wskazujaca na nowy etag przypisany klientowi */
                        contentWithEtagResponse = true;
                        Serial.println(F("[RECEIVE][COAP][GET]->2"));
                      }
                    }
                  }
                  /* szukany zasob nie ma przypisanego globalnego etaga z aktualna wartoscia */
                  else {
                    /* mozemy stworzyc nowy etag */
                    if ( (globalEtagsCounter < MAX_ETAG_COUNT) && (observators[observatorIndex].etagCounter < MAX_ETAG_CLIENT_COUNT) ) {
                      /* mozemy przypisac nowy etag do etagow znanych obserwatorowi */
                      if ( observators[observatorIndex].etagCounter < MAX_ETAG_CLIENT_COUNT ) {
                        /* szukamy wolnej sesji */
                        createdSessionNumber = addNewSessionConnectedWithConServerMessage(observatorIndex);

                        /* wolna sesja */
                        if ( createdSessionNumber != MAX_SESSIONS_COUNT + 1 ) {
                          /* stworz nowy globalny etag */
                          for ( uint8_t globalIndex = 0; globalIndex < MAX_ETAG_COUNT; globalIndex++) {
                            if ( globalEtags[globalIndex].details >= 128 ) {
                              etagGlobalIndex = globalIndex;

                              globalEtagsCounter++;
                              globalEtags[globalIndex].resource = &resources[resourceNumber];
                              globalEtags[globalIndex].savedValue = resources[resourceNumber].value;
                              globalEtags[globalIndex].etagId = etagIdCounter++;
                              globalEtags[globalIndex].timestamp = 0;
                              globalEtags[globalIndex].details = 0;
                              break;
                            }
                          }

                          /* przypisz adres nowego etaga do tablicy etagow skojarzonych z klientem */
                          observators[observatorIndex].etagsKnownByTheObservator[observators[observatorIndex].etagCounter++] = &globalEtags[etagGlobalIndex];
                          etagIndex = observators[observatorIndex].etagCounter - 1;

                          /* uzupełnij sesje */
                          sessions[createdSessionNumber].etag = observators[observatorIndex].etagsKnownByTheObservator[etagIndex];
                          sessions[createdSessionNumber].etag->timestamp = (millis() / 1000);
                          sessions[createdSessionNumber].sessionTimestamp = sessions[createdSessionNumber].etag->timestamp;

                          /* wysylamy wiadomosc 2.05 z opcja etag wskazujaca na nowy etag przypisany klientowi */
                          contentWithEtagResponse = true;
                        }
                      }
                    }
                  }
                }
              }
            }

            /* nie znaleziono wskazanego etaga na liscie etagow skojarzonych z klientem */
            if (etagIndex == MAX_ETAG_CLIENT_COUNT + 1) {
              sendErrorResponse(BAD_REQUEST, "ETAG NOT FOUND");
              return;
            }
          }

          /* wyslij wiadomosci zwrotne */
          if (validResponse || contentWithEtagResponse) {
            sendEtagResponse(createdSessionNumber, validResponse, blockOptionValue);
            return;
          }
          else {
            sendContentResponse(resourceNumber, true, blockOptionValue);
            return;
          }
        }
        /*-----koniec analizy opcji ETAG----------------------------------------------------------------------------- */




        /*-----analiza wiadomości URI-PATH + OBSERVE----------------------------------------------------------*/
        if ( observeOptionValue == 0 ) {
          sendContentResponse(resourceNumber, true, blockOptionValue);
          return;
        }
        /*-----koniec analizy wiadomości URI-PATH + OBSERVE-------------------------------------------------- */



        /*-----analiza wiadomości jedynie z opcją URI-PATH----------------------------------------------------------*/
        /* wyslij wiadomość NON 2.05 content z opcja CONTENT_TYPE zgodną z accept lub domyślną */
        Serial.println(F("[RECEIVE][COAP][GET]->Only Uri Path"));
        Serial.println(resources[resourceNumber].value);

        sendContentResponse(resourceNumber , false, blockOptionValue);
        return;
        /*-----koniec analizy wiadomości jedynie z opcją URI-PATH------------------------------------------------- */

      }  //if (strcmp(resources[resourceNumber].uri, uriPath) == 0)
    } //for loop (przeszukiwanie zasobu)

    /* nie znaleziono zasobu na serwerze */
    /* wiadomość zwrotna zawierająca kod błedu 4.04 NOT_FOUND */
    Serial.println(F("[RECEIVE][COAP][GET]->Resource not found"));
    sendErrorResponse(NOT_FOUND, "RESOURCE NOT FOUND");
  }
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
void receiveEmptyRequest() {
  Serial.println(F("[RECEIVE][COAP]->receiveEmptyRequest"));

  /* przeszukujemy liste sesji */
  for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
    if (sessions[sessionNumber].details < 128) {
      /* sesja zajęta */
      if ( sessions[sessionNumber].ipAddress == remoteIp ) {
        if ( sessions[sessionNumber].portNumber == remotePortNumber ) {
          /* znaleźliśmy sesję związana z danym klientem */
          if ( parser.parseType(ethMessage) == TYPE_ACK ) {
            /* zmień status sesji na wolny */
            Serial.println(F("[RECEIVE][COAP]->kasuje sesji:"));
            resources[5].value = (double)(resources[5].value * 0,9) + (double)((double)(millis() - sessions[sessionNumber].sessionTimestamp) * 0,1);
            sessions[sessionNumber].details = 128;
            return;
          }
          if ( parser.parseType(ethMessage) == TYPE_RST ) {
            /* szukamy obserwatora w sesji*/
            uint8_t observatorIndex = checkIfClientIsObserving((sessions[sessionNumber].etag->resource->flags & 0x1c) - 1);

            /* znalezlismy klienta na liscie obserwatorow */
            if (observatorIndex != MAX_OBSERVATORS_COUNT + 1) {
              /* usuwamy klienta z listy obserwatorów */
              observators[observatorIndex].details = 128;

              /* zmień status sesji na wolny */
              sessions[sessionNumber].details = 128;
            }
          }
        }
      }
    }
  }
}



/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej kod błedu oraz payload diagnostyczny.
    -jesli żądanie typu CON - odpowiedz typu ACK
    -jesli żądanie typu NON - odpowiedz typu NON
    -jesli w żądaniu jest token to go przepisujemy
*/
void sendErrorResponse(uint16_t errorType, char* errorMessage) {
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);

  /*------- set TYPE -------*/
  if (parser.parseType(ethMessage) == TYPE_CON) {
    builder.setType(TYPE_ACK);
  }  else  {
    builder.setType(TYPE_NON);
  }

  /*------- set CODE -------*/
  uint16_t codeClass = (errorType / 100);
  builder.setCodeClass(codeClass);
  builder.setCodeDetail(errorType - (codeClass * 100));

  /*------- set MESSAGE ID -------*/
  if (parser.parseType(ethMessage) == TYPE_CON) {
    builder.setMessageId(parser.parseMessageId(ethMessage));
  } else {
    builder.setMessageId(messageId++);
  }

  /*------- set TOKEN -------*/
  uint8_t tokenLen = parser.parseTokenLen(ethMessage);
  byte* token = parser.parseToken(ethMessage, tokenLen);
  builder.setToken(token, tokenLen);

  builder.setPayload(errorMessage);
  sendEthernetMessage(builder.build(), builder.getResponseSize(), remoteIp, remotePortNumber);
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
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  /* kod wiaodmości 2.05 CONTENT */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(5);
  builder.setMessageId(messageId++);

  uint8_t tokenLen = parser.parseTokenLen(ethMessage);
  byte* token = parser.parseToken(ethMessage, tokenLen);
  builder.setToken(token, tokenLen);


  if (addObserveOption) {
    char charValueObserve[5];
    getCharValueFromResourceValue(charValueObserve, observeCounter++, 1);
    builder.setOption(OBSERVE, charValueObserve);
  }
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

  sendEthernetMessage(builder.build(), builder.getResponseSize(), remoteIp, remotePortNumber);

  Serial.println(F("[SEND][COAP][CONTENT_RESPONSE]->END"));
}
/*
   tak jak poprzednia funkcja z następującymi wyjątkami:
   nie ma argumentu payload, ponieważ jest on w pewnym sensie stały (well-know-core)
*/
void sendWellKnownContentResponse(uint8_t blockValue) {
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

  builder.setOption(CONTENT_FORMAT, LINK_FORMAT);

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
  //  Serial.println(blockSize);
  blockString[1] = '\0';
  //  Serial.println(resources[0].size);
  //  Serial.println(((blockValue & 0xf0) >> 4));
  //  Serial.println(blockSize * ((blockValue & 0xf0) >> 4));
  //  Serial.println(resources[0].size - blockSize * ((blockValue & 0xf0) >> 4));
  if (resources[0].size > blockSize * (((blockValue & 0xf0) >> 4) + 1)) {
    // 8 - ustawiamy 4. najmłodszy bit na 1, jeśli jest jeszcze cos do wysłania
    blockString[0] = (blockValue & 0xf7) + 8;
  } else {
    blockString[0] = (blockValue & 0xf7);
  }
  ///  Serial.println(blockValue & 0xf7);
  //Serial.println(blockString[0], BIN);
  builder.setOption(BLOCK2, blockString);


  setWellKnownCorePayload((blockValue & 0xf0) >> 4, blockSize);


  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(builder.build(), builder.getResponseSize(), remoteIp, remotePortNumber);
}
/**
   Metoda odpowiedzialna za wysylanie wiadomosci zwrotnej na wiadomosc zawierajaca Etag
   - wysylana jest wiadomosc 2.05 content z opcja etag, observe, content_type, block i paylodem
   lub
   - wysylana jest wiaodmosc 2.03 valid z opcja etag, observe, block
*/
void sendEtagResponse(uint8_t sessionNumber, boolean isValid, uint8_t blockValue) {
  Serial.println(F("[SEND][COAP][sendEtagResponse]->START"));
  Serial.println(isValid);

  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_CON);

  builder.setCodeClass(CLASS_SUC);
  if (isValid) {
    sessions[sessionNumber].details = (sessions[sessionNumber].details & 0xfe) + 1;
    builder.setCodeDetail(3);
  }
  else {
    builder.setCodeDetail(5);
  }
  builder.setMessageId(sessions[sessionNumber].messageID);

  uint8_t tokenLen = sessions[sessionNumber].tokenLen;
  byte* token = sessions[sessionNumber].token;
  builder.setToken(token, tokenLen);

  char charEtagValue[5];
  getCharValueFromResourceValue(charEtagValue, sessions[sessionNumber].etag->etagId, 1);
  builder.setOption(ETAG, charEtagValue);
  Serial.println(sessions[sessionNumber].etag->etagId);

  char charObserveValue[5];
  getCharValueFromResourceValue(charObserveValue, observeCounter++, 1);
  builder.setOption(OBSERVE, charObserveValue);
  Serial.println(observeCounter);

  if (!isValid) {
    builder.setOption(CONTENT_FORMAT, PLAIN_TEXT);
  }

  if ((blockValue & 0x07) != 7) {
    char blockString[2];
    blockString[0] = blockValue;
    blockString[1] = '\0';
    builder.setOption(BLOCK2, blockString);
  }

  if (!isValid) {
    char charPayloadValue[5];
    getCharValueFromResourceValue(charPayloadValue, sessions[sessionNumber].etag->savedValue, 0);
    builder.setPayload(charPayloadValue);
  }

  sendEthernetMessage(builder.build(), builder.getResponseSize(), remoteIp, remotePortNumber);
}
// END:CoAP_Methodes-----------------------------


// START:CoAP_Get_Methodes----------------------
void getCharValueFromResourceValue(char* charValue, unsigned long value, uint8_t requestedType) {
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
    case 1:
      /* value is already in plain text, only convert to char */
      if (value > 10) {
        charValue[0] = value / 10;
        charValue[1] = (value % 10);
        charValue[2] = '\0';
      } else {
        charValue[0] = value;
        charValue[1] = '\0';
      }
      break;
  }
}
/**
   Metoda odpowiedzialna za wyszukiwanie klienta w liście obserwatorów
   - metoda zwraca indeks elementu w tablicy observators
   - jeżeli nie znaleziono klienta, metoda zwraca wartość MAX_OBSERVATORS_COUNT + 1

   - porównywane wartości:
      - wpis aktywny (detail)
      - uri zasobu
      - adres ip i numer portu
      - wartosc tokena
*/
uint8_t checkIfClientIsObserving(int resourceNumber) {
  for (uint8_t observatorIndex = 0; observatorIndex < MAX_OBSERVATORS_COUNT; observatorIndex++ ) {
    if (  observators[observatorIndex].details < 128 ) { // wpis jest aktywny
      if ( strcmp(observators[observatorIndex].resource->uri, resources[resourceNumber].uri) == 0) {
        if ( observators[observatorIndex].ipAddress == remoteIp ) {
          if ( observators[observatorIndex].portNumber == remotePortNumber ) {
            Serial.println(F("aaaaa"));
            if ( tokenCompare(observators[observatorIndex].token, parser.parseToken(ethMessage, parser.parseTokenLen(ethMessage)), parser.parseTokenLen(ethMessage)) == 0) {
              /* znaleziono klienta na liscie obserwatorow */
              return observatorIndex;
            }
          }
        }
      }
    }
  }
  /* nie znaleziono klienta na liście obserwatorow */
  return MAX_OBSERVATORS_COUNT + 1;
}
/**
   Metoda odpowiedzialna za porównywanie tablic bajtów
   - jeżeli tablice są rowne to zwraca 0
   - jezeli tablice sa rozne to zwraca 1
*/
uint8_t tokenCompare(byte* a, byte* b, size_t originTokenLen) {
  boolean same = true;
  for ( uint8_t number = 0; number < originTokenLen; number++) {
    if (b[number] != a[number]) {
      same = false;
    }
  }

  if (same) {
    return 0;
  }
  else {
    return 1;
  }
}
/**
   Metoda kopiująca zawartość tokena
*/
void tokenCopy(byte* to, byte* from, size_t originTokenLen) {
  for ( uint8_t number = 0; number < originTokenLen; number++) {
    to[number] = from[number];
  }

  /* uzupelniam reszte miejsc zerami */
  while ( originTokenLen < MAX_TOKEN_LEN ) {
    to[originTokenLen++] = 0;
  }
}
/**
   Metoda służaca do dodawania nowej sesji zwiazanej z wiadomoscia CON wyslana przez serwer
   - zwraca numer sesji utworzonej
   - jezeli nie ma wolnej sesji to zwraca MAX_SESSIONS_COUNT + 1
*/
uint8_t addNewSessionConnectedWithConServerMessage(uint8_t observatorIndex) {
  for ( uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++ ) {
    if ( sessions[sessionNumber].details >= 128 ) {
      /* sesja wolna */

      sessions[sessionNumber].ipAddress = remoteIp;
      sessions[sessionNumber].portNumber = remotePortNumber;
      sessions[sessionNumber].messageID = messageId++;
      sessions[sessionNumber].details = 0;  // active, put, text

      tokenCopy(sessions[sessionNumber].token, observators[observatorIndex].token, observators[observatorIndex].tokenLen);
      sessions[sessionNumber].tokenLen = observators[observatorIndex].tokenLen;
      return sessionNumber;
    }
  }
  return MAX_SESSIONS_COUNT + 1;
}
// END:CoAP_Get_Methodes------------------------


// START:CoAP_WellKnown_Methodes-----------------
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
      if (setWellKnownCorePartToPayload(&freeLen, blockNumber, "obs;", blockSize)) {
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
// START:CoAP_WellKnown_Methodes-----------------
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
void receivePutRequest() {
  Serial.println(F("receivePutRequest"));
  uint8_t optionNumber = parser.getFirstOption(ethMessage, messageLen);
  char uriPath[20] = "";
  if ( optionNumber != URI_PATH ) {
    if ( optionNumber > URI_PATH || optionNumber == NO_OPTION) {
      /* pierwsza opcja ma numer większy niż URI-PATH lub wiadomośc jest bez opcji - brak wskazania zasobu */
      sendErrorResponse(BAD_REQUEST, "NO URI");
      return;
    }
    else {
      /* szukamy opcji Etag oraz Observe */
      while ( optionNumber != URI_PATH && optionNumber != NO_OPTION) {
        optionNumber = parser.getNextOption(ethMessage, messageLen);
      } // end of while loop
    } // end of else
  } // end of if (firstOption != URI_PATH)

  /* odczytujemy wartość URI-PATH */
  if ( optionNumber != NO_OPTION ) {
    strcpy(uriPath, parser.fieldValue);
  }
  else {
    /* wiadomość nie zawierala opcji URI_PATH, ale zawiera jakies inne opcje */
    sendErrorResponse(BAD_REQUEST, "NO URI");
    return;
  }

  optionNumber = parser.getNextOption(ethMessage, messageLen);
  while (optionNumber == URI_PATH) {
    strcat(uriPath, "/");
    strcat(uriPath, parser.fieldValue);
    optionNumber = parser.getNextOption(ethMessage, messageLen);
  }
  Serial.println(F("URIIII"));
  Serial.println(uriPath);
  Serial.println(optionNumber);
  Serial.println(parser.fieldValue[0]);
  while (optionNumber != CONTENT_FORMAT)  {
    Serial.println(F("WHILE"));
    //nie ma content-format! BLAD
    if (optionNumber > CONTENT_FORMAT || optionNumber == NO_OPTION) {
      sendErrorResponse(BAD_REQUEST, "NO CONTENT-FORMAT");
      return;
    }
    optionNumber = parser.getNextOption(ethMessage, messageLen);
  }

  // sprawdzamy zawartość opcji ContentFormat
  if (parser.fieldValue[0] != PLAIN_TEXT[0]) {
    sendErrorResponse(BAD_REQUEST, "BAD content value: only text/plain allowed");
    return;
  }
  //sparsowaliśmy uri - szukamy zasobu na serwerze
  for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
    if (strcmp(resources[resourceNumber].uri, uriPath) == 0) {
      // sprawdzamy, czy na danym zasobie można zmienić stan
      if ((resources[resourceNumber].flags & 0x02) == 2) {
        // szukamy wolnej sesji
        uint8_t sessionNumber = addNewSessionConnectedWithConServerMessage(0);
        tokenCopy(sessions[sessionNumber].token, parser.parseToken(ethMessage, parser.parseTokenLen(ethMessage)), parser.parseTokenLen(ethMessage));
        sessions[sessionNumber].messageID = parser.parseMessageId(ethMessage);
        sessions[sessionNumber].sensorID = ((resources[resourceNumber].flags & 0x0c) >> 2 );
        sessions[sessionNumber].details = B00100000;
        sessions[sessionNumber].sessionTimestamp = millis();
        // szukamy opcji ContentFormat


        // sprawdzamy wartość pola Type, jeżeli jest to CON to wysyłamy puste ack, jeżeli NON to kontynuujemy
        if ( parser.parseType(ethMessage) == TYPE_CON ) {
          Serial.println(F("CON"));
          sendEmptyAckResponse(sessionNumber);
        }
        Serial.println(F("NON"));
        Serial.println(parser.parsePayload(ethMessage, messageLen)[0]);
        Serial.println(parser.parsePayload(ethMessage, messageLen)[0] - '0');
        //sendPutResponse(sessionNumber, 2);
        // wysyłamy żądanie zmiany zasobu do obiektu IoT wskazanego przez uri
        sendMessageToThing(PUT_TYPE, sessions[sessionNumber].sensorID, parser.parsePayload(ethMessage, messageLen)[0] - '0');
        return;
      } // end of if((resources[resourceNumber].flags & 0x02) == 2

      // jeżeli otrzymaliśmy wiadomośc PUT dotyczącą obiektu, którego stanu nie możemy zmienić to wysyłamy błąd
      sendErrorResponse(METHOD_NOT_ALLOWED, "Operation not permitted");

    } // end of if (strcmp(resources[resourceNumber].uri, parser.fieldValue) == 0)
  } // end of for loop (przeszukiwanie zasobów)

  // błędne uri - brak takiego zasobu na serwerze
  sendErrorResponse(NOT_FOUND, "No a such resource");
}

void sendEmptyAckResponse(uint8_t sessionNumber) {
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_ACK);
  builder.setCodeClass(CLASS_REQ);
  builder.setCodeDetail(DETAIL_EMPTY);
  builder.setMessageId(sessions[sessionNumber].messageID);
  uint8_t tokenLen = parser.parseTokenLen(ethMessage);
  byte* token = parser.parseToken(ethMessage, tokenLen);
  builder.setToken(token, tokenLen);

  sendEthernetMessage(builder.build(), builder.getResponseSize(), sessions[sessionNumber].ipAddress, sessions[sessionNumber].portNumber);
}

/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi do klienta związanej z żądaniem PUT.
    - wersja: DEFAULT_SERVER_VERSION;
    - typ: NON
    - kod wiadomości: 2.04 suc
*/
void sendPutResponse(uint8_t sessionNumber, uint8_t value) {
  Serial.println(F("sendPutResponse"));
  builder.init();
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  // kod wiadomości 2.04
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(DETAIL_CHANGED);


  builder.setToken(sessions[sessionNumber].token, sessions[sessionNumber].tokenLen);
  builder.setMessageId(messageId++);
  builder.setOption(CONTENT_FORMAT, PLAIN_TEXT);
  char valueChar[2];
  valueChar[0] = value + '0';
  valueChar[1] = '\0';
  builder.setPayload(valueChar);
  sendEthernetMessage(builder.build(), builder.getResponseSize(), sessions[sessionNumber].ipAddress, sessions[sessionNumber].portNumber);

  // zmieniamy sesję z aktywnej na nieaktywną
 sessions[sessionNumber].details = ((sessions[sessionNumber].details ^ 0x80));
}

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
  Serial.println(message, BIN);
  if ( ((message & 0xc0) >> 6) == RESPONSE_TYPE ) {
    Serial.println(F("RESPONSE_TYPE"));
    for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
      // odnajdujemy sensorID w liście zasobów serwera
      if ( ((message & 0x38) >> 3) == ((resources[resourceNumber].flags & 0x1c) >> 2) ) {
        // aktualizujemy zawartość value w zasobie
        if( ((message & 0x38) >> 3) == 1 ){
          /* aktualizacja znacznika czasowego zwiazanego z lampka */
          resources[resourceNumber].value = millis() % 10;
          Serial.println(F("aktualizujemy zawartość value w zasobie"));
          Serial.println(resources[resourceNumber].value);
        }
        else {
          /* aktualizacja wartosci pozostalych zasobow */
          resources[resourceNumber].value = (message & 0x07);
        }

        // przeszukujemy tablicę sesji w poszukiwaniu sesji związanej z danym sensorID
        // jeżeli sesja jest aktywna i posiada sensorID równe sensorID z wiadomości to przekazujemy ją do analizy
        for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
         Serial.println(sessionNumber);
          Serial.println(sessions[sessionNumber].details);
          if ( ((sessions[sessionNumber].details & 0x80) < 128)
               && ((sessions[sessionNumber].sensorID == ((message & 0x38) >> 3))) ) {
                Serial.println(F("IFIFIFIFIIFFIFIIF"));
            if ( ((sessions[sessionNumber].details & 0x60) >> 5) == 1 ) {
              // PUT
              Serial.println(F("getMessageFromThing->PUT"));
              Serial.println(message & 0x07);
              
              sendPutResponse(sessionNumber, message & 0x07);

              /* zmień stan sesji na wolny */
              sessions[sessionNumber].details = (sessions[sessionNumber].details | (1 << 7));
            }
          }
        }

        /* jeżeli zasob jest obserwowalny, to wyslij do wszystkich obserwatorow wiadomosc o zmianie stanu */
        if ((resources[resourceNumber].flags & 0x02 >> 1) == 1 ){
            Serial.println(F("**{start}"));
            
            uint8_t globalEtagIndex = MAX_ETAG_COUNT + 1;
            uint8_t createdSessionNumber = 0;
            /* przeszukuje listę obserwatorow w poszukiwaniu aktywnych klientow */
            for ( uint8_t observatorIndex=0; observatorIndex < MAX_OBSERVATORS_COUNT; observatorIndex++ ){
              if ( observators[observatorIndex].resource == &resources[resourceNumber] ){
                Serial.println(F("**{obserwator}"));
                Serial.println(observatorIndex);
                
                /* sprawdz, czy klient ma znany etag z ta wartoscia */
                boolean etagFound = false;

                /* przeszukiwanie lokalnej tablicy etagow obserwatora */
                for ( uint8_t obsEtagIndex=0; obsEtagIndex < MAX_ETAG_CLIENT_COUNT; obsEtagIndex++ ){
                  if ( observators[observatorIndex].etagsKnownByTheObservator[obsEtagIndex]-> savedValue == resources[resourceNumber].value){
                     Serial.println(F("**{obserwator zna etaga}"));
                     Serial.println(observators[observatorIndex].etagsKnownByTheObservator[obsEtagIndex]->etagId);
                    
                    /* klient zna etaga, z obecna wartoscia - wysylamy 2.03 valid*/
                    etagFound = true;
                    
                    /* szukamy wolnej sesji */
                    createdSessionNumber = addNewSessionConnectedWithConServerMessage(observatorIndex);

                    /* znaleziono wolna sesje */
                    if ( createdSessionNumber != MAX_SESSIONS_COUNT + 1 ) {
                      /* uzupełnij sesje */
                      sessions[createdSessionNumber].etag = observators[observatorIndex].etagsKnownByTheObservator[obsEtagIndex];
                      sessions[createdSessionNumber].etag->timestamp = (millis() / 1000);
                      sessions[createdSessionNumber].sessionTimestamp = sessions[createdSessionNumber].etag->timestamp;

                      /* wysylamy wiadomosc 2.03 valid z aktywnym etagiem */
                      sendEtagResponse(createdSessionNumber, true, 255);
                      break;
                    }
                    /* brak wolnej sesji */
                    else {
                      /* wysylamy wiadomosc 2.05 z nowa wartoscia znacznika, opcja content type i observe */
                      sendContentResponse(resourceNumber, true, 255); 
                      break;
                    }
                  }
                }

                /* brak znanego etaga obserwatorowi */
                if(!etagFound) {
                  /* przeszukaj liste globalnych etagow */
                  for ( uint8_t globalIndex=0; globalIndex < MAX_ETAG_COUNT; globalIndex++ ){
                    if ( globalEtags[globalIndex].savedValue == resources[resourceNumber].value ) {
                      Serial.println(F("**{znaleziono globalny etag}"));
                      Serial.println(globalIndex);
                      
                      /* znaleziono etag na liscie globalnej */
                      globalEtagIndex = globalIndex;

                      /* sprawdzamy, czy klient ma miejsce na dodanie nowego etaga */
                      if ( observators[observatorIndex].etagCounter < MAX_ETAG_CLIENT_COUNT ){
                        /* szukamy wolnej sesji */
                        createdSessionNumber = addNewSessionConnectedWithConServerMessage(observatorIndex);

                        /* znaleziono wolna sesje */
                        if ( createdSessionNumber != MAX_SESSIONS_COUNT + 1 ) {
                            Serial.println(F("**{znaleziono globalny etag}{znaleziono wolna sesje}"));
                            Serial.println(createdSessionNumber);
                          
                          /* stworz nowy wpis na liscie znanych etagow klientowi */
                          observators[observatorIndex].etagsKnownByTheObservator[globalEtagsCounter++] = &globalEtags[globalIndex];
                          
                          /* uzupełnij sesje */
                          sessions[createdSessionNumber].etag = &globalEtags[globalIndex];
                          sessions[createdSessionNumber].etag->timestamp = (millis() / 1000);
                          sessions[createdSessionNumber].sessionTimestamp = sessions[createdSessionNumber].etag->timestamp;
    
                          /* wysylamy wiadomosc 2.05 content z nowym etagiem i paylodem */
                          sendEtagResponse(createdSessionNumber, false, 255);
                          break;
                        }
                        /* brak wolnej sesji */
                        else {
                          /* wysylamy wiadomosc 2.05 z nowa wartoscia znacznika, opcja content type i observe */
                          sendContentResponse(resourceNumber, true, 255); 
                          break;
                        }
                      }
                      /* brak miejsca u klienta */
                      else {
                        Serial.println(F("**brak miejsca w lokalnej tablicy etaga}"));
                        sendContentResponse(resourceNumber, true, 255);
                        break;
                      }                   
                    }
                  }

                  /* brak etaga na liscie globalnej */
                  if ( globalEtagIndex == MAX_ETAG_COUNT + 1){
                    Serial.println(F("**{brak etaga na liscie globalnej}"));
                    /*sprawdzamy, czy jest miejsce na liscie globalnej */
                    
                    /* mamy miejsce */
                    if ( globalEtagsCounter < MAX_ETAG_COUNT ){
                      /* dodajemy nowy wpis do listy */
                      globalEtags[globalEtagsCounter].resource = &resources[resourceNumber];
                      globalEtags[globalEtagsCounter].savedValue = resources[resourceNumber].value;
                      globalEtags[globalEtagsCounter].etagId = etagIdCounter++;
                      globalEtags[globalEtagsCounter].timestamp = 0;
                      globalEtags[globalEtagsCounter].details = 0;
  
                      globalEtagIndex = globalEtagsCounter;
                      globalEtagsCounter++;

                      Serial.println(F("**{ dodajemy nowy wpis do listy globalnej }"));

                      /* sprawdzamy, czy klient ma miejsce na dodanie nowego etaga */
                      if ( observators[observatorIndex].etagCounter < MAX_ETAG_CLIENT_COUNT ){
                        /* szukamy wolnej sesji */
                        createdSessionNumber = addNewSessionConnectedWithConServerMessage(observatorIndex);
                        
                        /* znaleziono wolna sesje */
                        if ( createdSessionNumber != MAX_SESSIONS_COUNT + 1 ) {
                          Serial.println(F("**{ dodajemy nowy wpis do listy globalnej }"));
                          
                          /* stworz nowy wpis na liscie znanych etagow klientowi */
                          observators[observatorIndex].etagsKnownByTheObservator[observators[observatorIndex].etagCounter++] = &globalEtags[globalEtagIndex];
                          
                          /* uzupełnij sesje */
                          sessions[createdSessionNumber].etag = &globalEtags[globalEtagIndex];
                          sessions[createdSessionNumber].etag->timestamp = (millis() / 1000);
                          sessions[createdSessionNumber].sessionTimestamp = sessions[createdSessionNumber].etag->timestamp;
    
                          /* wysylamy wiadomosc 2.05 content z nowym etagiem i paylodem */
                          sendEtagResponse(createdSessionNumber, false, 255);
                          break;
                        }
                        /* brak wolnej sesji */
                        else {
                          /* wysylamy wiadomosc 2.05 z nowa wartoscia znacznika, opcja content type i observe */
                          sendContentResponse(resourceNumber, true, 255); 
                          break;
                        }
                    }
                    /* klient nie ma miejsca na dodanie nowego etaga */
                    else{
                      /* wysylam wiadomosc 2.05 contenet bez etaga*/
                      Serial.println(F("**{ klient nie ma miejsca na dodanie nowego etaga }"));
                      sendContentResponse(resourceNumber, true, 255);
                    }
                  }
                  /* brak miejsca */
                    else {
                      Serial.println(F("**{ brak miejsca w globalnych  }"));
                      sendContentResponse(resourceNumber, true, 255);
                      break;
                    }
                  }
                }
                                
              }
            }           
          
          Serial.println(F("**{koniec}"));
        }
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
  Serial.println(value, BIN);
  byte message;
  message = ( message | ((type & 0x03) << 6) );
  message = ( message | ((sensorID & 0x07) << 3) );
  message = ( message | (value & 0x07) );
  Serial.println(message, BIN);
  sendRF24Message(message);
}
// END:Thing_Methodes



