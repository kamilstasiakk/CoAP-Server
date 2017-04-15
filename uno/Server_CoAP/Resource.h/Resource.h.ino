/*
  Resource.h
  Created in 2017 by:
    Krzysztof Kossowski,
    Kamil Stasiak;
    Piotr Kucharski;
  
  Struktura opisująca każdy z dostępnych na serwerze zasobów.
*/
struct record
{
   char[] uri;
   char[] rt;
   char[] if;
   uint16_t value;
      
   /*  
   *  7 6 5 | 4 3 2 |   1  |    0
   *        |   ID  | POST | OBSERVE   
   *  
   *  ID      - 0-7 
   *  POST    - 0.cannot / 1.can    
   *  OBSERVE - 0.cannot / 1.can
  */
   uint8_t flags;
} Resource;


struct observator
{
  Resource* resource;
  char token[8];
  IPAddress ipAddress;
  uint16_t portNumber;
  
} Observator;

struct session
{
  IPAddress ipAddress;
  uint16_t portNumber;

  
  char token[8];
  char messageID[2];  // do weryfikowania, czy nasza odpowiedź została potwierdzona
  uint8_t sensorID;
  
  /*  
   *  7     |6 5  |   4 3 2 1 0   |
   *  STAN  | TYP | CONTENT_TYPE  |   
   *  
   *  STAN          - 0.active / 1.nonactive   
   *  TYP           - 0.get / 1.post    
   *  CONTENT_TYPE  - 50.json / 41.xml / 0.text
  */
  byte details = 0;
} Session;


struct etagOption
{
  Resource* resource;
  uint16_t value;
  uint16_t etag;

  // token wyjmujemy z geta
} Etag;

uint8_t RESOURCES_COUNT = 5;
uint8_t MAX_SESSIONS_COUNT = 5;
uint8_t MAX_ETAG_COUNT = 20;
uint8_t MAX_OBSERVATORS_COUNT = 5;

// error codes
uint16_t BAD_REQUEST = 400;
uint16_t NOT_FOUND = 404;
uint16_t METHOD_NOT_ALLOWED = 402;
uint16_t INTERNAL_SERVER_ERROR = 500;

uint16_t ETAG = 4;
uint16_t URI_PATH = 11;
uint16_t CONTENT-FORMAT = 12;
uint16_t ACCEPT = 17;
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


