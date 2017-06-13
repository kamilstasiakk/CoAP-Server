/*
  CoapPraser.cpp - Library for parsing CoAP messages.
  Created by Kamil Stasiak, 2017.
  Implements only selected parts of the CoAP protocol.
*/
#include "Arduino.h"
#include "CoapBuilder.h"

//mozna jeszcze sprawdzanie czy nie przekroczono maksymalnej dlugosci zrobic

CoapBuilder::CoapBuilder(){
  message[4] = '\0';
  }

void CoapBuilder::init()
{
  //ver = 1, NON, no token
  message[0] = 0x50;
  //code 2.05 content
  message[1] = 0x85;
  //message id = 0
  message[2] = 0;
  message[3] = 0;
  message[4] = '\0';
  _lastOptionStart = 4;
  _messageLen = 4;
}

void CoapBuilder::setVersion(uint8_t value)
{
 
//	Serial.println(F("**[BUILDER][setVersion]:"));
//	Serial.println(message[0] & 0x3f);
//	Serial.println((value & 0x03) << 6);
	
	  message[0] = (message[0] & 0x3f) + ((value & 0x03) << 6);
	
//	Serial.println(message[0],BIN);
//	Serial.println(F("**[BUILDER][setVersion]:END"));
	

}

void CoapBuilder::setType(uint8_t value)
{
//  	Serial.println(F("**[BUILDER][setType]:"));
//	Serial.println(message[0] & 0xcf);
//	Serial.println((value & 0x03) << 4);
	
	    message[0] = (message[0] & 0xcf) + ((value & 0x03) << 4);
	
	
//	Serial.println(message[0],BIN);
//	Serial.println(F("**[BUILDER][setType]:END"));
}

void  CoapBuilder::setTokenLen(uint8_t value)
{
//    Serial.println(F("**[BUILDER][setTokenLen]:"));
//	Serial.println(message[0] & 0xf0);
//	Serial.println(value & 0x0f);
	
	message[0] = (message[0] & 0xf0) + (value & 0x0f);
	
//	Serial.println(message[0],BIN);
//	Serial.println(F("**[BUILDER][setTokenLen]:END"));
	_messageLen += value;
}

void CoapBuilder::setCodeClass(uint8_t value)
{
//    Serial.println(F("**[BUILDER][setCodeClass]:"));
//	Serial.println(message[1] & 0x1f);
//	Serial.println((value & 0x07) << 5);

	message[1] = (message[1] & 0x1f) + ((value & 0x07) << 5); 

//	Serial.println(message[1],BIN);
//	Serial.println(F("**[BUILDER][setCodeClass]:END")); 	
}

void CoapBuilder::setCodeDetail(uint8_t value)
{
//    Serial.println(F("**[BUILDER][setCodeDetail]:"));
//	Serial.println(message[1],BIN);
//	Serial.println(message[1] &  0xe0);
//	Serial.println(value & 0x1f);
	
	message[1] = (message[1] &  0xe0) + (value & 0x1f);
	
//	Serial.println(message[1],BIN);
//	Serial.println(F("**[BUILDER][setCodeDetail]:END")); 
}

void CoapBuilder::setMessageId(uint16_t value)
{ 
//    Serial.println(F("**[BUILDER][setMessageId]:"));
//	Serial.println(value & 0xff00);
//	Serial.println((value & 0xff00) >> 8);
//	Serial.println(value & 0x00ff);

  message[2] = (value & 0xff00) >> 8;
  message[3] = value & 0x00ff;	
  
 // 	Serial.println(message[2],BIN);
//	Serial.println(message[3],BIN);
//	Serial.println(F("**[BUILDER][setMessageId]:END"));
}

