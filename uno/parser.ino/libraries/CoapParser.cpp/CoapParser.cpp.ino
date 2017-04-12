/*
  CoapPraser.cpp - Library for parsing CoAP messages.
  Created by Kamil Stasiak, 2017.
  Implements only selected parts of the CoAP protocol.
*/
#include "Arduino.h"
#include "CoapParser.h"

CoapParser::CoapParser(){}

//tylko wartosc 1 poprawna
int CoapParser::parseVersion(char* message)
{
  return ((message[0] & 0xb0) >> 6);
}

//zwraca wartość pola type, wartości te opisane są jako stałe TYPE_*
uint8_t CoapParser::parseType(char* message)
{
  return ((message[0] & 0x30) >> 4);
}

//zwraca długość tokena, tylko wartosci 0-8 poprawne
uint8_t CoapParser::parseTokenLen(char* message)
{
  return ((message[0] & 0x0f));
}


//zwraca kod klasy, tylko wartosci {0,2,4,5} poprawne - stałe CLASS_*
uint8_t CoapParser::parseCodeClass(char* message)
{
  return ((message[1] & 0xe0) >> 5);
}

//zwraca dodatkowe info o kodzie - stałe DETAIL_*
uint8_t CoapParser::parseCodeDetail(char* message) 
{
  return ((message[1] & 0x1f));
}

//zwraca message ID
uint16_t CoapParser::parseMessageId(char* message)
{
  return ((message[2]& 0xff) << 8  + message[3]);
}

// zwraca wskaznik na tablice, w ktorej zapisany jest Token, 
//należy zczytać jego wartość przed wywołaniem kolejnej metody zwracającej char*
char* CoapParser::parseToken(char* message, uint8_t tokenLen) 
{
  uint8_t byteNumber;
   for (byteNumber = 0; byteNumber < tokenLen; byteNumber++) {
      fieldValue[byteNumber] = message[4+byteNumber];
   }
   fieldValue[byteNumber] = '\0';
   return fieldValue;
}


//0 - brak opcji
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint32_t CoapParser::getFirstOption(char* message) {
  _currentOptionStart = 4 + parseTokenLen(message);
  _payloadStart = 0;
  getNextOption(message);
}

//0 - brak opcji
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint32_t CoapParser::getNextOption(char* message) {  
  //sprawdzam czy sa jakies opcje/payload
  if (strlen(message) <= _currentOptionStart)
    return 0;
    
  uint8_t type = (uint8_t) message[_currentOptionStart] & 0xf0;
  
  switch (type) {
    case 13:
      int optiontype = message[_currentOptionStart+1] + 13;
      uint32_t optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+2);
      uint8_t offset = computeOptLenOffset(optionLen);
      int i;
      for (i = 0; i < optionLen; i++) {
        fieldValue[i] = message[_currentOptionStart + 2 + offset + i];
      }
      fieldValue[i] = '\0';
      _currentOptionStart += 2 + optionLen + offset;
      return optiontype;
      break;
    case 14:
      int optiontype =  message[_currentOptionStart+1] << 8 + message[_currentOptionStart+2] + 269;
      uint32_t optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+3);
      uint8_t offset = computeOptLenOffset(optionLen);
      int i;
      for (i = 0; i < optionLen; i++) {
        fieldValue[i] = message[_currentOptionStart + 3 + offset + i];
      }
      fieldValue[i] = '\0';
      _currentOptionStart += 3 + optionLen + offset;
      return optiontype;
      break;
    case 15:
      _payloadStart = _currentOptionStart + 1;
      return 0;
      break;
    default: 
      uint32_t optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+1);
      uint8_t offset = computeOptLenOffset(optionLen);
      int i;
      for (i = 0; i < optionLen; i++) {
        fieldValue[i] = message[_currentOptionStart + 1 + offset + i];
      }
      fieldValue[i] = '\0';
      _currentOptionStart += 1 + optionLen + offset;
      return type;
    break;
  }
}


private uint32_t CoapParser::getOptionLen(char* message, uint8_t startBase, uint8_t startExtended) {
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

//okreslamy przesuniecie poczatku pola wartosc opcji spowodowane extended option len
private uint32_t CoapParser::computeOptLenOffset(uint32_t optionLen) {
  if (optionLen < 13) 
        return 0;
      else if (optionLen < 269)
        return 1;
      else if 
        return 2;
}


char* CoapParser::parsePayload(char* message) {
  for (int i = _payloadStart; i < strlen(message); i++) {
    fieldValue[i - _payloadStart] = message[i];
  }
  return fieldValue;
}

uint8_t CoapParser::getPayloadSize() {
  return strlen(message) - _payloadStart;
}




