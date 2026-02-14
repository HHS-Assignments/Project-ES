Voor alle opdrachten geldt: 100% C of C++ code. De code moet compileerbaar zijn met een standaard C compiler zoals gcc. Zorg ervoor dat de code goed gestructureerd is en gebruik duidelijke variabelenamen. Voeg commentaar toe waar nodig om de code begrijpelijk te maken. Test de code grondig dooor middel van test cases and lat deze testen automatiseren in gheithub actions om ervoor te zorgen dat deze correct werkt en geen fouten bevat.


# Opdrachtbeschrijving Project
Casus gebouw automatisering

## 1 Inleiding
Veel gebouwen kunnen met behulp van sensoren en actuatoren effici¨enter en gebrui-
kersvriendelijker worden gemaakt, waardoor deze beter toegankelijker worden voor
mensen met bepaalde handicappen. Bovendien wordt de leefbaarheid en het conform
binnen en buiten het gebouw verbeterd.
Het doel van dit project is om een multifunctioneel gebouw bestaande uit 4 verdiepen,
waarbij elke verdieping een eigen functie heeft, te automatiseren. Met als centraal
thema het verbeteren van comfort, veiligheid, energie-effici¨entie, toegankelijkheid en
duurzaamheid. Door de multifunctionaliteit en veelzijdigheid van het gebouw zal elke
verdieping op een speciale manier geautomatiseerd moeten worden, waarbij elke verdie-
ping z’n eigen automatiseringskenmerken heeft. In de toekomst zou ook een uitbreiding
gewenst zijn om de verdiepingen met elkaar te laten communiceren, bijvoorbeeld in
noodgevallen, denk hierbij aan brand en dergelijke.
De opdracht zal volgens een agile methode worden uitgevoerd, waarbij een onderzoek
naar een selectie van (veld)bussen een belangrijk onderdeel is. Het project is grofweg
onder te verdelen in de volgende twee onderdelen.
1. In het eerste deel zal voornamelijk gefocust zijn:
• In het helder krijgen van de opdracht en het hierbij maken van een plan
van aanpak.
• In een onderzoeken naar welke bussysteem (CAN, I2C, SPI) het beste ge-
selecteerd kan worden om de onderlinge embedded-componenten, in een
gebouw automatisering, met elkaar te laten communiceren. Verder zal be-
keken worden hoe losse componenten (met een WMOS D1 bordje) kan com-
municeren met een Raspberry PI en hoe twee Raspberry PI’s met elkaar
kunnen communiceren.
• Het opstellen van product backlog’s
2. In het tweede deel worden de besluiten van deel 1 uitgevoerd door middel van
drie sprints.
Als eerst wordt de huidige situatie van de huidige installatie beschreven, waarna de
gewenste situatie beschreven staat
4
2 Het te automatiseren gebouw.
Het bedrijf L & B heeft een 4 verdiepingen kantoor gebouw aan de rand van een na-
tuurlandschap laten ombouwen naar een revalidatie oord, met een receptie, restaurant,
hotelkamers en een revalidatie verdieping.
Het idee is om het gebouw modern in te richten en hierbij de domotica leidend te
laten zijn. Waardoor het gebouw effici¨enter, in zowel personeel, energiebeheer, ... kan
worden. Bovendien zou het gebouw hierdoor gebruiksvriendelijker moeten zijn voor
mensen met een handicap.
De eerste idee¨en per verdieping wat de home-automatisering betreft zijn, worden in
de volgende hoofdstukken uitgelegd.
2.1 De entree
De entree van het gebouw is de begane grond en wordt weergegeven in figuur 2.1. De
Figuur 2.1: Plattegrond van de entree van het gebouw.
eerste idee¨en voor de home-automatisering zijn als volgt:
• Om energie te besparen kan het gebouw alleen betreden worden via een luchtsluis,
waarbij de luchtsluis de volgende componenten heeft:
– RFID lezer
5
– Noodknop binnen de twee deuren.
– Buiten en binnen temperatuur meting
• De centrale hal heeft:
– RGB lampen.
– Bewegingsdetectoren.
– lichtkrant.
– Schemerlamp.
– relax stoel.
• De baliemedewerkster
– Kan diverse instellingen doen.
– Heeft verschillende controle LED

