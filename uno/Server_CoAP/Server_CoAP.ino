#include <Resources.h>
#include <CoapParser.h>
#include <CoapBuilder.h>

/*
  CoapServer.ino
  Created in 2017 by:textValue
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
IPAddress ip = (192, 168, 1, 1);
short localPort = 1237;
const uint8_t MAX_BUFFER = 100; //do zastanowienia
char ethMessage[MAX_BUFFER];

// CoAP variables:
Resource resources[RESOURCES_COUNT];
Session sessions[MAX_SESSIONS_COUNT];
Observator observators[MAX_OBSERVATORS_COUNT];

uint32_t observeCounter = 0; // globalny licznik związany z opcją observe - korzystamy z niego, gdy serwer generuje wiadomość notification
Etag globalEtags[MAX_ETAG_COUNT]; // globalna lista etagow
uint32_t globalEtagCounter = 2;  // 0 i 1 zarezerwowane dla rejestracji

CoapParser parser = CoapParser();
CoapBuilder builder = CoapBuilder();
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
  resources[1].uri = "sensor/lamp";
  resources[1].resourceType = "Lamp";
  resources[1].interfaceDescription = "state";
  strcpy(resources[0].textValue, "0"); //OFF
  resources[1].flags = B00000010;

  // przycisk
  resources[2].uri = "sensor/button";
  resources[2].resourceType = "Button";
  resources[2].interfaceDescription = "state";
  strcpy(resources[1].textValue, "0");
  resources[2].flags = B00000101;

  // metryka PacketLossRate
  resources[3].uri = "metric/PLR";
  resources[3].resourceType = "PacketLossRate";
  resources[3].interfaceDescription = "value";
  strcpy(resources[2].textValue, "0"); //OFF
  resources[3].flags = B00000000;

  // metryka ByteLossRate
  resources[4].uri = "metric/BLR";
  resources[4].resourceType = "ByteLossRate";
  resources[4].interfaceDescription = "value";
  strcpy(resources[4].textValue, "0");
  resources[4].flags = B00000000;

  // metryka MeanAckDelay
  resources[5].uri = "metric/MAD";
  resources[5].resourceType = "MeanACKDelay";
  resources[5].interfaceDescription = "value";
  strcpy(resources[5].textValue, "0");
  resources[5].flags = B00000000;

  // well-known.core
  resources[0].uri = ".well-known/core";
  resources[0].resourceType = "";
  resources[0].interfaceDescription = "";
  Resource resources[RESOURCES_COUNT];
  strcpy(resources[5].textValue, "0");
  resources[0].flags = B00000000;
  
  // wysyłamy wiadomości żądające podania aktualnego stanu zapisanych zasobów
  // TO_DO: trzeba tutaj dorobić pętlę, jeżeli zaczyna sie od /sensor/ to wyslij geta
  sendMessageToThing(GET_TYPE, ( (resources[1].flags & 0x1c) >> 2), 0);
}

// End:Resources--------------------------------


// RF24:Methodes--------------------------------
/*
    Metoda odpowiedzialna za inicjalizację parametrów interfejsu radiowego:
    - przypisujemy identyfikator interfejsu radiowego oraz numer kanału, na którym będzie pracował;
*/
void initializeRF24Communication() {
  radio.begin();
  network.begin(RF_CHANNEL, THIS_NODE_ID);
}
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
    Metoda odpowiedzialna za inicjalizację parametrów interfejsu ethernetowego:
    - przypisujemy adres MAC, adres IP oraz numer portu;
