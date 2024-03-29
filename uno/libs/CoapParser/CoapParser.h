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
//tylko obiekty deklarowane w globalnych zmiennych

class CoapParser
{
  public:
	static const int FIELD_MAX_LENGHT = 15;
    byte fieldValue[FIELD_MAX_LENGHT];
	
    typedef struct record Record;

    CoapParser();
    //zwraca numer wersji
    uint8_t parseVersion(char* message);
    //zwraca wartość pola type, wartości te opisane są jako stałe TYPE_*
    uint8_t parseType(char* message);
    //zwraca długość tokena, tylko wartosci 0-8 poprawne
    uint8_t parseTokenLen(char* message);
    //zwraca kod klasy, tylko wartosci {0,2,4,5} poprawne - stałe CLASS_*
    uint8_t parseCodeClass(char* message);
    //zwraca dodatkowe info o kodzie - stałe DETAIL_*
    uint8_t parseCodeDetail(char* message);
    //zwraca message ID
    uint16_t parseMessageId(byte* message);
    // zwraca wskaznik na tablice, w ktorej zapisany jest Token, 
    //należy zczytać jego wartość przed wywołaniem kolejnej metody zwracającej char* lub
    //getOption
    byte* parseToken(byte* message, uint8_t tokenLen);
    // zwraca numer pierwszej opcji, jej wartość zapisana jest w polu fieldValue, 
    //należy zczytać wartość przed wywołaniem kolejnej metody zwracającej char* lub
    //getOption
    uint8_t getFirstOption(char* message, uint8_t messageLen);
    // zwraca numer kolejnej opcji, jej wartość zapisana jest w polu fieldValue, 
    //należy zczytać wartość przed wywołaniem kolejnej metody zwracającej char* lub
    //getOption
    uint8_t getNextOption(char* message, uint8_t messageLen) ;
    //zwraca długość pola danych
    uint8_t getPayloadSize(char* message);
    // zwraca wskaznik na tablice, w ktorej zapisany jest payload, 
    //należy zczytać jego wartość przed wywołaniem kolejnej metody zwracającej char* lub
    //getOption
    byte* parsePayload(byte* message, uint8_t messageLen);
	uint8_t getOptionLen();
  private:
	uint8_t _optionLen;
    uint8_t _currentOptionStart;
    uint8_t _payloadStart;
    uint8_t _lastOptionType;
    uint32_t getOptionLen(char* message, uint8_t startBase, uint8_t startExtended);
    uint8_t computeOptLenOffset(uint32_t optionLen);
};

#endif