3 De automatiserings-middelen.
De opzet van de automatisering is in eerste instantie per verdieping en wordt in figuur
3.1 weergegeven. De automatisering van elke verdieping wordt gevormd rond twee
Raspberry pi die door middel van socket programmeren met elkaar kunnen commu-
niceren. Verder is elke Raspberry pi verantwoordelijk voor communicatie met slimme
Figuur 3.1: De beschikbare componenten tijdens het project.
sensoren en/of actuatoren. Zo is Raspberry pi 1 verantwoordelijk voor het verkrijgen
en verwerken van data van de slimme draadloze sensoren en het aansturen van de
draadloze actuatoren, Raspberry pi 2 is verantwoordelijk voor voor het verkrijgen en
verwerken van data van de slimme sensoren en het aansturen van actuatoren die door
middel van een bus met elkaar verbonden zijn.
De componenten die elke groep krijgt toegewezen zijn te vinden in bijlage A.
10
4 De opdracht deel 1
(week 1 t/m week 7).
Leest eerst hoofdstuk 1, 2 en 3 goed door om de context van de opdracht te begrijpen.
4.1 Gestelde eisen en Problemen
Om te voorkomen dat bij een grote storing alles uitvalt, is besloten dat:
A: Elke verdieping zelfstandig geregeld wordt, waardoor deze onafhankelijk van an-
dere verdiepingen zijn. In de toekomst zou eventueel een gecontroleerde verbin-
ding tussen de geautomatiserings-systemen van elke verdieping mogelijk moeten
zijn.
B: Elke verdieping wordt geautomatiseerd door twee Raspberry pi ’s die ieder een
eigen verantwoording heeft zoals te zien is in figuur 3.1
• ´E´en Raspberry pi controleert de onderdelen die verbindingen maakt via
WIFI (met gebruikmaking van de Wemos D1 mini module).
• ´E´en Raspberry pi controleert de microprocessoren ( STM32 L432KC) die
via een bussysteem met elkaar verbonden zijn.
• Beide Raspberry pi ’s communiceren met elkaar.
C: De programeertaal is C (embedded deel ) en C++ (Raspberry pi deel).
4.2 Concrete opdracht.
Met een aantal nieuwe medewerkers van de afdeling domotica vorm je een project-
groep. Je krijgt de volgende opdracht: Ontwerp voor de toegewezen verdieping een
automatiseringsplan en houdt hierbij rekening met de gestelde eisen van hoofdstuk 4.1
punt B:.
• Er wordt gewerkt volgens een duidelijk projectplan met informatie over de ge-
kozen fasering, deliverables en beheersing van minimaal: de planning, inzet van
mensen, communicatie met de stakeholders en kwaliteit van de oplossing.
• Voorafgaand aan het ontwerp wordt een overzichtelijk programma van eisen op-
gesteld, gebaseerd op de eerste grove idee¨en zoals in hoofdstuk 2 beschreven
staan aangevuld met interviews van de opdrachtgever.
• Een probleem analyse dat goed is gedocumenteerd en onderbouwd in een analyse rapport
met daarin onder andere het probleemdomein dat aan de hand van een UML dia-
11
gram wordt uitgelegd en waarin de product backlogs worden opgenomen.
• Een onderzoekrapport naar welk protocol (CAN-bus, I2, SPI ) het beste gekozen
kan worden om de microcontroles te verbinden met de Raspberry pi , denk hierbij
onder andere aan de onderzoeksvragen.
• De kwaliteit van essenti¨ele delen van het ontwerp en het halen van de eisen wordt
aangetoond met een gerealiseerd Proof of Concept. De opbouw/configuratie en
testresultaten staan in een testrapport met daaraan gekoppeld een conclusie en
advies over de opzet van de automatisering van de toegewezen verdieping.
• Aan het eind (dus na week 7) worden de resultaten gepresenteerd (PowerPoint-
presentatie) in de vorm van een onderbouwd advies.
• De opdrachtgever is tijdens de eerste fase alleen beschikbaar voor een inter-
view, voor de rest wordt deze vertegenwoordigd via de projectbegeleider, die
wekelijks op de hoogte gehouden wordt van de voortgang. Instructies van de
opdrachtgever (vertegenwoordigd door de projectbegeleider), zoals bijvoorbeeld
voorkeursoplossingen, worden meegenomen in het project. Alle initiatieven tot
communicatie/afstemming liggen bij de projectgroep.
Doordat we werken met verschillende opdrachtgevers, is de kans groot dat er
ook verschillende eisen gesteld worden aan dezelfde case (automatisering van
een verdieping). De opdrachtgever bepaald immers wat geautomaiseerd moet
worden.
4.3 Scope.
PoC: Je hoeft niet het complete sensor en actuatoren netwerk te demonstreren. Beperk
je tot de communicatie van een microcontrollers met een wmos via de Raspberry pi ,
waarbij een sensor uitgelezen en een actuator aangestuurd wordt, zoals weergegeven
in figuur 4.1.
Figuur 4.1: De proef of concept van deel 1.
Communicatie opdrachtgever: de opdrachtgever is in het eerste deel bereikbaar voor
een interview. Vragen kunnen gesteld worden aan de toegewezen projectbegeleider.
Eisen moeten gebaseerd zijn op de hier beschreven casus. Uiteraard kan bij twijfel of
bij ontbrekende belangrijke eisen contact worden gezocht met de projectbegeleider.
12
De focus van de analyse ligt op het probleemdomein en de hieruit voortkomende pro-
duct backlogs niet op het eindresultaat