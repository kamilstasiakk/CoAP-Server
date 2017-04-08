/*
  CoapPraser.h - Library for parsing CoAP messages.
  Created by Kamil Stasiak, 2017.
  Implements only selected parts of the CoAP protocol.
*/
#ifndef CoapParser_h
#define CoapParser_h

#include "Arduino.h"

//nie robilem tego jako parse() i zapisywaie wartosci do pół + gettery, bo szkoda pamieci
//jesli w arduino nie ejst zla praktyka tworzenie obiektow w loopie to warto przerobic to troche - popki co widziałem
tylko biekty deklarowane w globalnych zmiennych

class CoapParser
{
  public:
    const int FIELD_MAX_LENGHT = 100;
    char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
    char fieldValue[FIELD_MAX_LENGHT];


typedef struct record Record;

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

    CoapParser();
    int CoapParser::parseVersion(char* message);
    uint8_t CoapParser::parseType(char* message);
    uint8_t CoapParser::parseTokenLen(char* message);
    uint8_t CoapParser::parseCodeClass(char* message);
    uint8_t CoapParser::parseCodeDetail(char* message);
    uint16_t CoapParser::parseMessageId(char* message);
    char* CoapParser::parseToken(char* message, uint8_t tokenLen);
    void CoapParser::parseOptions(char* message);
    uint8_t CoapParser::getOptionsCount();
    uint8_t CoapParser::getFirstOptionType();
    char* CoapParser::getFirstOptionValue();
    uint8_t CoapParser::getNextOptionType() ;
    char* CoapParser::getNextOptionValue();
    uint8_t CoapParser::getPayloadSize();
   
  private:
    uint8_t _currentOptionStart;
    uint8_t _payloadStart;
};

#endif