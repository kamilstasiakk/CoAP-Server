#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h> 


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
     //Zmienna przechowująca odebraną komendę od servera. Możliwe wartości:
     // '0' - wyłączenie lampki
     // '1' - włączenie lampki
     // 'g' - pobranie stanu lampki
     char receivedCommand;
     
     //Metoda odpowiedzialna za aktualizację sieci
     network.update();

    //Czesc radiowa odbieranie
    while(network.available())
      {
        //Zczytywanie wiadomości odebranej przez moduł RF24
        RF24NetworkHeader header;
        network.read(header, &receivedCommand, sizeof(receivedCommand));
  
        Serial.print("Received packet: ");
        Serial.print(receivedCommand); 
      }    
    
      //Przetwarzanie zapytań odebranych przez moduł radiowy:

      //Przetwarzanie zapytania o pobranie stanu lampki
      if (receivedCommand == 'g')
      SendData(LAMP, digitalRead(lampPin));
      
      //Przetwarzanie zapytania o włączenie lampki
      else if (receivedCommand = '1')
      digitalWrite(lampPin, HIGH);

      //Przetwarzanie zapytania o wyłączenie lampki
      else if (receivedCommand = '0')
      digitalWrite(lampPin, LOW);

      
      //Warunek sprawdzajacy czy przycisk został przyciśnięty
       if(digitalRead(buttonPin) != lastPosition)
         {
          
          if (digitalRead(buttonPin) == LOW)
                { 
                SendData(BUTTON, 0);
                lastPosition = LOW;
                }
           else
           lastPosition = HIGH;
        }
}
    
//Metoda odpowiedzialma za przesyłanie informacji drogą radiową
void SendData(uint8_t sensor, uint8_t data)
   {
          char value;
          value = (sensor << 1) & 0x02 ;
          value += (data & 0x01 );
          //Przesyłanie informacji przez moduł RF24
          RF24NetworkHeader header(REMOTE_NODE_ID);
          network.write(header, &value, sizeof(value));          
   }

