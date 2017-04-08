/*
  CoapPraser.cpp - Library for parsing CoAP messages.
  Created by Kamil Stasiak, 2017.
  Implements only selected parts of the CoAP protocol.
*/
#include "Arduino.h"
#include "CoapParser.h"

CoapParser::CoapParser(){}

//Poprawic fukcje - przesuniecie jeszcze dodac!

//tylko wartosc 1 poprawna
int CoapParser::parseVersion(char* message)
{
  return ((message[0] & 0xb0) >> 6);
  //reply[0] = reply[0] & 0xb0;
  //reply[0] = reply[0] + (value & 0x03) ;
}

uint8_t CoapParser::parseType(char* message)
{
  return ((message[0] & 0x30) >> 4);
}

//tylko wartosci 0-8 poprawne
uint8_t CoapParser::parseTokenLen(char* message)
{
  return ((message[0] & 0x0f) >> 0);
}


//tylko wartosci {0,2,4,5} poprawne
uint8_t CoapParser::parseCodeClass(char* message)
{
  return ((message[1] & 0xe0) >> 5);
}

uint8_t CoapParser::parseCodeDetail(char* message)
{
  return ((message[1] & 0x1f) >> 0);
}


uint16_t CoapParser::parseMessageId(char* message)
{
  return ((message[2]& 0xff) << 8  + message[3]);
}

char* CoapParser::parseToken(char* message, uint8_t tokenLen) 
{
  uint8_t byteNumber;
   for (byteNumber = 0; byteNumber < tokenLen; byteNumber++) {
      fieldValue[byteNumber] = message[4+byteNumber];
   }
   fieldValue[byteNumber] = '\0';
   return fieldValue;
}

void CoapParser::parseOptions(char* message)
{
  _optionsCount = 0;
  int currentOptionStart = 4 + parseTokenLen(message);
  //while it is not equal payload marker do counting
  while (message[currentOptionStart] != 0xff) {
    _optionsCount++;
    
    currentOptionStart += message[currentOptionStart]
  }
}
//0 - brak opcji
uint8_t CoapParser::getFirstOptionType() {
  _currentOptionNumber = 4 + _tokenLen;
  if (message[currentOptionStart] == 0xff) {
    return 0;
  }
  else {
    
  }
}

char* CoapParser::getFirstOptionValue() {
  
  
}

uint8_t CoapParser::getNextOptionType() {
  
}


char* CoapParser::getNextOptionValue() {
  
}

