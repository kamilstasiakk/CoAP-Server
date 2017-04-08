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
}

uint8_t CoapParser::parseType(char* message)
{
  return ((message[0] & 0x30) >> 4);
}

//tylko wartosci 0-8 poprawne
uint8_t CoapParser::parseTokenLen(char* message)
{
  return ((message[0] & 0x0f));
}


//tylko wartosci {0,2,4,5} poprawne
uint8_t CoapParser::parseCodeClass(char* message)
{
  return ((message[1] & 0xe0) >> 5);
}

uint8_t CoapParser::parseCodeDetail(char* message)
{
  return ((message[1] & 0x1f));
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
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint32_t CoapParser::getFirstOption(char* message) {
  _currentOptionStart = 4 + parseTokenLen(message);
  //TODO: sprawdzic czy nie koniec
  if (message
  uint8_t type = (uint8_t) message[currentOptionStart] & 0xf0;
  
  switch (type) {
    case 13:
      int optiontype = message[_currentOptionStart+1] + 13;
      uint32_t optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+2)
      for (int i = 0; i < optionLen; i++) {
        
      }
      return optiontype;
      break;
    case 14:
      return message[_currentOptionStart+1] << 8 + message[_currentOptionStart+2] + 269;
      break;
    case 15:
      return 0;
      break;
    default: 
      return len;
    break;
  }
}


uint32_t CoapParser::getOptionLen(char* message, uint8_t startBase, uint8_t startExtended) {
  uint8_t len = (uint8_t) (message[startBase] & 0x0f);
  
  switch (len) {
    case 13:
      return message[startExtended] + 13;
      break;
    case 14:
      return message[startExtended] << 8 + message[startExtended + 1] + 269;
      break;
    case 15:
      return 0;
      break;
    default: 
      return len;
    break;
  }
}


char* CoapParser::getFirstOptionValue() {
  
  
}

uint8_t CoapParser::getNextOptionType() {
  
}


char* CoapParser::getNextOptionValue() {
  
}

