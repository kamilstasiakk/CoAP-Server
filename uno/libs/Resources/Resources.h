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

//const char NO_CONTENT_FORMAT[] = "NO CONTENT-FORMAT";

const uint16_t CON_TIMEOUT = 5000;
const uint8_t RESOURCES_COUNT = 6;
const uint8_t MAX_SESSIONS_COUNT = 3;
const uint8_t MAX_ETAG_COUNT = 2; 	//globalnie
const uint8_t MAX_ETAG_CLIENT_COUNT = 2;	//u obserwatora
const uint8_t MAX_OBSERVATORS_COUNT = 2;

const uint8_t ETAG_VALID_TIME_IN_SECONDS = 128;
const uint8_t MAX_TOKEN_LEN = 8;

// error codes
const uint16_t BAD_REQUEST = 400;
const uint16_t NOT_FOUND = 404;
const uint16_t METHOD_NOT_ALLOWED = 402;
const uint16_t NOT_ACCEPTABLE = 406;
const uint16_t INTERNAL_SERVER_ERROR = 500;

const uint16_t ETAG = 4;
const uint16_t URI_PATH = 11;
const uint16_t CONTENT_FORMAT = 12;
const uint16_t ACCEPT = 17;
const uint16_t OBSERVE = 6;
const uint16_t BLOCK2 = 23;
const uint16_t NO_OPTION = 0;

const char PLAIN_TEXT[2] = {0, '\0'};
const char LINK_FORMAT[2] = {40, '\0'};
const char XML[2] = {41, '\0'};
const char JSON[2] = {50, '\0'};

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
//code details - changed
const uint8_t DETAIL_CHANGED = 4;

const uint8_t DEFAULT_SERVER_VERSION = 1;


/**
* Struktura przechowująca wszystkie informacje na temat zasobu
* - uri: identyfikator zasobu;
* - resourceType: typ zasobu;
* - interfaceDescription: opis wartości zwracanej;
* - value: przypisany stan zasobu;
* - size: rozmiar stanu zasobu (potrzebne przy opcji blokowej);
*/
struct Resource {
  char* uri;
  char* resourceType;
  char* interfaceDescription;
  unsigned long value;
  uint16_t size;
  /*
     7 6 5 | 4 3 2 |   1  |    0
           |   ID  |  PUT | OBSERVE

     ID      - wartości 0-7 (id w komunikacji radiowej)
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
   - timestamp: znacznik czasowy ostatniego użycia
   - details:
      - stan obiektu (0 aktywny ,1 wolny - można nadpisać daną strukture nową)
      - typ obiektu (json, xml lub plain text)
*/
struct Etag {
  Resource* resource;
  unsigned long savedValue;
  uint8_t etagId;
  uint8_t timestamp;

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
  byte token[MAX_TOKEN_LEN];
  size_t tokenLen;
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
      - content_type: reprezentacja zasobu

*/
struct Session {
  IPAddress ipAddress;
  uint16_t portNumber;
  
  uint16_t messageID;  // do weryfikowania, czy nasza odpowiedź została potwierdzona
  Etag* etag;
  uint16_t sessionTimestamp;
  
  byte token[MAX_TOKEN_LEN];
  size_t tokenLen;
  uint8_t sensorID;
  /*
      7     |6 5  |   4 3 2 1   |	0 		|
      STAN  | TYP | CONTENT_TYPE|	isValid  |

      STAN          - 0.active / 1.nonactive
      TYP           - 1.put / 0.conFromServer
      CONTENT_TYPE  - 50.json / 41.xml / 0.text
  */
  byte details = 128;
};
