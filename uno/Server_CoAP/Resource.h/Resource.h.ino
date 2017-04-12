/*
  Resource.h
  Created by Krzysztof Kossowski, 2017.

  Struktura opisująca każdy z dostępnych na serwerze zasobów.
*/
struct record
{
   char[] uri;
   char[] rt;
   char[] if;
   uint16_t value;
   uint8_t flags;

   // zmienna details może zawierać:
   // 7 6 5 4 3  | 2 1 |    0
   //            | id  | observe
} Resource;


struct observator
{
  Resource* resource;
  char token[8];
  IPAddress ipAddress;
  uint16_t portNumber;
  
} Observator;


struct etagOption
{
  Resource* resource;
  uint16_t value;
  uint16_t etag;

  // token wyjmujemy z geta
} Etag;