*/
void initializeEthernetCommunication() {
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
}
/*
    Metoda odpowiedzialna za odbieranie wiadomości z interfejsu ethernetowego
    - jeżeli pakiet ma mniej niż 4 bajty (minimalną wartośc nagłówka) to odrzucamy go;
    - jeżeli pakiet ma przynajmniej 4 bajty, zostaje poddany dalszej analizie;
*/
void receiveEthernetMessage() {
  int packetSize = Udp.parsePacket(); //the size of a received UDP packet, 0 oznacza nieodebranie pakietu
  if (packetSize) {
    if (packetSize >= 4) {
      Udp.read(ethMessage, MAX_BUFFER);
      (ethMessage, Udp.remoteIP(), Udp.remotePort());
    }
  }
}
/*
    Metoda odpowiedzialna za wysyłanie wiadomości poprzez interfejs ethernetowy:
    - message jest wiadomością, którą chcemy wysłać
    - ip jest adresem ip hosta, do którego adresujemy wiadomość np:IPAddress adres(10,10,10,1)
    - port jest numerem portu hosta, do którego adresuemy wiadomość
*/
void sendEthernetMessage(char* message, IPAddress ip, uint16_t port) {
  size_t messageSize = strlen(message);
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
void getCoapClienMessage(char* message, IPAddress ip, uint16_t port) {
  if (parser.parseVersion(message) != 1) {
    sendErrorResponse(ip, port, message, BAD_REQUEST, "WRONG VERSION TYPE");
    return;
  }
  else if ( parser.parseCodeClass(message) != CLASS_REQ) {
    sendErrorResponse(ip, port, message, BAD_REQUEST, "WRONG CLASS TYPE");
    return;
  }

  switch (parser.parseCodeDetail(message)) {
    case DETAIL_EMPTY:
      if ( (parser.parseType(message) == TYPE_ACK) || (parser.parseType(message) == TYPE_RST) ) {
        receiveEmptyRequest(message, ip, port);
        return;
      }
      break;
    case DETAIL_GET:
      if ( (parser.parseType(message) == TYPE_CON) || (parser.parseType(message) == TYPE_NON) ) {
        receiveGetRequest(message, ip, port);
        return;
      }
      break;
    case DETAIL_PUT:
      if ( (parser.parseType(message) == TYPE_CON) || (parser.parseType(message) == TYPE_NON) ) {
        receivePutRequest(message, ip, port);
        return;
      }
      break;
    default:
      sendErrorResponse(ip, port, message, BAD_REQUEST, "WRONG DETAIL CODE");
      return;
  }

  /* wyślij błąd oznaczający zły typ widomości do danego kodu detail */
  sendErrorResponse(ip, port, message, BAD_REQUEST, "WRONG TYPE OF MESSAGE");
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
void receiveGetRequest(char* message, IPAddress ip, uint16_t portNumber) {
  char etagOptionValues[ETAG_MAX_OPTIONS_COUNT][8];
  int etagValueNumber = 0;
  uint8_t etagCounter = "";  // JAK ZROBIC TO NA LISCIE ABY MOC ROBIC KILKA ETAGOW
  char observeOptionValue[3] = {'0', '0', '2'}; // klient może wysłac jedynie 0 lub 1
  char uriPath[255] = "";
  uint16_t acceptOptionValue = 0;

  /*---wczytywanie opcji-----------------------------------------------------------------------------*/
  uint8_t optionNumber = parser.getFirstOption(message);
  if ( optionNumber != URI_PATH ) {
    if ( optionNumber > URI_PATH ) {
      /* pierwsza opcja ma numer większy niż URI-PATH - brak wskazania zasobu */
      sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "NO URI");
      return;
    }
    else {
      /* szukamy opcji Etag oraz Observe */
      while ( optionNumber != URI_PATH ) {
        if ( optionNumber == ETAG ) {
          /* wystąpiła opcja ETAG, zapisz jej zawartość */
          strcpy(etagOptionValues[etagValueNumber++], parser.fieldValue);
        }
        if ( optionNumber == OBSERVE ) {
          /* wystąpiła opcja OBSERVE, zapisz jej zawartość */
          strcpy(observeOptionValue, parser.fieldValue);
        }
        if ( optionNumber > URI_PATH || optionNumber == NO_OPTION ) {
          /* brak opcji URI-PATH - brak wskazania zasobu */
          sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "NO URI");
          return;
        }
        optionNumber = parser.getNextOption(message);
      } // end of while loop
    } // end of else
  } // end of if (firstOption != URI_PATH)

  /* odczytujemy wartość URI-PATH */
  strcpy(uriPath, parser.fieldValue);
  while(optionNumber = parser.getNextOption(message) == URI_PATH) {
    strcat(uriPath, parser.fieldValue);
  }

  /* przeglądamy dalsze opcje wiadomości w celu odnalezienia opcji ACCEPT */
  while ( optionNumber != NO_OPTION ) {
    if ( optionNumber == ACCEPT ) {
      /* wystąpiła opcja ACCEPT, zapisz jej zawartość */
      acceptOptionValue = parser.fieldValue;
      break;
    }
  }
  /*---koniec wczytywania opcji-----------------------------------------------------------------------------*/

  /*---zabronione kombinacje opcji---*/
  /*---koniec zabronionych kombinacji opcji---*/

  /*----analiza wiadomości---- */
  /* szukamy wskazanego zasobu na serwerze */
  for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
    /* sprawdzamy, czy wskazanym zasobem jest well.known/core*/
    if ( strcmp(resources[0].uri, uriPath ) == 0 ) {
      /* tworzymy specjalny payload zawierający odpowiedz, w ktorej zawarte sa opisy zasobow */
      char payload[256];
      strcat(payload, "</.well-known/core>,");
      for (uint8_t index = 1; index < RESOURCES_COUNT; index++ ) {
        strcat(payload, "</");
        strcat(payload, resources[index].uri);
        strcat(payload, ">;");

        strcat(payload, "rt=\"");
        strcat(payload, resources[index].resourceType);
        strcat(payload, "\";");

        strcat(payload, "title=\"");
        strcat(payload, resources[index].interfaceDescription);
        strcat(payload, "\",");
      }
      strcat(payload, "</shutdown>");

      /* wysyłamy wiadomosc zwrotna z ladunkiem */
      sendContentResponse(ip, portNumber, "0", payload, false);
    }

    if ( strcmp(resources[resourceNumber].uri, uriPath) == 0) {
      /* wskazany zasób znajduje się na serwerze */
      
      /* zmienne mówiące o pozycji w listach */
      uint8_t observatorIndex = MAX_OBSERVATORS_COUNT + 1;
      uint8_t etagIndex = MAX_ETAG_COUNT + 1;

      /*-----analiza zawartości opcji OBSERVE-----------------------------------------------------------------------------*/
      if ( observeOptionValue != 2 ) {
        /* opcja OBSERVE wystąpiła w żądaniu */
        if ( observeOptionValue == "0" || observeOptionValue == "1") {
          /* 0 - jeżeli dany klient nie znajduje się na liście obserwatorów to go dodajemy */
          /* 1 -  jeżeli dany klienta znajduje sie na liscie obserwatorów to usuwamy go */

          /* sprawdzanie, czy dany zasób może być obserwowany */
          if ( (resources[resourceNumber].flags & 0xfe) == 1 ) {
            /* zasób może być obserwowany */
            /* sprawdzenie, czy dany klient nie znajduje się już na liście obserwatorów dla danego zasobu */
            boolean alreadyExist = false;
            for ( observatorIndex = 0; observatorIndex < MAX_OBSERVATORS_COUNT; observatorIndex++ ) {
              if ( strcmp(observators[observatorIndex].resource->uri, resources[resourceNumber].uri)) {
                if ( observators[observatorIndex].details < 128 ) {
                  /* dany wpis jest oznaczony jako aktywny */
                  if ( observators[observatorIndex].ipAddress == ip ) {
                    if ( observators[observatorIndex].portNumber == portNumber ) {
                      if ( observeOptionValue == "1" ) {
                        /* usuń klienta z listy obserwatorów - zmień status na wolny */
                        observators[observatorIndex].details = (observators[observatorIndex].details | 0x80);
                        break;
                      } else {
                        /* sprawdzamy, czy zmieniła się wartość tokena */
                        if ( strcmp(observators[observatorIndex].token, parser.parseToken(message, parser.parseTokenLen(message))) == 0) {
                          /* dany klient jest już zapisany na liście obserwatorów */
                          alreadyExist = true;
                          break;
                        }
                        else {
                          /* aktualizujemy numer tokena przypisanego do danego obserwatora */
                          strcpy(observators[observatorIndex].token, parser.parseToken(message, parser.parseTokenLen(message)));
                          alreadyExist = true;
                          break;
                        }
                      }
                    }
                  }
                }
              }
            } // koniec fora

            /* jeżeli dany klient nie znajdował się na liście, to szukamy pierwszego wolnego miejsca aby go zapisać */
            if (!alreadyExist && observeOptionValue == "0") {
              for ( observatorIndex = 0; observatorIndex < MAX_OBSERVATORS_COUNT; observatorIndex++) {
                if ( observators[observatorIndex].details >= 128) {
                  /* jeżeli jest jeszcze miejsce na liście obserwatorów, to dopisz klienta */
                  observators[observatorIndex].ipAddress = ip;
                  observators[observatorIndex].portNumber = portNumber;
                  strcpy(observators[observatorIndex].token, parser.parseToken(message, parser.parseTokenLen(message)));
                  observators[observatorIndex].details = (observators[observatorIndex].details & 0x7f);
                  observators[observatorIndex].resource = &resources[resourceNumber];
                  alreadyExist = true;
                  break;
                }
              }

              /*  jeżeli lista jest pełna to wyślij błąd INTERNAL_SERVER_ERROR  */
              if (observatorIndex == MAX_OBSERVATORS_COUNT - 1 ) {
                sendErrorResponse(ip, portNumber, ethMessage, INTERNAL_SERVER_ERROR, "OBSERV LIST FULL");
                return;
              }
            }
          }
          else {
            /* zasób nie może być obserwowany  - pomijamy opcje, wiadomośc zwrotna nie będzie zawierać opcji observe */
            strcpy(observeOptionValue, "2");
          }
        }
        else {
          /* podano błędną wartość opcji OBSERVE - wyślij bład */
          sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "WRONG OBSERVE VALUE");
          return;
        }
      }
      /*-----koniec analizy wartości observe----------------------------------------------------------------------------- */

      char value;
        
      /*-----analiza wartości parametrów ETAG----------------------------------------------------------------------------- */
      if (  etagValueNumber != 0 ) {
        /* sprawdzamy, czy dany klient jest zapisany na liście obserwatorów */
        observatorIndex = checkIfClientIsObserving(message, ip, portNumber, observatorIndex, resourceNumber);


        /* jeżeli obserwator znalazł się, to na pewno jego index jest mniejszy niż maksymalna pojemność listy */
        if ( observatorIndex < MAX_OBSERVATORS_COUNT) {
          /* klient jest zapisany na liscie obserwatorów  - analizujemy wartośc opcji etag */
          for (int parsedEtagNumber = 0; parsedEtagNumber < etagValueNumber; parsedEtagNumber++) {
            /* znajdź wartość etaga w liscie etagów przypisanych do danego obserwatora */
            for ( etagIndex = 0; etagIndex < MAX_ETAG_COUNT; etagIndex++ ) {
              if ( strcmp(observators[observatorIndex].etagsKnownByTheObservator[etagIndex]->etagId, etagOptionValues[parsedEtagNumber]) == 0 ) {
                /* znaleziono etag pasujący do żądanego */
                /* sprawdz, czy dana wartość jest nadal aktualna */
                if (observators[observatorIndex].etagsKnownByTheObservator[etagIndex]->savedValue == resources[resourceNumber].textValue ) {
                  /* wartość związana z etagiem jest nadal aktualna */
                  /* szukamy wolnej sesji - sesja potrzebna ze względu na wiadomość zwrtoną typu CON */
                  for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
                    if ((sessions[sessionNumber].details & 0x80) == 128) {
                      /* sesja wolna */
                      sessions[sessionNumber].ipAddress = ip;
                      sessions[sessionNumber].portNumber = portNumber;
                      sessions[sessionNumber].messageID = ++messageId;
                      strcpy(sessions[sessionNumber].token, observators[observatorIndex].token);
                      sessions[sessionNumber].etag.savedValue = etagOptionValues[parsedEtagNumber];
                      sessions[sessionNumber].details = 0;  // active, put, text

                      /* aktualizujemy wartość stempla czasowego danego etaga (liczony w sekundach)*/
                      observators[observatorIndex].etagsKnownByTheObservator[etagIndex]->timestamp = (millis() / 1000);

                      /* wysyłamy wiadomość 2.03 Valid z opcją Etag, bez payloadu i kończymy wykonywanie funkcji */
                      sendValidResponse(&sessions[sessionNumber]);
                      return;
                    }
                  }

                  /* Brak wolnej sesji - wyslij odpowiedź z kodem błedu */
                  sendErrorResponse(ip, portNumber, ethMessage, INTERNAL_SERVER_ERROR, "TOO MUCH REQUESTS");
                  return;

                }
                else {
                  /* wartośc związana z danym etagiem nie jest już aktualna */

                  /* szukamy, czy dany zasób ma już przypisany etag do takiej wartości jaką ma obecnie */
                  for (int globalIndex = 0; globalIndex < MAX_ETAG_COUNT; globalIndex++) {
                    if ( strcmp(globalEtags[globalIndex].resource->uri, resources[resourceNumber].uri ) == 0) {
                      if ( globalEtags[globalIndex].savedValue == resources[resourceNumber].textValue ) {
                        /* znaleziono globalny etag skojarzony z danym zasobem i danym stanem zasobu */

                        /* przeszukujemy listę etagów, ktore są znane obserwatorowi w celu znalezienia aktualnego etaga */
                        for ( etagIndex = 0; etagIndex < MAX_ETAG_COUNT; etagIndex++ ) {
                          if (strcmp(observators[observatorIndex].etagsKnownByTheObservator[etagIndex]->etagId, globalEtags[globalIndex].etagId) == 0) {
                            /* znaleźliśmy etaga aktualnego w liście etagów znanych klientówi */

                            /* szukamy wolnej sesji - sesja potrzebna ze względu na wiadomość zwrtoną typu CON */
                            for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
                              if ((sessions[sessionNumber].details & 0x80) == 128) {
                                /* sesja wolna */
                                sessions[sessionNumber].ipAddress = ip;
                                sessions[sessionNumber].portNumber = portNumber;
                                sessions[sessionNumber].messageID = ++messageId;
                                strcpy(sessions[sessionNumber].token, observators[observatorIndex].token);
                                sessions[sessionNumber].etag.savedValue = globalEtags[globalIndex].savedValue;
                                sessions[sessionNumber].details = 0;  // active, put, text

                                /* aktualizujemy wartość stempla czasowego danego etaga (liczony w sekundach)*/
                                observators[observatorIndex].etagsKnownByTheObservator[etagCounter]->timestamp = (millis() / 1000);

                                /* wysyłamy wiadomość 2.05 content z opcją observe, z opcją etag oraz z ładunkiem (CON)*/
                                sendContentResponseWithEtag(&sessions[sessionNumber]);
                                return;
                              }
                            }
                            /* Brak wolnej sesji - wyslij odpowiedź z kodem błedu */
                            sendErrorResponse(ip, portNumber, ethMessage, INTERNAL_SERVER_ERROR, "TOO MUCH REQUESTS");
                            return;
                          }
                        }

                        /* nie ma etaga aktualnego na liście etagów znanych klientowi - wysyłamy wiadomość 2.05 Content z aktualnym etagiem jako opcją*/
                        /* sprawdzamy, czy do danego klienta możemy jeszcze dodać nowy etag */
                        if ( observators[observatorIndex].etagCounter < MAX_ETAG_CLIENT_COUNT ) {
                          /* najpierw trzeba znaleźć sesję wolną, bo jak nie będzie sesji to klient nadal nie będzie wiedział o tym etagu */
                          /* szukamy wolnej sesji - sesja potrzebna ze względu na wiadomość zwrtoną typu CON */
                          for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
                            if ((sessions[sessionNumber].details & 0x80) == 128) {
                              /* sesja wolna */

                              /* mozna dołożyć wskaźnik do tablicy obserwatora */
                              observators[observatorIndex].etagsKnownByTheObservator[++etagCounter] = &globalEtags[globalIndex];

                              sessions[sessionNumber].ipAddress = ip;
                              sessions[sessionNumber].portNumber = portNumber;
                              sessions[sessionNumber].messageID = ++messageId;
                              strcpy(sessions[sessionNumber].token, observators[observatorIndex].token);
                              sessions[sessionNumber].etag.savedValue = globalEtags[globalIndex].savedValue;
                              sessions[sessionNumber].details = 0;  // active, put, text

                              /* aktualizujemy wartość stempla czasowego danego etaga (liczony w sekundach)*/
                              observators[observatorIndex].etagsKnownByTheObservator[etagCounter]->timestamp = (millis() / 1000);

                              /* wysyłamy wiadomość 2.05 content z opcją observe, z opcją etag oraz z ładunkiem (CON)*/
                              sendContentResponseWithEtag(&sessions[sessionNumber]);
                              
                              /* zapisz wartośc sesji - potrzebne do pozniejszego wyslania wiadomosci */
                              return;
                            }
                          }
                          /* Brak wolnej sesji - wyslij odpowiedź z kodem błedu */
                          sendErrorResponse(ip, portNumber, ethMessage, INTERNAL_SERVER_ERROR, "TOO MUCH REQUESTS");
                          return;
                        }
                        else {
                          /* brak miejsca na nowy etag - zwróc wiadomość 2.05 content bez opcji etag, z opcją observe */
                          sendContentResponse(ip, portNumber, observators[observatorIndex].token, resources[resourceNumber].textValue, true);
                          return;
                        }
                      }
                    }
                  }

                  /* nie ma globalnego etaga związanego z aktualna wartością zasobu */
                  /* sprawdzamy, czy możemy dodać (lub nadpisać) element w liście globalnej etagów */
                  for (int globalIndex = 0; globalIndex < MAX_ETAG_COUNT; globalIndex++) {
                    if ( ((globalEtags[globalIndex].timestamp - (millis() / 1000)) >  ETAG_VALID_TIME_IN_SECONDS ) || (globalEtags[globalIndex].details > 127) ) {
                      /* jest wolne miejsce w tablicy */

                      /* tworzymy nowy wpis w tablicy globlnej etagów */
                      globalEtags[globalIndex].resource = &resources[resourceNumber];
                      globalEtags[globalIndex].savedValue = resources[resourceNumber].textValue;
                      globalEtags[globalIndex].etagId = ++globalEtagCounter;
                      globalEtags[globalIndex].timestamp = (millis() / 1000);
                      globalEtags[globalIndex].details = 0; //active, text

                      /* szukamy wolnej sesji - sesja potrzebna ze względu na wiadomość zwrtoną typu CON */
                      for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
                        if ((sessions[sessionNumber].details & 0x80) == 128) {
                          /* sesja wolna */

                          /* dodajemy dany wpis do lokalnej tablicy danego obserwatora */
                          observators[observatorIndex].etagsKnownByTheObservator[++etagCounter] = &globalEtags[globalIndex];

                          sessions[sessionNumber].ipAddress = ip;
                          sessions[sessionNumber].portNumber = portNumber;
                          sessions[sessionNumber].messageID = ++messageId;
                          strcpy(sessions[sessionNumber].token, observators[observatorIndex].token);
                          sessions[sessionNumber].etag.savedValue = etagOptionValues[parsedEtagNumber];
                          sessions[sessionNumber].details = 0;  // active, put, text

                          /* aktualizujemy wartość stempla czasowego danego etaga (liczony w sekundach)*/
                          observators[observatorIndex].etagsKnownByTheObservator[etagIndex]->timestamp = (millis() / 1000);

                          /* wysyłamy wiadomość 2.05 content z opcją observe, z opcją etag oraz z ładunkiem (CON)*/
                          sendContentResponseWithEtag(&sessions[sessionNumber]);
                          return;
                        }
                      }

                      /* Brak wolnej sesji - wyslij odpowiedź z kodem błedu */
                      sendErrorResponse(ip, portNumber, ethMessage, INTERNAL_SERVER_ERROR, "TOO MUCH REQUESTS");
                      return;
                    }
                  }
                  /* brak miejsca na nową wartość etag - wyślij wiadomość 2.05 content z opcją observe, z ładunkiem, bez opcji etag */
                  sendContentResponse(ip, portNumber, observators[observatorIndex].token, resources[resourceNumber].textValue, true);
                  return;
                }
              }
            }
          }
          /* nie znaleziono wartości etag na liście etagów przpisanych do danego klienta - wyślij odpowiedz z kodem błedu 4.04 NOT_FOUND */
          sendErrorResponse(ip, portNumber, ethMessage, NOT_FOUND, "ETAG NOT FOUND");
          return;
        }
        else {
          /* klient nie jest zapisany na liście obserwatorów - nie analizuj opcji etag */
          /* wyślij odpowiedz z kodem błedu 4.02 Method Not Allowed */
          sendErrorResponse(ip, portNumber, ethMessage, METHOD_NOT_ALLOWED, "YOU HAVE TO BE AN OBSERVATOR TO USE ETAG");
          return;
        }
      }
      /*-----koniec analizy wartości parametrów ETAG----------------------------------------------------------------------------- */

      /*-------analiza wiadomości z opcją acccept------------------------------------------------------------------------------*/
      if ( acceptOptionValue == 0 ) {
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
            sendErrorResponse(ip, portNumber, ethMessage, NOT_ACCEPTABLE, "UNKNOWN ACCEPT VALUE");
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
        sendContentResponse(ip, portNumber, observators[observatorIndex].token, resources[resourceNumber].textValue, true);
        return;
      }
      /*-----koniec analizy wiadomości z opcją observe i URI_PATH-------------------------------------------------- */

      /*-----analiza wiadomości jedynie z opcją URI-PATH----------------------------------------------------------*/
      /* wyslij wiadomość NON 2.05 content bez opcji, z samym paylodem w dowolnej dostępnej formie */
      sendContentResponse(ip, portNumber, parser.parseToken(message, parser.parseTokenLen(message)), resources[resourceNumber].textValue, false);
      /*-----koniec analizy wiadomości jedynie z opcją URI-PATH------------------------------------------------- */


    }  //if (strcmp(resources[resourceNumber].uri, uriPath) == 0)
  } //for loop (przeszukiwanie zasobu)

  /* nie znaleziono zasobu na serwerze */
  /* wiadomość zwrotna zawierająca kod błedu 4.04 NOT_FOUND */
  sendErrorResponse(ip, portNumber, ethMessage, NOT_FOUND, "RESOURCE NOT FOUND");
}

