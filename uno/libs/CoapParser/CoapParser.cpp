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
uint16_t CoapParser::parseMessageId(byte* message)
{
//	Serial.println(F("--[PARSER][parseMessageId]:"));
	//Serial.println(message[2],BIN);
//	Serial.println(message[3],BIN);
	//Serial.println(message[2]& 0xff);
//Serial.println((uint16_t)((message[2]& 0xff) << 8));
//uint16_t a = (message[2]& 0xff) << 8;
//	Serial.println(((message[2]& 0xff) << 8)  + message[3]);
//	a += message[3];
//	Serial.println(F("--[PARSER][parseMessageId]:END"));
	
  return ((uint16_t)((message[2]& 0xff) << 8)  + message[3]);
 //return a;
}

// zwraca wskaznik na tablice, w ktorej zapisany jest Token, 
//należy zczytać jego wartość przed wywołaniem kolejnej metody zwracającej char*
byte* CoapParser::parseToken(byte* message, uint8_t tokenLen) 
{   
  uint8_t byteNumber;
  //Serial.println(F("PARSER : tokenLEn"));
//	Serial.println(tokenLen);
	
	//Serial.println(F("PARSER : MESSAGE"));
	//Serial.println(message[4], BIN);
	//Serial.println(message[5], BIN);
   for (byteNumber = 0; byteNumber < tokenLen; byteNumber++) {
      fieldValue[byteNumber] = message[4+byteNumber];
	//  Serial.println(F("PARSER : iter"));
	//	Serial.println(message[4+byteNumber], BIN);
   }
   fieldValue[byteNumber] = '\0';
  
   
//  Serial.println(F("--[PARSER][parseToken]:"));
//	Serial.println(fieldValue);
//	Serial.println(F("--[PARSER][parseToken]:END"));
   
   return fieldValue;
}


//255 - brak opcji
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint8_t CoapParser::getFirstOption(char* message, uint8_t messageLen) {
  _currentOptionStart = 4 + parseTokenLen(message);
  _lastOptionType = 0;
  _payloadStart = 0;
  return getNextOption(message, messageLen);
}

//255 - brak opcji
//zwraca typ opcji, wartosc wyciagamy z pola fieldValue
uint8_t CoapParser::getNextOption(char* message, uint8_t messageLen) {  
  //sprawdzam czy sa jakies opcje/payload
  Serial.println(F("CoapParser::getNextOption START"));
  if (messageLen <= _currentOptionStart)
    return 0;
	//Serial.println(F("Po ifie"));
   // Serial.println(_currentOptionStart);
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
	  _optionLen = optionLen & 0xff;
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
	  _optionLen = optionLen & 0xff;
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
      }
      fieldValue[i] = '\0';// jesli dlugosc opcji = 0 to zwrocona wartosc 
	  //opcji tez bedzie 0 (oczekiwane zachowanie dla opcji accept)
      _currentOptionStart += 1 + optionLen + offset;
	  _optionLen = optionLen & 0xff;
	   _lastOptionType += type;
	    //Serial.println(_currentOptionStart);
		// Serial.println(_optionLen);
		//  Serial.println(_lastOptionType);
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


byte* CoapParser::parsePayload(byte* message, uint8_t messageLen) {
  for (int i = 4 + parseTokenLen(message); i < messageLen; i++) {
	  Serial.println(F("parsePayload"));
	  Serial.println(i);
	  Serial.println(message[i]);
    if (message[i] == 255) {
		Serial.println(F("parsePayload - > IFFFF"));
        Serial.println(i);
        Serial.println(message[i + 1]);
		fieldValue[0] = message[i + 1];
		fieldValue[1] = '\0';
		return fieldValue;
	}
  }
  fieldValue[0] = '\0';
  return fieldValue;
}

uint8_t CoapParser::getPayloadSize(char* message) {
  return (strlen(message) - _payloadStart);
}

uint8_t CoapParser::getOptionLen() {
  return _optionLen;
}