void CoapBuilder::setToken(byte* value, uint8_t tokenLen)
{
  setTokenLen(tokenLen);
 // Serial.println(F("[BUILDER] CoapBuilder::setToken"));
//  Serial.println(value[0], BIN);
//  Serial.println(tokenLen, BIN);
  
  //shift if options or payload are set
 // if (_messageLen > 4) {
    //from message[messageLen - 1 + tokenLen] = message[messageLen - 1]
    //to message[messageLen - messageLen + 4 + tokenLen] = message[messageLen - messageLen + 4]
   // for (uint8_t j = 1; j > _messageLen - 5; j++) {
  //    message[_messageLen - j + tokenLen] = message[_messageLen - j];
  //  }
//  } else {
    message[4 + tokenLen] = '\0';
 // }
  _lastOptionStart += tokenLen;
 // Serial.println(_lastOptionStart, BIN);
  //set token value
  for (int i = 0; i < tokenLen; i++) {
    message[4 + i] = value[i];
	//Serial.println(F("iter"));
 // Serial.println(message[4 + i], BIN);
  }
 // Serial.println(F("msg"));
 // Serial.println(message[4], BIN);
 // Serial.println(message[5], BIN);
 // Serial.println(message[6], BIN);
//  Serial.println(F("[BUILDER] CoapBuilder::setToken"));
	//Serial.println(F("**[BUILDER][setToken]:Hole Message"));	
//	Serial.println(F("***[BUILDER][setToken]:END"));
}

//to simplyfy operating rising option adding order is required
//payload must be set after options too
void CoapBuilder::setOption(uint32_t optionNumber, char* value)
{
//	Serial.println(value[0],BIN);
//	Serial.println(strlen(value));
//	Serial.println(F("SET Option VALUE"));
//	Serial.println(value);
	//Serial.println(F("SET Option VALUE BIN"));
//	Serial.println(value[0], BIN);
  
  uint8_t optNumOffset = 0;
  uint8_t optLenOffset = 0;

  if (message[_lastOptionStart] != '\0') //there was some option set
  {
    optionNumber -= _lastOptionNum;
    _lastOptionStart = _messageLen;
//	 Serial.println(F("there was some option set"));
  }
  _lastOptionLen = strlen(value);
 // Serial.println(F("BUILDER SET Option));
//	Serial.println(value[0], BIN);
  if (_lastOptionLen == 0) {
	  _lastOptionLen = 1;
  }
  _lastOptionNum = optionNumber;
  
  //writing option number 
  if (optionNumber < 13) {
	  message[_lastOptionStart] = (optionNumber << 4);
	  _messageLen += _lastOptionLen + 1;
  } else if (optionNumber < 269){
      message[_lastOptionStart] = (13 << 4);
      message[_lastOptionStart + 1] = (optionNumber - 13);
      optNumOffset = 1;
	  _messageLen += _lastOptionLen + 2;
  } else {
    message[_lastOptionStart] = 14 << 4;
    message[_lastOptionStart + 1] = (((optionNumber - 269) & 0xff00) >> 8);
    message[_lastOptionStart + 2] = ((optionNumber - 269) & 0x00ff);
    optNumOffset = 2;
	_messageLen += _lastOptionLen + 3;
  } 
  
//    Serial.println(F("**[BUILDER][setOptions]:Option nubber"));
//	Serial.println(_lastOptionStart);
//	Serial.println(message[0],BIN);
//	Serial.println(message[1],BIN);
//	Serial.println(message[2],BIN);
//	Serial.println(message[3],BIN);
//	Serial.println(message[4],BIN);
//	Serial.println(F("**[BUILDER][setOptions]:Option nubberEND"));
  
  
  //writing option length 
  if (_lastOptionLen < 13) {
      message[_lastOptionStart] += _lastOptionLen;
  } else if (_lastOptionLen < 269){
      message[_lastOptionStart] += 13;
      message[_lastOptionStart + 1 + optNumOffset] = _lastOptionLen - 13;
      optLenOffset = 1;
	  _messageLen++;
  } else {
    message[_lastOptionStart] += 14;
    message[_lastOptionStart + 1 + optNumOffset] = ((_lastOptionLen - 269) & 0xff00) >> 8;
    message[_lastOptionStart + 2 + optNumOffset] = (_lastOptionLen - 269) & 0x00ff;
    optLenOffset = 2;
	_messageLen +=2;
  }
  
  //auxiliary adding to deacrease adding operations in for loop
  _lastOptionStart += optNumOffset + optLenOffset + 1;
  //writing the value of option
  for (uint8_t i = 0; i < _lastOptionLen; i++) {
    message[_lastOptionStart + i] = value[i]; 
//	Serial.println(_lastOptionStart + i);

//	Serial.println(F("---------------------"));
//	Serial.println(value[0]);
//	Serial.println(_lastOptionLen);
//	Serial.println(value[1]);
//	Serial.println(*value);
//	Serial.println(value);
//	Serial.println(_lastOptionLen);
//	Serial.println(F("add value"));
  }
  //ending message
  message[_lastOptionStart + _lastOptionLen] = '\0';
  
  //redo autiliary adding
  _lastOptionStart -= optNumOffset + optLenOffset + 1;
  //adding option header to option value length - needed if many options are set
  _lastOptionLen += optNumOffset + optLenOffset + 1;
  
  
//  	Serial.println(F("**[BUILDER][setOptions]:Hole Message"));	
//	Serial.println(message[0],BIN);
//	Serial.println(message[1],BIN);
//	Serial.println(message[2],BIN);
//	Serial.println(message[3],BIN);
//	Serial.println(message[4],BIN);	
//	Serial.println(message[5],BIN);	
//	Serial.println(F("**[BUILDER][setOptions]:END"));
}

