/*
  Resource.h
  Created in 2017 by:
    Krzysztof Kossowski,
    Kamil Stasiak;
    Piotr Kucharski;
*/




/**
   Struktura opisująca każdy z dostępnych na serwerze zasobów.
   - uri: identyfikator zasobu
   - resourceType: typ zasobu (potrzebne do wiaodmości ./well-known)
   - interfaceDescription: opis zasobu (potrzebne do wiadomości ./well-known)

   - value: wartość zasobu
   - flags:
      - ID: identyfikator zasobu w komunikacji bezprzewodowej
      - POST: flaga mówiąca o tym, czy na danym zasobie możemy wykonać operację PUT
      - OBSERVE: flaga mówiąca o tym, czy na danym zasobie możliwy jest mechanizm obserwacji

   - observators: tablica obserwatorów zapisanych do śledzenia stanu danego zasobu
    (aktywna tylko w momencie, gdy dany zasob może być obserwowalny)
*/
struct Resource {
  char* uri;
  char* resourceType;
  char* interfaceDescription;
  char textValue[8];  // jedna z możliwych reprezentacji
  /*
     7 6 5 | 4 3 2 |   1  |    0
           |   ID  |  PUT | OBSERVE

     ID      - wartości 0-7
     PUT    - 0.cannot / 1.can
     OBSERVE - 0.cannot / 1.can
  */
  uint8_t flags;
  //Observator observators[MAX_OBSERVATORS_COUNT];

};
