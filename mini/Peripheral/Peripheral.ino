/*
  Peripheral.ino
  Stworzone w 2017 przez:
    Krzysztof Kossowski,
    Kamil Stasiak,
    Piotr Kucharski;

  Zawiera:
  - obsługę bezprzewodowej komunikacji za pomocą modułu nRF24L01;
  - obsługe podłączonej lampki LED 
    (zdalne włączanie/wyłączanie lampki, odpytanie o stan lampki);
  - obsługę podłączonego przycisku monostabilnego
    (informowanie zdalnego serwera o zmianie stanu przycisku, o naciśnięciu przycisku);
*/

#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h> 

// zmienne związane z komunikacją radiową RF24;
const uint16_t THIS_NODE_ID = 0;
const uint16_t REMOTE_NODE_ID = 1;
const uint8_t RF_CHANNEL = 60;
RF24 radio(7,8);
RF24Network network(radio);

// zmienne dotyczące podłączeń sensorowych;
uint8_t lampPin = 2;
uint8_t buttonPin = 3;

// zmienne związane z własnym protokołem radiowym;
const uint8_t LAMP = 0;
const uint8_t BUTTON = 1;
const uint8_t RESPONSE = 0;
const uint8_t STATE_CHANGED = 1;

// zmienne związana z przyciskiem;
int lastPosition = HIGH;

void setup() {
  SPI.begin();
  radio.begin();
  network.begin(RF_CHANNEL, THIS_NODE_ID); 
  
  pinMode(buttonPin, INPUT);
  pinMode(lampPin, OUTPUT);
}

void loop()
{     
  /* regularne sprawdzanie połączenia radiowego */
  network.update();
  while(network.available())
  {
    /* pobieranie wiadomości odebranej przez moduł RF24 */
    byte receivedMessage;
    RF24NetworkHeader header;
    network.read(header, &receivedMessage, sizeof(receivedMessage));
    
    /*---- Przetwarzanie zapytań odebranych przez moduł radiowy dotyczący lampki ------*/
    if ((receivedMessage & 0x38) == LAMP ) {
      if (((receivedMessage & 0xc0) >> 6) == 1){
        /* wiadomość typu GET */
        SendDataToServer(RESPONSE, LAMP ,digitalRead(lampPin));
      }
      else if (((receivedMessage & 0xc0) >> 6) == 2 ) {
        /* wiadomość typu PUT */ 

        /* odczytujemy żądaną wartość lampki */
        if ((receivedMessage & 0x07) == 0 ) {
          /* zgaś lampkę i wyślij odpowiedź zwrotną zawierającą nowy stan zasobu */
          digitalWrite(lampPin, LOW);
          SendDataToServer(RESPONSE, LAMP , 0);
        }
        else if ((receivedMessage & 0x07) == 1 ) {
          /* zapal lampkę i wyślij odpowiedź zwrotną zawierającą nowy stan zasobu */
          digitalWrite(lampPin, HIGH);
          SendDataToServer(RESPONSE, LAMP , 1);
        }
      }
    }
  }
      
  /* cykliczne sprawdzanie, czy stan przycisku zostal zmieniony */
  if(digitalRead(buttonPin) != lastPosition)
  {
    /* stan przycisku uległ zmianie */      
    if (digitalRead(buttonPin) == LOW)
    {
      /* przycisk został wciśnięty - poinformuj serwer o zmianie stanu zasobu */               
      SendDataToServer(RESPONSE, BUTTON , STATE_CHANGED);
      lastPosition = LOW;
    }              
    else {
      lastPosition = HIGH;
    }
  }
}
    
/**
 * Metoda odpowiedzialma za przesyłanie informacji drogą radiową zgodnie z autorskim protokołem:
 * 
    Protokół radiowy:
    | 7  6 | 5  4  3  | 2 1 0 |
    | type | sensorID | value |

    type:     0-Response; 1-Get; 2-Put
    sensorID: 0-Lamp; 1-Button
    value:    Lamp: 0-OFF 1-ON  Button: 1-change state(clicked)
 *
 */
void SendDataToServer(uint8_t type, uint8_t sensorID, uint8_t value)
{
  byte message;
  message = ( message | ((type & 0x03) << 6) );
  message = ( message | ((sensorID & 0x07) << 3) );
  message = ( message | (value & 0x07) );
  
  RF24NetworkHeader header(REMOTE_NODE_ID);
  network.write(header, &message, sizeof(message));          
}

