#include "Arduino.h"
#include <Ethernet.h>
#include <EthernetUdp.h>
/*
  Resource.h
  Created in 2017 by:
    Krzysztof Kossowski,
    Kamil Stasiak;
    Piotr Kucharski;
*/


const int RESOURCES_COUNT = 5;
const int MAX_SESSIONS_COUNT = 5;
const int MAX_ETAG_COUNT = 10;
const int MAX_ETAG_CLIENT_COUNT = 3;
//uint8_t MAX_OBSERVATORS_COUNT = 5;

// error codes
uint16_t BAD_REQUEST = 400;
uint16_t NOT_FOUND = 404;
uint16_t METHOD_NOT_ALLOWED = 402;
uint16_t INTERNAL_SERVER_ERROR = 500;

uint16_t ETAG = 4;
uint16_t URI_PATH = 11;
uint16_t CONTENT_FORMAT = 12;
uint16_t ACCEPT = 17;
uint16_t OBSERVE = 6;
uint16_t NO_OPTION = 0;

uint8_t PLAIN_TEXT = 0;
uint8_t XML = 41;
uint8_t JSON = 50;

// TYPE (T) fields:
const uint8_t TYPE_CON = 0;
const uint8_t TYPE_NON = 1;
const uint8_t TYPE_ACK = 2;
const uint8_t TYPE_RST = 3;

// Code fields:
//code class - request
const uint8_t CLASS_REQ = 0;
//code class - success response
const uint8_t CLASS_SUC = 2;
//code class - client error response
const uint8_t CLASS_CERR = 4;
//code class - server error response
const uint8_t CLASS_SERR = 5;

//code details - empty message
const uint8_t DETAIL_EMPTY = 0;
//code details - get
const uint8_t DETAIL_GET = 1;
//code details - post
const uint8_t DETAIL_POST = 2;
//code details - put
const uint8_t DETAIL_PUT = 3;
//code details - delete
const uint8_t DETAIL_DELETE = 4;

const uint8_t DEFAULT_SERVER_VERSION = 1;

// maksymalna liczba opcji etag możliwych w jednej wiadomości
const int ETAG_MAX_OPTIONS_COUNT = 3;
const uint8_t ETAG_VALID_TIME_IN_SECONDS = 128;

const int MAX_OBSERVATORS_COUNT = 3;

struct Resource {
  char* uri;
  char* resourceType;
  char* interfaceDescription;
  char textValue[8];  // jedna z możliwych reprezentacji
  /*
     7 6 5 | 4 3 2 |   1  |    0
           |   ID  |  PUT | OBSERVE

     ID      - wartości 0-7
     PUT    - 0.cannot / 1.can
     OBSERVE - 0.cannot / 1.can
  */
  uint8_t flags;
};


/**
   Struktura przechowująca zapisane na serwerze wartości opcji ETag
   - resource: wskaźnik na zasób na serwerze, którego dotyczy dany ETag;
   - savedValue: wartość zasobu przypisaną do danego ETaga;
   - etagId: identyfikator danego etaga;
   - timestamp: znacznik czasowy ostatniego użycia (potrzebny do mechanizmu nadpisywania)
   - details:
      - stan obiektu (0 aktywny ,1 wolny - można nadpisać daną strukture nową)
      - typ obiektu (json, xml lub plain text)

   Jeżeli currentTime-timestamp > x to zmieniamy stan na nonactive i możemyn nadpisać dany wpis
*/
struct Etag {
  Resource* resource;
  uint16_t savedValue;
  uint32_t etagId;
  uint8_t timestamp;

  // token wyjmujemy z geta
  /*
      7     |6 5 |   4 3 2 1 0   |
                 | CONTENT_TYPE  |

      STAN          - 0.active / 1.nonactive
      CONTENT_TYPE  - 50.json / 41.xml / 0.text
  */
  byte details = 128;
};

/**
   Struktura opisująca obserwatora danego obiektu
   - ipAddress: adres IP klienta obserwującego zasób;
   - portNumber: numer portu klienta obserwującego zasób;
   - token: wartość tokena identyfikująca dany zasób po stronie klienta;

   - etagsKnownByTheObservator: lista wskaźników na etagi globalne, które są znane obserwatorowi;
   - etagCounter: zmienna określająca ilość wykorzystanych miejsc w tablicy etagsKnownByTheObservator;
   - details:
      - stan struktury: 0- aktywna 1-wolna (można ją nadpisać)
*/
struct Observator {
  IPAddress ipAddress;
  uint16_t portNumber;
  char token[8];
  Resource* resource;
  Etag* etagsKnownByTheObservator[MAX_ETAG_CLIENT_COUNT];
  uint8_t etagCounter;
  /*
      7     |6 5 4 3 2 1 0 |
      STAN  |

      STAN          - 0.active / 1.nonactive
  */
  byte details = 128;

};

/**
   Struktura opisująca sesję,
   - pojawia się w momencie wysłania przez serwer wiadomości typu CON;
   - pojawia się w momencie odebrania wiadomośći PUT;

   - ipAddress: adres ip klienta;
   - portNumber: numer portu klienta;
   - messageID: potrzebne do wiadomości typu CON
   - etagValue: potrzebne do wiadomości tyou CON
   - token: 
      - potrzebne do wiadomośći zwrotnej dotyczacej wiadomosci PUT;
      - potrzebne w wiadomości CON do zidentyfikowania wiadomosci u klienta;
   
   - sensorID: potrzebne do zidentyfikowania zasobu w komunikacji bezprzewodowej (chyba niepotrzebne)

   - details:
      - stan: stan wpisu, aktywny 0, wolny 1 (możliwy do nadpisania)
      - typ: typ sesji, czy związana z wiadomością put czy mechanizmem CON
      - content_type: reprezentacja zasobu (chyba niepotrzebne)

*/
struct Session {
  IPAddress ipAddress;
  uint16_t portNumber;
  
  uint16_t messageID;  // do weryfikowania, czy nasza odpowiedź została potwierdzona
  Etag etag;
  char token[8];
  
  uint8_t sensorID;
  /*
      7     |6 5  |   4 3 2 1 0   |
      STAN  | TYP | CONTENT_TYPE  |

      STAN          - 0.active / 1.nonactive
      TYP           - 1.put / 0.conFromServer
      CONTENT_TYPE  - 50.json / 41.xml / 0.text
  */
  byte details = 128;
};
