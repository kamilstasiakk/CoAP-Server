/*
  CoapPraser.h - Library for parsing CoAP messages.
  Created by Kamil Stasiak, 2017.
  Implements only selected parts of the CoAP protocol.
*/
#ifndef CoapBuilder_h
#define CoapBuilder_h

#include "Arduino.h"

class CoapBuilder
{
  public:
    static const int MAX_MESSAGE_LENGHT = 100;
    char message[MAX_MESSAGE_LENGHT];
    
    CoapBuilder();
    /*set fields as default
     * version = 1
     * type = non
     * token length = 0
     * msg id = 0
    */
    void init();
    //sets version field value
    void setVersion(uint8_t value);
    //sets type field value
    void setType(uint8_t value);
    //sets class subfield of code field value
    void setCodeClass(uint8_t value);
    //sets detail subfield of code field value
    void setCodeDetail(uint8_t value);
    //sets setMessageId field value
    void setMessageId(uint16_t value);
     //sets Token and TKL field value
    void setToken(char* value);
    //sets Option
    void setOption(uint32_t optionNumber, char* value);
    //sets Payload
    void setPayload(char* value);
	//appends to payload
	void appendPayload(char* value);
    char* build(); 
    char* buildAckHeader();

  private:
    
    uint32_t _lastOptionStart;
    uint32_t _lastOptionLen; // Option length with option header
    uint32_t _lastOptionNum;
    void  setTokenLen(uint8_t value);
    
};
#endif
