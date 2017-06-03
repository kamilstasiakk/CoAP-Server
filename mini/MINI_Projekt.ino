#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h> 
/*
    Protokół radiowy:
    | 7  6 | 5  4  3  | 2 1 0 |
    | type | sensorID | value |

    type:     0-Response; 1-Get; 2-Put
    sensorID: 0-Lamp; 1-Button
    value:    Lamp: 0-OFF 1-ON  Button: 1-change state(clicked)

*/

//zmienne do radiowej komunikacji
const uint16_t THIS_NODE_ID = 0;
const uint16_t REMOTE_NODE_ID = 1;
RF24 radio(7,8);
RF24Network network(radio);

//Zmienne przechowujące piny
uint8_t lampPin = 2;
uint8_t buttonPin = 3;

//Zmienne przechowujące ID sensora 
const uint8_t LAMP = 0;
const uint8_t BUTTON = 1;

//Zmienne do przycisku
float now = 0.0;
unsigned timeCatched = 0;
int lastPosition = HIGH;



void setup() {
  Serial.begin(115200); 
  SPI.begin();
  radio.begin();
  network.begin(60, THIS_NODE_ID); 
  pinMode(buttonPin, INPUT);
  pinMode(lampPin, INPUT);
}

void loop()
{ 
     
     byte receivedMessage;
     
     //Metoda odpowiedzialna za aktualizację sieci
     network.update();

    //Czesc radiowa odbieranie
    while(network.available())
      {
        //Zczytywanie wiadomości odebranej przez moduł RF24
        RF24NetworkHeader header;
        network.read(header, &receivedMessage, sizeof(receivedMessage));
  
        Serial.print("Received packet: ");
        Serial.print(receivedMessage); 
      }    
    
      //Przetwarzanie zapytań odebranych przez moduł radiowy:

      //Przetwarzanie zapytania o pobranie stanu lampki
      if (((receivedMessage & 0x03) << 6) == 1 )
      SendDataToServer(0, 0 ,digitalRead(lampPin));
      
      //Przetwarzanie zapytania o włączenie lampki
      else if (((receivedMessage & 0x03) << 6) == 2 && (receivedMessage & 0x07) == 0)
      digitalWrite(lampPin, HIGH);

      //Przetwarzanie zapytania o wyłączenie lampki
      else if ((((receivedMessage & 0x03) << 6) == 2) && (receivedMessage & 0x07) == 0)
      digitalWrite(lampPin, LOW);

      
      //Warunek sprawdzajacy czy przycisk został przyciśnięty
       if(digitalRead(buttonPin) != lastPosition)
         {
          
          if (digitalRead(buttonPin) == LOW)
                { 
                  
                SendDataToServer(0, 1 ,digitalRead(buttonPin));
                lastPosition = LOW;
                }
                
           else
           lastPosition = HIGH;
        }
}
    
//Metoda odpowiedzialma za przesyłanie informacji drogą radiową
void SendDataToServer(uint8_t type, uint8_t sensorID, uint8_t value)
   {
             byte message;
             message = ( message | ((type & 0x03) << 6) );
             message = ( message | ((sensorID & 0x07) << 3) );
             message = ( message | (value & 0x07) );
             RF24NetworkHeader header(REMOTE_NODE_ID);
             network.write(header, &value, sizeof(value));          
   }

