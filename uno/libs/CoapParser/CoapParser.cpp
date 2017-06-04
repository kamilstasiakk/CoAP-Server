/*
  CoapPraser.cpp - Library for parsing CoAP messages.
  Created by Kamil Stasiak, 2017.
  Implements only selected parts of the CoAP protocol.
*/
#include "Arduino.h"
#include "CoapParser.h"

CoapParser::CoapParser(){}

//tylko wartosc 1 poprawna
uint8_t CoapParser::parseVersion(char* message)
{
//	Serial.println(F("--[PARSER][parseVersion]:"));
//	Serial.println(message[0],BIN);
//	Serial.println(message[0] & 0xc0);
//	Serial.println((message[0] & 0xc0) >> 6);
//	Serial.println(F("--[PARSER][parseVersion]:END"));
	
  return ((message[0] & 0xc0) >> 6);
}

//zwraca wartość pola type, wartości te opisane są jako stałe TYPE_*
uint8_t CoapParser::parseType(char* message)
{
//	Serial.println(F("--[PARSER][parseType]:"));
//	Serial.println(message[0],BIN);
//	Serial.println(message[0] & 0x30);
//	Serial.println((message[0] & 0x30) >> 4);
//	Serial.println(F("--[PARSER][parseType]:END"));
	
  return ((message[0] & 0x30) >> 4);
}

//zwraca długość tokena, tylko wartosci 0-8 poprawne
uint8_t CoapParser::parseTokenLen(char* message)
{
//	Serial.println(F("--[PARSER][parseTokenLen]:"));
//	Serial.println(message[0],BIN);
//	Serial.println((message[0] & 0x0f));
//	Serial.println(F("--[PARSER][parseTokenLen]:END"));
	
  return ((message[0] & 0x0f));
}


//zwraca kod klasy, tylko wartosci {0,2,4,5} poprawne - stałe CLASS_*
uint8_t CoapParser::parseCodeClass(char* message)
{
//	Serial.println(F("--[PARSER][parseCodeClass]:"));
//	Serial.println(message[1],BIN);
//	Serial.println((message[1] & 0xe0));
//	Serial.println((message[1] & 0xe0) >> 5);
//	Serial.println(F("--[PARSER][parseCodeClass]:END"));
	
  return ((message[1] & 0xe0) >> 5);
}

//zwraca dodatkowe info o kodzie - stałe DETAIL_*
uint8_t CoapParser::parseCodeDetail(char* message) 
{
//	Serial.println(F("--[PARSER][parseCodeDetail]:"));
//	Serial.println(message[1],BIN);
//	Serial.println((message[1] & 0x1f));
//	Serial.println(F("--[PARSER][parseCodeDetail]:END"));
	
  return ((message[1] & 0x1f));
}

//zwraca message ID
uint16_t CoapParser::parseMessageId(char* message)
{
//	Serial.println(F("--[PARSER][parseMessageId]:"));
//	Serial.println(message[2],BIN);
//	Serial.println(message[3],BIN);
//	Serial.println(message[2]& 0xff);
//	Serial.println((message[2]& 0xff) << 8);
//	Serial.println((message[2]& 0xff) << 8  + message[3]);
//	Serial.println(F("--[PARSER][parseMessageId]:END"));
	
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
   Serial.println(fieldValue);
   
   	Serial.println(F("--[PARSER][parseToken]:"));
	Serial.println(fieldValue);
	Serial.println(F("--[PARSER][parseToken]:END"));
   
   return fieldValue;
}


//0 - brak opcji
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint8_t CoapParser::getFirstOption(char* message) {
  _currentOptionStart = 4 + parseTokenLen(message);
  _lastOptionType = 0;
  _payloadStart = 0;
  return getNextOption(message);
}

//0 - brak opcji
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint8_t CoapParser::getNextOption(char* message) {  
  //sprawdzam czy sa jakies opcje/payload
  if (strlen(message) <= _currentOptionStart)
    return 0;
  uint8_t type = (uint8_t) (message[_currentOptionStart] & 0xf0) >> 4;
  uint8_t offset = 0;
  uint32_t optionLen = 0;
  int optiontype = 0;
  int i;
  switch (type) {
    case 13:
      optiontype = message[_currentOptionStart+1] + 13;
      optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+2);
      offset = computeOptLenOffset(optionLen);
      int i;
      for (i = 0; i < optionLen; i++) {
        fieldValue[i] = message[_currentOptionStart + 2 + offset + i];
      }
      fieldValue[i] = '\0';
      _currentOptionStart += 2 + optionLen + offset;
	  _lastOptionType += optiontype;
      return _lastOptionType;
      break;
    case 14:
      optiontype =  message[_currentOptionStart+1] << 8 + message[_currentOptionStart+2] + 269;
      optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+3);
      offset = computeOptLenOffset(optionLen);
      for (i = 0; i < optionLen; i++) {
        fieldValue[i] = message[_currentOptionStart + 3 + offset + i];
      }
      fieldValue[i] = '\0';
      _currentOptionStart += 3 + optionLen + offset;
	  _lastOptionType += optiontype;
      return _lastOptionType;
      break;
    case 15:
      _payloadStart = _currentOptionStart + 1;
      return 0;
      break;
    default: 
      optionLen = getOptionLen(message, _currentOptionStart, _currentOptionStart+1);
      offset = computeOptLenOffset(optionLen);
      for (i = 0; i < optionLen; i++) {
        fieldValue[i] = message[_currentOptionStart + 1 + offset + i];
		
	Serial.println(F("--[PARSER][option]:"));
	Serial.println(message[_currentOptionStart + 1 + offset + i],BIN);
	Serial.println(fieldValue[i]);
	Serial.println(F("--[PARSER][options]:END"));
		
      }
      fieldValue[i] = '\0';
      _currentOptionStart += 1 + optionLen + offset;
	   _lastOptionType += type;
      return _lastOptionType;
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

//okreslamy przesuniecie poczatku pola wartosc opcji spowodowane extended option len
uint8_t CoapParser::computeOptLenOffset(uint32_t optionLen) {
  if (optionLen < 13) 
        return 0;
      else if (optionLen < 269)
        return 1;
      else
        return 2;
}


char* CoapParser::parsePayload(char* message) {
  for (int i = _payloadStart; i < strlen(message); i++) {
    fieldValue[i - _payloadStart] = message[i];
  }
  return fieldValue;
}

uint8_t CoapParser::getPayloadSize(char* message) {
  return (strlen(message) - _payloadStart);
}




