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
}

void CoapBuilder::setVersion(uint8_t value)
{
 
  message[0] = (message[0] & 0x3f) + ((value & 0x03) << 6);
}

void CoapBuilder::setType(uint8_t value)
{

  message[0] = (message[0] & 0xcf) + ((value & 0x03) << 4);
}

void  CoapBuilder::setTokenLen(uint8_t value)
{
  message[0] = (message[0] & 0xf0) + (value & 0x0f);
}

void CoapBuilder::setCodeClass(uint8_t value)
{
  message[1] = (message[1] & 0x1f) + ((value & 0x07) << 5); 
}

void CoapBuilder::setCodeDetail(uint8_t value)
{
  message[1] = (message[1] &  0x1f) + (value & 0x1f);
}

void CoapBuilder::setMessageId(uint16_t value)
{
  message[2] = (value & 0xff00) >> 8;
  message[3] = value & 0x00ff;
}

void CoapBuilder::setToken(char* value)
{
  uint8_t tokenLen = (uint8_t) strlen(value);
  setTokenLen(tokenLen);
  uint8_t  messageLen = strlen(message);
  //shift if options or payload are set
  if (messageLen > 4) {
    //from message[messageLen - 1 + tokenLen] = message[messageLen - 1]
    //to message[messageLen - messageLen + 4 + tokenLen] = message[messageLen - messageLen + 4]
    for (uint8_t j = 1; j > messageLen - 5; j++) {
      message[messageLen - j + tokenLen] = message[messageLen - j];
    }
  } else {
    message[4 + tokenLen] = '/0';
  }
  _lastOptionStart += tokenLen;
  //set token value
  for (int i = 0; i < strlen(value); i++) {
    message[4 + i] = value[i];
  }
}

//to simplyfy operating rising option adding order is required
//payload must be set after options too
void CoapBuilder::setOption(uint32_t optionNumber, char* value)
{
  _lastOptionLen = strlen(value);
  _lastOptionNum = optionNumber;
  uint8_t optNumOffset = 0;
  uint8_t optLenOffset = 0;
  
  if (message[_lastOptionStart] != '\0') //there was some option set
  {
    optionNumber -= _lastOptionNum;
    _lastOptionStart += _lastOptionLen;
  }
  
  //writing option number 
  if (optionNumber < 13) {
      message[_lastOptionStart] = optionNumber << 4;
  } else if (optionNumber < 269){
      message[_lastOptionStart] = 13 << 4;
      message[_lastOptionStart + 1] = optionNumber - 13;
      optNumOffset = 1;
  } else {
    message[_lastOptionStart] = 14 << 4;
    message[_lastOptionStart + 1] = ((optionNumber - 269) & 0xff00) >> 8;
    message[_lastOptionStart + 2] = (optionNumber - 269) & 0x00ff;
    optNumOffset = 2;
  }
  
  //writing option length 
  if (_lastOptionLen < 13) {
      message[_lastOptionStart] += _lastOptionLen;
  } else if (_lastOptionLen < 269){
      message[_lastOptionStart] += 13;
      message[_lastOptionStart + 1 + optNumOffset] = _lastOptionLen - 13;
      optLenOffset = 1;
  } else {
    message[_lastOptionStart] += 14;
    message[_lastOptionStart + 1 + optNumOffset] = ((_lastOptionLen - 269) & 0xff00) >> 8;
    message[_lastOptionStart + 2 + optNumOffset] = (_lastOptionLen - 269) & 0x00ff;
    optLenOffset = 2;
  }
  
  //auxiliary adding to deacrease adding operations in for loop
  _lastOptionStart += optNumOffset + optLenOffset + 1;
  //writing the value of option
  for (uint8_t i = 0; i < _lastOptionLen; i++) {
    message[_lastOptionStart + i] = value[i]; 
  }
  //ending message
  message[_lastOptionStart + _lastOptionLen] = '\0';
  
  //redo autiliary adding
  _lastOptionStart -= optNumOffset + optLenOffset + 1;
  //adding option header to option value length - needed if many options are set
  _lastOptionLen += optNumOffset + optLenOffset + 1;
}

//payload must be set after setting options
void CoapBuilder::setPayload(char* value)
{
  uint8_t i;
  uint8_t messageLen = strlen(message);
  //adding payload marker
  message[messageLen ++] = 255;
  for (i = 0; i < strlen(value); i++) {
    message[messageLen + i] = value[i];
  }
  message[messageLen + i] = '\0';
  _payloadLen = strlen(value);
}

void CoapBuilder::setPayload(char* value, uint8_t start)
{
  uint8_t i;
  uint8_t messageLen = strlen(message);
  //adding payload marker
  message[messageLen ++] = 255;
  for (i = start; i < strlen(value); i++) {
    message[messageLen + i] = value[i];
  }
  message[messageLen + i] = '\0';
  _payloadLen = strlen(value);
}

//append string to payload
void CoapBuilder::appendPayload(char* value) {
	uint8_t i;
  uint8_t messageLen = strlen(message);
  for (i = 0; i < strlen(value); i++) {
    message[messageLen + i] = value[i];
  }
  message[messageLen + i] = '\0';
  _payloadLen += strlen(value);
}

void CoapBuilder::appendPayload(char* value, uint8_t len) {
	uint8_t i;
  uint8_t messageLen = strlen(message);
  for (i = 0; i < len; i++) {
    message[messageLen + i] = value[i];
  }
  message[messageLen + i] = '\0';
  _payloadLen += strlen(value);
}

void CoapBuilder::appendPayload(char* value, uint8_t start, uint8_t end) {
	uint8_t i;
  uint8_t messageLen = strlen(message);
  for (i = start; i < end; i++) {
    message[messageLen + i] = value[i];
  }
  message[messageLen + i] = '\0';
  _payloadLen += strlen(value);
}
// remove Payload from message (payload tag is removed too)
void CoapBuilder::flushPayload() {
	_payloadLen = 0;
		uint8_t i;
  for (i = 0; i < strlen(message); i++) {
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

char* CoapBuilder::build()
{
  return message;
}