//payload must be set after setting options
void CoapBuilder::setPayload(char* value)
{
  uint8_t i;	
  //adding payload marker
  message[_messageLen ++] = 255;
  
 // Serial.println(F("strlen(value)"));
//  Serial.println(strlen(value));
 // Serial.println(F("value"));
 // Serial.println(value);
  
  
  for (i = 0; i < strlen(value); i++) {
    message[_messageLen + i] = value[i];
  }
  _messageLen += strlen(value);
  
  //message[_messageLen++] = '\0';
  _payloadLen = strlen(value);
//  Serial.println(_messageLen);
  
//   Serial.println(F("**[BUILDER][setPayload1]:Hole Message"));	
//	Serial.println(message[0],BIN);
//	Serial.println(message[1],BIN);
//	Serial.println(message[2],BIN);
//	Serial.println(message[3],BIN);
//	Serial.println(message[4],BIN);	
//	Serial.println(message[5],BIN);
//	Serial.println(message[6],BIN);
//	Serial.println(message[7],BIN);
//	Serial.println(message[8],BIN);
//	Serial.println(message[9],BIN);
//	Serial.println(F("**[BUILDER][setPayload1]:END"));
}

void CoapBuilder::setPayload(char* value, uint8_t start)
{
  uint8_t i;
  //adding payload marker
  message[_messageLen ++] = 255;
  for (i = start; i < strlen(value); i++) {
    message[_messageLen + i] = value[i];
  }
  message[_messageLen + i] = '\0';
  _payloadLen = strlen(value);
}

//append string to payload
void CoapBuilder::appendPayload(char* value) {
	int i;
	//Serial.println(F("BUILDER->appendPayload1 START"));
	//Serial.println(value);
	//for (int j = 0; j < _messageLen; j++) {
	//	Serial.print(message[j]);
	//}
	//Serial.println(F(""));
	//Serial.println(_payloadLen);
	//Serial.println(_messageLen);
  for (i = 0; i < strlen(value); i++) {
    message[_messageLen + i] = value[i];
	//Serial.print(value[i]);
  }
  message[_messageLen + i] = '\0';
  _messageLen += i;
  _payloadLen += strlen(value);
	//for (int j = 0; j < _messageLen; j++) {
	//	Serial.print(message[j]);
	//}
	//Serial.println(F(""));
	//Serial.println(_payloadLen);
  //Serial.println(F("BUILDER->appendPayload1 STOP"));
}

void CoapBuilder::appendPayload(char* value, uint8_t len) {
//	Serial.println(F("BUILDER->start"));
 // Serial.println(len);
 // Serial.println(_messageLen);
 // Serial.println(message[_messageLen]);
 // Serial.println(message[_messageLen + 1]);
 Serial.println(F("BUILDER->appendPayload2 START"));
	Serial.println(value);
	for (int j = 0; j < _messageLen; j++) {
		Serial.print(message[j]);
	}
	Serial.println(F(""));
	Serial.println(_payloadLen);
	Serial.println(_messageLen);
	int i;
  for (i = 0; i < len; i++) {
    message[_messageLen + i] = value[i];
	Serial.print(value[i]);
	Serial.print(message[_messageLen + i]);
  }
  Serial.println(F(""));
  message[_messageLen + i] = '\0';
  _messageLen += i;
  _payloadLen += len;
  for (int j = 0; j < _messageLen; j++) {
		Serial.print(message[j]);
	}
	Serial.println(F(""));
	Serial.println(_payloadLen);
  Serial.println(F("BUILDER->appendPayload2 STOP"));
}