/**
   Metoda odpowiedzialna za wyszukiwanie klienta w liście obserwatorów
*/
int checkIfClientIsObserving(char* message, IPAddress ip, uint16_t portNumber, int observatorIndex, int resourceNumber) {
  if ( observatorIndex == (MAX_OBSERVATORS_COUNT + 1) ) {
    /*nie było opcji observe, trzeba sprawdzić czy klient jest na liscie obserwatorów*/
    for ( observatorIndex = 0; observatorIndex < MAX_OBSERVATORS_COUNT; observatorIndex++ ) {
      if ( strcmp(observators[observatorIndex].resource->uri, resources[resourceNumber].uri)) {
        if ( observators[observatorIndex].details < 128 ) {
          if ( observators[observatorIndex].ipAddress == ip ) {
            if ( observators[observatorIndex].portNumber == portNumber ) {
              if ( strcmp(observators[observatorIndex].token, parser.parseToken(message, parser.parseTokenLen(message)) ) == 0) { //DLACZEGO TOKEN MA SIE ROWNAC 0?
                /* znaleziono klienta na liście obserwatorów */
                break;
              }
            }
          }
        }
      }
    }
  }
  return observatorIndex;
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
  /* przeszukujemy liste sesji */
  for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
    if ((sessions[sessionNumber].details & 0x80) != 128) {
      /* sesja zajęta */
      if ( sessions[sessionNumber].ipAddress == ip ) {
        if ( sessions[sessionNumber].portNumber == portNumber ) {
          /* znaleźliśmy sesję związana z danym klientem */

          if ( parser.parseType(message) == TYPE_ACK ) {
            /* wiadomość typu ack */

            /* zmień status sesji na wolny */
            sessions[sessionNumber].details = (sessions[sessionNumber].details | (1 << 7));
            return;
          }

          if ( parser.parseType(message) == TYPE_RST ) {
            /* wiadomośc typu rst */

            /* szukamy obserwatora w sesji*/
            for ( int observatorIndex = 0; observatorIndex < MAX_OBSERVATORS_COUNT; observatorIndex++ ) {
              if ( observators[observatorIndex].ipAddress == ip ) {
                if ( observators[observatorIndex].portNumber == portNumber ) {
                  if ( observators[observatorIndex].token == sessions[sessionNumber].token ) {
                    /* znaleźliśmy na liście obserwatorów klienta, który wysłał RST */

                    /* usuwamy klienta z listy obserwatorów */
                    observators[observatorIndex].details = (observators[observatorIndex].details | (1 << 7));

                    /* zmień status sesji na wolny */
                    sessions[sessionNumber].details = (sessions[sessionNumber].details | (1 << 7));
                    return;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
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
  uint8_t optionNumber = parser.getFirstOption(message);
  if (optionNumber != URI_PATH) {
    if (optionNumber > URI_PATH) {
      sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "NO URI");
      return;
    } else {
      while (optionNumber != URI_PATH)  {
        //nie ma uri! BLAD
        if (optionNumber > URI_PATH || optionNumber == NO_OPTION) {
          sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "NO URI");
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
            // sesja wolna
            sessions[sessionNumber].ipAddress = ip;
            sessions[sessionNumber].portNumber = portNumber;
            strcpy(sessions[sessionNumber].token, parser.parseToken(message, parser.parseTokenLen(message)));
            sessions[sessionNumber].messageID = parser.parseMessageId(message);
            sessions[sessionNumber].sensorID = ((resources[resourceNumber].flags & 0x0c) >> 2 );
            sessions[sessionNumber].details = B00100000;  // active, put, text

            // szukamy opcji ContentFormat
            optionNumber = parser.getNextOption(message);
            while (optionNumber != CONTENT_FORMAT)  {
              //nie ma content-format! BLAD
              if (optionNumber > CONTENT_FORMAT || optionNumber == NO_OPTION) {
                sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "NO CONTENT-FORMAT");
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
                sendErrorResponse(ip, portNumber, ethMessage, METHOD_NOT_ALLOWED, "Bad CONTENT-FORMAT");
                return;
              }
            } else {
              sendErrorResponse(ip, portNumber, ethMessage, BAD_REQUEST, "0 byte length CONTENT-FORMAT");
              return;
            }

            // sprawdzamy wartość pola Type, jeżeli jest to CON to wysyłamy puste ack, jeżeli NON to kontynuujemy
            if ( parser.parseType(message) == TYPE_CON ) {
              sendEmptyAckResponse(&sessions[sessionNumber]);
            }

            // wysyłamy żądanie zmiany zasobu do obiektu IoT wskazanego przez uri
            sendMessageToThing(PUT_TYPE, sessions[sessionNumber].sensorID, parser.parsePayload(message));
            return;
          } // end of (sessions[sessionNumber].contentFormat > 127)

          // brak wolnej sesji - bład INTERNAL_SERVER_ERROR
          sendErrorResponse(ip, portNumber, ethMessage, INTERNAL_SERVER_ERROR, "Too much requests");
          return;
        } // end of for loop (przeszukiwanie sesji)
      } // end of if((resources[resourceNumber].flags & 0x02) == 2

      // jeżeli otrzymaliśmy wiadomośc PUT dotyczącą obiektu, którego stanu nie możemy zmienić to wysyłamy błąd
      sendErrorResponse(ip, portNumber, ethMessage, METHOD_NOT_ALLOWED, "Operation not permitted");

    } // end of if (strcmp(resources[resourceNumber].uri, parser.fieldValue) == 0)
  } // end of for loop (przeszukiwanie zasobów)

  // błędne uri - brak takiego zasobu na serwerze
  sendErrorResponse(ip, portNumber, ethMessage, NOT_FOUND, "No a such resource");
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
    builder.setType(TYPE_NON);
  if (parser.parseTokenLen(message) > 0) {
    builder.setToken(parser.parseToken(message, parser.parseTokenLen(message)));
    if (errorType < 500) { //server error
      builder.setCodeClass(CLASS_SERR);
    } else {
      builder.setCodeClass(CLASS_CERR);
      builder.setCodeDetail(errorType);
      builder.setMessageId(messageId++);
      builder.setPayload(errorMessage);
      sendEthernetMessage(builder.build(), ip, portNumber);
    }
  }
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
  byte response[4];
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_ACK);
  builder.setCodeClass(CLASS_REQ);
  builder.setCodeDetail(DETAIL_EMPTY);
  builder.setMessageId(session->messageID);

  strcpy(response, builder.build());
  sendEthernetMessage(response, session->ipAddress, session->portNumber);
}
/*
   Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi zawierającej potwierdzenie odebrania żądania wraz z ładunkiem.
*/
void sendPiggybackAckResponse(IPAddress ip, uint16_t portNumber, char* message, uint8_t errorType) {

}
/*
    Metoda odpowiedzialna za stworzenie i wysłanie odpowiedzi do klienta związanej z żądaniem PUT.
    - wersja: DEFAULT_SERVER_VERSION;
    - typ: NON
    - kod wiadomości: 2.04 suc
*/
void sendPutResponse(Session* session) {
  char response;
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  // kod wiadomości 2.04
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(4);

  builder.setToken(session->token);
  messageId += 1;
  builder.setMessageId(messageId);
  response = builder.build();

  sendEthernetMessage(response, session->ipAddress, session->portNumber);

  // zmieniamy sesję z aktywnej na nieaktywną
  session->details = ((session->details ^ 0x80));
}
/**

*/
void sendAckResponse(Session* session) {

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
void sendContentResponse(IPAddress ip, uint16_t portNumber, char* tokenValue, char* payloadValue, bool addObserveOption) {
  char response;
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_NON);

  /* kod wiaodmości 2.05 CONTENT */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(5);

  builder.setToken(tokenValue);
  builder.setMessageId(0);

  if (addObserveOption) {
    builder.setOption(OBSERVE, ++observeCounter);
  }

  builder.setPayload(payloadValue);

  /* stwórz wiadomość zwrotną */
  response = builder.build();

  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(response, ip , portNumber);
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
  char response;
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_CON);

  /* kod wiaodmości 2.05 CONTENT */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(5);

  builder.setToken(session->token);
  builder.setMessageId(session->messageID);
  builder.setOption(ETAG, session->etag.etagId);

  builder.setOption(OBSERVE, ++observeCounter);

  //TODO zalezc
  builder.setPayload(session->etag.savedValue);

  /* stwórz wiadomość zwrotną */
  response = builder.build();

  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(response, session->ipAddress, session->portNumber);
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
  char response;
  builder.setVersion(DEFAULT_SERVER_VERSION);
  builder.setType(TYPE_CON);

  /* kod wiaodmości 2.03 VALID */
  builder.setCodeClass(CLASS_SUC);
  builder.setCodeDetail(3);

  builder.setToken(session->token);

  builder.setMessageId(session->messageID);

  builder.setOption(ETAG, session->etag.etagId);
  builder.setOption(OBSERVE, ++observeCounter);

  /* stwórz wiadomość zwrotną */
  response = builder.build();

  /* wyślij utworzoną wiaodmość zwrotną */
  sendEthernetMessage(response, session->ipAddress, session->portNumber);
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
  if ( ((message & 0xc0) >> 6) == RESPONSE_TYPE ) {
    for (uint8_t resourceNumber = 0; resourceNumber < RESOURCES_COUNT; resourceNumber++) {
      // odnajdujemy sensorID w liście zasobów serwera
      if ( ((message & 0x38) >> 3) == ((resources[resourceNumber].flags & 0x1c) >> 2) ) {
        // aktualizujemy zawartość value w zasobie
        strcpy(resources[resourceNumber].textValue, (message & 0x07));



        // przeszukujemy tablicę sesji w poszukiwaniu sesji związanej z danym sensorID
        // jeżeli sesja jest aktywna i posiada sensorID równe sensorID z wiadomości to przekazujemy ją do analizy
        for (uint8_t sessionNumber = 0; sessionNumber < MAX_SESSIONS_COUNT; sessionNumber++) {
          if ( ((sessions[sessionNumber].details & 0x80) == 128)
               && ((sessions[sessionNumber].sensorID == ((message & 0x38) >> 3))) ) {
            if ( ((sessions[sessionNumber].details & 0x60) >> 5) == 1 ) {
              // PUT
              sendPutResponse(&sessions[sessionNumber]);

              /* zmień stan sesji na wolny */
              sessions[sessionNumber].details = (sessions[sessionNumber].details | (1 << 7));
            }
          }
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
  byte message;
  message = ( message | ((type & 0x03) << 6) );
  message = ( message | ((sensorID & 0x07) << 3) );
  message = ( message | (value & 0x07) );
  sendRF24Message(message);
}
// END:Thing_Methodes
