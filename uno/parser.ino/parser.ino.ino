#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008


byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

const int FIELD_MAX_LENGHT = 100;
unsigned int localPort = 8888;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
char fieldValue[FIELD_MAX_LENGHT];
EthernetUDP Udp;

const uint8_t TYPE_CON = 0;
const uint8_t TYPE_NON = 1;
const uint8_t TYPE_ACK = 2;
const uint8_t TYPE_RST = 3;

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



void setup() {
  // put your setup code here, to run once:
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
   
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  int packetSize=Udp.parsePacket();

  if(packetSize){
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    for (int i = 0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());
    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer); 

  }
}
//tylko wartosc 1 poprawna
int parseVersion(char* message)
{
  return (message[0] & 0xb0);
  //reply[0] = reply[0] & 0xb0;
  //reply[0] = reply[0] + (value & 0x03) ;
}

uint8_t parseType(char* message)
{
  return (message[0] & 0x30);
}

//tylko wartosci 0-8 poprawne
uint8_t parseTokenLen(char* message)
{
  return (message[0] & 0x0f);
}

//tylko wartosci {0,2,4,5} poprawne
uint8_t parseCodeClass(char* message)
{
  return (message[1] & 0xe0);
}

uint8_t parseCodeDetail(char* message)
{
  return (message[1] & 0x1f);
}

uint16_t parseMessageId(char* message)
{
  return (message[2]& 0xff) << 8  + message[3];
}

char* parseToken(char* message, uint8_t tokenLen) 
{
  uint8_t byteNumber;
   for (byteNumber = 0; byteNumber < tokenLen; byteNumber++) {
      fieldValue[byteNumber] = message[4+byteNumber];
   }
   fieldValue[byteNumber] = '\0';
   return fieldValue;
}

