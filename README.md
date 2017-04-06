0 10.0.1.1
1 10.0.1.2
0 10.0.1.3
1 WdmMux.OutClientPort
127.0.0.1:8305
Listening on 127.0.0.1:8305.
Agent:Listening on 127.0.0.1:9005.
Zamosc/03:08:44/OutNetworkPort[0]/Wysyłam sygnał o średniej dlugości fali równej 0 nm
Zamosc/03:08:44/OutClientPort[1]/Wysyłam sygnał kliencki
Zamosc/03:08:45/Port wejsciowy nr 0 (sieciowy)/Odebrałem sygnał o średniej długości fali 0
is sig signal!
conf message!
response_conf;10.0.0.37;255.255.255.224;10.0.1.1;255.255.255.240
LRMZ/03:08:45/LRM/Send LocalTopology! Added new link: 10.0.1.1 --- 10.0.0.37
Zamosc/03:08:45/OutNetworkPort[0]/Wysyłam sygnał o średniej dlugości fali równej 0 nm
Zamosc/03:08:45/Port wejsciowy nr 0 (sieciowy)/Odebrałem sygnał o średniej długości fali 0
is sig signal!
conf response
LRMA/03:08:45/LRM/Send LocalTopology! Added new link: 10.0.1.3 --- 10.0.0.34
Topology
LRM/03:08:46/LRM-middle/Initializing
{"LrmMessage":null,"Topology":null,"IsRequest":false,"SourceClientIp":"","DestinationClientIp":"","Id":0,"FromWho":"Operator2","Confirmation":false,"SourceSnp":{"Ip":{"Address":"10.0.1.1","Netmask":"255.255.255.224"},"Lambda":1530.33411},"DestinationSnp":{"Ip":{"Address":"10.0.1.4","Netmask":"255.255.255.224"},"Lambda":1530.33411},"SourceSnpp":{"Ip":null},"DestinationSnpp":{"Ip":null},"Type":0,"SubnetworkConnection":null,"NtfctnType":0,"NetworkIp":null,"SoftPermanent":false}
Cntrol Agent:odebralem connReq! 10.0.1.1 10.0.1.4
Zamosc/03:08:54/CC/Odebralem ConnectionRequestIn 10.0.1.1 10.0.1.4 1530,334
lambda:1530,334
KAMILU MAGRYTO! TO NIE TAK!
Zamosc/03:08:54/Pole komutacyjne/ Dodano następujący wpis:
 port wejściowy 0 lambda we 1530,334 nm port wyjściowy 1 lambda wy 1530,334 nm
Zamosc/03:08:54/Wdm Multiplekser/dodano wpis do FIB
Zamosc/03:08:54/CC/Połączenie z InterElementsAPI.SNP do klienta zostało zestawione
Zamosc/03:08:54/Port wejsciowy nr 0 (sieciowy)/Odebrałem sygnał o średniej długości fali 0
is sig signal!
LRMZ/03:08:54/LRM/Assign snp: 10.0.0.37Lambda: 1530,334
Zamosc/03:09:37/Port wejsciowy nr 1 (kliencki)/Brak światła!!
{"LrmMessage":null,"Topology":null,"IsRequest":false,"SourceClientIp":"","DestinationClientIp":"","Id":1,"FromWho":"Operator2","Confirmation":false,"SourceSnp":{"Ip":{"Address":"10.0.1.1","Netmask":"255.255.255.224"},"Lambda":1530.33411},"DestinationSnp":{"Ip":{"Address":"10.0.1.4","Netmask":"255.255.255.224"},"Lambda":1530.33411},"SourceSnpp":{"Ip":null},"DestinationSnpp":{"Ip":null},"Type":12,"SubnetworkConnection":null,"NtfctnType":0,"NetworkIp":null,"SoftPermanent":false}
Zamosc/03:14:34/CC/Odebrałem ConnectionDeletionIn: 10.0.1.1 10.0.1.4 1530,334
Zamosc/03:14:34/CC/Nie mozna usunąć połączenia, bo LRM nie pozwala... Żartowalem... MAGRYTO!
Zamosc/03:14:34/Pole komutacyjne/ Usunięto następujący wpis:
 port wejściowy 0 lambda we 1530,334 nm port wyjściowy 1 lambda wy 1530,334 nm
Zamosc/03:14:34/Wdm Multiplekser/WDM Mux: WDM Mux: Usunięto następujący wpis z tablicy : + port we = 0lamdba we = 1530,334port wy = 1lambda wy = 1530,334
{"LrmMessage":null,"Topology":null,"IsRequest":false,"SourceClientIp":"","DestinationClientIp":"","Id":2,"FromWho":"Operator2","Confirmation":false,"SourceSnp":{"Ip":{"Address":"10.0.1.1","Netmask":"255.255.255.224"},"Lambda":1530.33411},"DestinationSnp":{"Ip":{"Address":"10.0.1.4","Netmask":"255.255.255.224"},"Lambda":1530.33411},"SourceSnpp":{"Ip":null},"DestinationSnpp":{"Ip":null},"Type":12,"SubnetworkConnection":null,"NtfctnType":0,"NetworkIp":null,"SoftPermanent":false}
Zamosc/03:16:17/CC/Odebrałem ConnectionDeletionIn: 10.0.1.1 10.0.1.4 1530,334
Zamosc/03:16:17/CC/Nie mozna usunąć połączenia, bo LRM nie pozwala... Żartowalem... MAGRYTO!
Failed!
Zamosc/03:16:17/CC/Nie mozna usunąć połączenia, bo nie ma