void CoapBuilder::appendPayload(char* value, uint8_t start, uint8_t end) {
	uint8_t i;
	Serial.println(F("BUILDER->appendPayload3 START"));
	Serial.println(value);
	
	for (int j = 0; j < _messageLen; j++) {
		Serial.print(message[j]);
	}
	Serial.println(F(""));
	Serial.println(_payloadLen);
	Serial.println(_messageLen);
	
  for (i = start; i < end; i++) {
    message[_messageLen + i - start] = value[i];
	Serial.print(value[i]);
	Serial.print(message[_messageLen + i - start]);
  }
  Serial.println(F(""));
  message[_messageLen + i] = '\0';
  _payloadLen += (end - start);
  _messageLen += (end - start);
  for (int j = 0; j < _messageLen; j++) {
		Serial.print(message[j]);
	}
	Serial.println(F(""));
	Serial.println(_payloadLen);
  Serial.println(F("BUILDER->appendPayload3 STOP"));
}
// remove Payload from message (payload tag is removed too)
void CoapBuilder::flushPayload() {
	_messageLen -= _payloadLen;
	_payloadLen = 0;
	Serial.println(F("BUILDER->flushPayload"));
  Serial.println(_messageLen);
  Serial.println(message[_messageLen]);
  Serial.println(message[_messageLen-1]);
  Serial.println(message[_messageLen-2]);
  Serial.println(message[_messageLen+1]);
		uint8_t i;
  for (i = 0; i <_messageLen; i++) {
    if (message[i] == 255) {
		message[i] = '\0';
		return;
  }
}
}
// remove Payload from message (payload tag is removed too)
uint8_t CoapBuilder::getPayloadLen() {
	return _payloadLen;
}

byte* CoapBuilder::build()
{
	
//	   Serial.println(F("**[BUILDER][build]:Hole Message"));	
//	size_t messageSizeTMP = strlen(message);
//	for(int i = 0; i< messageSizeTMP; i++){
//		Serial.println(message[i],BIN);
//	}	
//	Serial.println(F("**[BUILDER][build]:END"));
	
  return message;
}

size_t CoapBuilder::getResponseSize()
{
	return _messageLen;
}

/*
 * zwraca: 
 * 0 gdy są równe
 * 1 w przeciwnym wypadku
 * bajtów używamy jako rozszerzonych ASCII (aby mogly miec wartosc 0<x<256
 * byte owartosci 0 oznacza to samo co '\0' dla ascii
 */
int8_t CoapBuilder::byteArrayCompere(byte* a, byte* b, size_t len) {
  for( uint8_t number=0; number < len; number++) {
    if (a[number] == 0) {
      if (b[number] == 0) {
        return 0;
      } else {
        return 1;
      }
    }
    if (b[number] == 0) {
        return 1;
    }
    if (b[number] != a[number]) {
        return 1;
    }
  }
}

//kopiuje zawartosc tablicy from do tablicy to
void CoapBuilder::byteArrayCopy(byte* to, byte* from) {
  uint8_t number = 0;
  while (1) {
    to[number] = from[number];
    if (from[number] == 0) {
      return;
    }
    number++;
  }
}


void CoapBuilder::byteArrayCat(byte* to, byte* from) {
  uint8_t number = 0;
  uint8_t numberFrom = 0;
  while (to[number] != 0) {
    number++; 
  }
  while (1) {
    to[number] = from[numberFrom];
    if (from[numberFrom] == 0) {
      return;
    }
    number++;
    numberFrom++;
  }
}


uint8_t CoapBuilder::byteArrayLen(byte* a) {
  uint8_t number = 0;
  while (a[number] != 0) {
    number++; 
  }
  return number;
}


