# HAWK CHUA — handleiding

*De volledige Nederlandse handleiding. Een overzicht in het Engels: [README](../README.md).*

## Een chaotische synthesizer voor de M-VAVE FM-1

Door **plieuwski**. Contact: Instagram [@plieuwski](https://www.instagram.com/plieuwski/). Op de FM-1 Firmware Hub als
`hawk-chua` (identiteit `FM-1_007`).

Firmware voor de FM-1 waarvan de oscillator **Chua's circuit** is: het eenvoudigste elektronische circuit dat chaotisch
gedrag vertoont. Elke stem is een eigen circuit; het scherm tekent live de attractor die de stem doorloopt. Gebouwd op
een uitgeklede [Felucca](https://github.com/hugelton/Felucca) van Hügelton Instruments (hardwarelaag, USB, update-pad).

## Het circuit

Twee condensatoren (C1, C2), een spoel (L), een weerstand (R) en één niet-lineair element: de **Chua-diode**, een
negatieve weerstand met een stuksgewijs lineaire karakteristiek. Dimensieloos:

```
dx/dt = α (y − x − g(x))      x: spanning over C1
dy/dt = x − y + z             y: spanning over C2
dz/dt = −β y                  z: stroom door L
```

g(x) heeft helling −8/7 binnen |x| < 1, −5/7 tot |x| < 3 en +2 daarbuiten (daar verzadigen de op-amps van de diode,
zoals in een echt circuit; zo blijft elke instelling begrensd). Rond x = ±1,5 liggen twee evenwichtspunten waar de
toestand omheen spiraleert; de energie die de diode bij x = 0 toevoegt, gooit hem van de ene spiraal naar de andere.

Draai je α op, dan doorloopt het circuit de klassieke route naar chaos: rust → een zuivere **limietcyclus** (Hopf) →
**periodeverdubbeling** → een chaotische **spiraal** rond één evenwicht → de **dubbele scroll** (beide evenwichten) →
een **grote buitenste cyclus** (het verzadigde circuit). Waar die overgangen precies liggen hangt af van β; dat is
offline uitgerekend (`tools/gen_chua_tables.c` → `firmware/src/chua_tables.h`), zodat de CHAOS-knop voor elke SHAPE
hetzelfde regime betekent.

**Toonhoogte** is de tijdschaal van het circuit: per sample schuift de solver h = f_noot × periode / 44100 tijdseenheden
op, met de omlooptijd uit dezelfde tabellen. In het cycle-regime klinkt een noot binnen 2 % op zijn toonhoogte (getest);
in de chaotische regimes rond die toonhoogte, met de typische subharmonische ruis van het scrollen.

## Bediening

| Bediening | Functie |
| --- | --- |
| 27 toetsen | spelen (4 stemmen, elk een eigen circuit en een eigen resonator/waveguide), sturen USB-MIDI uit (kanaal 1) |
| KNOB 1 — CHAOS | α: CYCLE 0 · PERIOD 2 32 · SPIRAL 48 · DBL SCROLL 60 · OUTER 112 |
| KNOB 2 — SHAPE | β 10..34: lage waarden trage, ronde spiralen; hoge waarden snelle, strakke |
| KNOB 3 — OUTPUT | welke spanning je hoort: X (het scrollen, diep), X+Y, Y (de spiraal, helder), Y+Z, Z (rond) |
| KNOB 4 — RING | resonantie van de low pass gate: 0 geen, 127 Q 6 (een korte, houtachtige ring) |
| (de vier knoppen) | glijden als een potmeter: elke stap loopt in ~12 ms vloeiend over (ook X, Y en LEVEL van de noise-programma's en EXPAND van BIG BANG), dus geen zipper-ruis als je draait; een presetwissel springt meteen |
| HOME | (op HOME nog een keer: het BANK-menu, zie PRESETS) de oscillator: het circuit (CHAOS, SHAPE, OUTPUT, RING en de attractor) of een noise-programma (X, Y, LEVEL, RING; BIG BANG: WAVE, ENTROPY, BANG, EXPAND; PIANO: HAMMER, STRINGS, DECAY, STIFF) |
| ENV | (het lampje brandt zolang de envelope open staat én zolang het geluid uitklinkt, ook de staart van een FX of reverb) 1× de ENV-pagina, 2× de MOD ENV (een AD-envelope op de toonhoogte, of met **SELECT** op een parameter van de oscillator of de FX: de lijst volgt wat de oscillator en het FX-model nu hebben, met hun eigen namen (bij CHUA: CHAOS, SHAPE, OUTPUT; bij een noise-programma zijn X en Y; RING; de vier FX-knoppen en POSITION, FX MIX; SHIMMER (bij MOUTH de klinker, bij een delay het filter) en SPACE MIX). HEIGHT is dan hoe ver hij de parameter boven of onder de knopstand duwt, bij elke noot opnieuw; een preset begint op de toonhoogte), 3× terug naar HOME. ENV-pagina: KNOB 1–4 = ATTACK, DECAY, SUSTAIN, RELEASE (0,5 ms .. 10 s), met de envelope getekend. **PRESETS kiest hier presets voor alleen die pagina**: op ENV de envelope (PLUCK, PIANO, ORGAN, PAD, STRINGS, BRASS, PERC, STAB, ACID, SWELL, BELL, GATE, REVERSE, LONG TAIL), op MOD ENV de toonhoogte-envelope (OFF, DIVE UP, ZAP, KICK DROP, BLIP, OCTAVE CHIRP, RISE, SLOW SWEEP, FALL, SCOOP); net zo op de SCL-pagina (MONO, NARROW, WIDE, FULL, SUBTLE DRIFT, DRIFT, SWIRL, SPACE) en de LFO-pagina, FREE EG (OFF, RING SWEEP, WOBBLE 1/8, SQUARE TRILL, BAR RISE, AUTO PAN, ORBIT DRIFT, S&H CHAOS, BRIGHT LFO, NOISE SCAN, GATE PUMP, SHIMMER SWELL, SCREAM RISE: een curve met lengte en doel, meteen aan) |
| FX | de FX-pagina's: 1× de resonatoren van Mutable Instruments Rings en de waveguides (VOICE, FLUTE, SYRINX, HAWK, BRASS, BOW, REED, DRUM, PIANO), 2× de shimmer reverb, 3× terug naar HOME |
| SELECT | op HOME: het octaaf van de oscillator (-3 .. +3, alle oscillatoren, de FX stemmen mee; hetzelfde als SELECT op EDIT, opgeslagen met SAVE) · POSITION (resonator) · TONE (shimmer) |
| ALGORITHM | op HOME de oscillator: CHUA of een van de 32 oscillators A0..D1 (30 noise-programma's, BIG BANG en PIANO) · het model (resonator of waveguide) · het interval (shimmer) |
| PRESETS | 4 banken van 128 presets, elk in de volgorde van General MIDI (program change 0..127 = GM 1..128). Na een verse installatie (nog niets opgeslagen) start hij op bank 3 (SYNTH), FIFTH PAD; daarna op de laatst gekozen preset (3 s na het kiezen onthouden). PRESETS draait binnen de bank. **Bank kiezen: druk op HOME terwijl je al op HOME staat**: het BANK-menu toont de vier banken (1 GM, 2 GM ALT, 3 SYNTH, 4 LAB), elk met het programma dat nu speelt zoals het in die bank heet; draai PRESETS, SELECT of KNOB 1 naar een andere bank (hetzelfde programmanummer laadt meteen); KNOB 2 PART B (OFF, OMNI: een tweede geluid over het eerste heen op dezelfde noten, of CH 1..16: op een eigen MIDI-kanaal, zoals MULTI op de GLO-pagina), KNOB 3 de bank en KNOB 4 de preset van dat tweede geluid; HOME nog een keer = terug. Via MIDI: bank select CC 0 of CC 32 = 0..3 (XG: LSB, GS: MSB), daarna program change. Bank 1 (GM): piano: GRAND PIANO, BRIGHT PIANO, ELECTRIC GRAND, HONKY-TONK, E.PIANO 1, E.PIANO 2, HARPSICHORD, CLAVINET; chromatische percussie: CELESTA, GLOCKENSPIEL, MUSIC BOX, VIBRAPHONE, MARIMBA, XYLOPHONE, TUBULAR BELLS, DULCIMER; orgel: DRAWBAR ORGAN, PERC. ORGAN, ROCK ORGAN, CHURCH ORGAN, REED ORGAN, ACCORDION, HARMONICA, TANGO ACCORD.; gitaar: NYLON GUITAR, STEEL GUITAR, JAZZ GUITAR, CLEAN GUITAR, MUTED GUITAR, OVERDRIVE GTR, DISTORTION GTR, GTR HARMONICS; bas: ACOUSTIC BASS, FINGER BASS, PICK BASS, FRETLESS BASS, SLAP BASS 1, SLAP BASS 2, SYNTH BASS 1, SYNTH BASS 2; strijkers: VIOLIN, VIOLA, CELLO, CONTRABASS, TREMOLO STR., PIZZICATO, HARP, TIMPANI; ensemble: STRINGS, SLOW STRINGS, SYNTH STRINGS 1, SYNTH STRINGS 2, CHOIR AAHS, VOICE OOHS, SYNTH VOICE, ORCHESTRA HIT; koper: TRUMPET, TROMBONE, TUBA, MUTED TRUMPET, FRENCH HORN, BRASS SECTION, SYNTH BRASS 1, SYNTH BRASS 2; riet: SOPRANO SAX, ALTO SAX, TENOR SAX, BARITONE SAX, OBOE, ENGLISH HORN, BASSOON, CLARINET; fluiten: PICCOLO, FLUTE, RECORDER, PAN FLUTE, BLOWN BOTTLE, SHAKUHACHI, WHISTLE, OCARINA; synth lead: SQUARE LEAD, SAW LEAD, CALLIOPE, CHIFF LEAD, CHARANG, VOICE LEAD, FIFTHS LEAD, BASS + LEAD; synth pad: NEW AGE PAD, WARM PAD, POLYSYNTH, CHOIR PAD, BOWED PAD, METALLIC PAD, HALO PAD, SWEEP PAD; synth FX: RAIN, SOUNDTRACK, CRYSTAL, ATMOSPHERE, BRIGHTNESS, GOBLINS, ECHOES, SCI-FI; etnisch: SITAR, BANJO, SHAMISEN, KOTO, KALIMBA, BAGPIPE, FIDDLE, SHANAI; percussie: TINKLE BELL, AGOGO, STEEL DRUMS, WOODBLOCK, TAIKO, MELODIC TOM, SYNTH DRUM, REVERSE CYMBAL; geluidseffecten: GTR FRET NOISE, BREATH NOISE, SEASHORE, BIRD TWEET, TELEPHONE, HELICOPTER, APPLAUSE, GUNSHOT. Koper, strijkers en riet spelen op de waveguides BRASS, BOW en REED. Bank 2: een alternatieve versie van elk GM-programma, zoals de GM2/XG-varianten (MELLOW GRAND, ROCK PIANO, DETUNED EP 1, COUPLED HARPSI, CHURCH BELL, ROTARY ORGAN, UKULELE, 12-STRING GTR, ACID BASS, SLOW VIOLIN, ORCHESTRA, DARK TRUMPET, BASS CLARINET, KENA, ICE PAD, WIND CHIMES, DOMINATOR (een Alpha Juno-hoover) ...). Bank 3 (SYNTH): goed bespeelbare synthesizergeluiden, 16 per groep: leads (MONO SAW, PWM LEAD, SYNC LEAD, CHUA LEAD, ACID LEAD ...), bassen (SUB BASS, SAW BASS, ACID BASS 2, FM BASS, WOBBLE BASS ...), pads (SAW PAD, GLASS PAD, SHIMMER PAD, SPACE PAD ...), keys (FM E.PIANO, SYNTH ORGAN, CLAV SYNTH ...), plucks (SAW PLUCK, KARPLUS PLUCK, BELL PLUCK ...), brass en strings (SYN BRASS 3, SOLINA 2, BOW STRINGS SYN ...), sequence-geluiden (SEQ SAW, SEQ ACID, SEQ BELL ...) en bellen (FM BELL, GLASS BELL, GAMELAN, BOWL ...). Bank 4 (LAB): experimenteel sounddesign, 16 per groep: chaos-drones van het circuit (DOUBLE SCROLL, OUTER ORBIT, BIFURCATION ...), de noise-programma's op hun randen (RADIO STATIC, GRAIN CLOUD, ATARI CHAOS ...), waveguides die door SCREAM schreeuwen (SCREAMING HORN, THROAT SINGER, DIDGERIDOO, HAWK CRY ...), metaal en modaal (SHEET METAL, GONG CHAOS, SAW BLADE ...), FREE EG-bewegingen (VOWEL MORPH, TALKING SAW, BREATH ENGINE ...), glitch en digitaal (BITCRUSH WALK, MODEM, NUMBERS STATION ...), ruimtes (UNDERWORLD, GLACIER, EVENT HORIZON ...) en ritmisch (CHAOS KICK, LASER, WATER DROP ...) |
| PRESETS op FX / SHIMMER | op de FX-pagina gaat de PRESETS-knop door FX-presets (FX OFF, BELL, TUNED BAR, GLASS, SITAR, DRONE STRINGS, PIANO STRING, PLUCKED STRING, SOPRANO AAH, TENOR OOH, BASS OH, TOM, TIMPANI, TAIKO, NIGHT FLUTE, OVERBLOWN, BIRD, TWO BIRDS, HAWK, TRUMPET, HORN, HARMON MUTE, VIOLIN, CELLO, SUL PONTICELLO, CLARINET, SAX, OBOE, MUSETTE, BANDONEON, HIGHLAND PIPES, KALIMBA), op de SHIMMER-pagina door shimmer-presets (SHIMMER OFF, UNDERWORLD, STONE TEMPLE, ORACLE CHAMBER, CATHEDRAL, GOTHIC CHOIR, MOSQUE DOME, DOME DEPTHS, SUB HAZE, SMALL ROOM, PIANO BODY, GUITAR BODY, VIOLIN BODY, CELLO BODY, HARP BODY, ACCORDION BOX, BAGPIPE, PIPE ORGAN, LESLIE SLOW, LESLIE FAST en de delays: PING PONG 1/8, DOTTED PONG, TRIPLET PONG, FILTER PONG, SLAPBACK, TRESILLO ECHO, DUB SIREN, GALLOP, CLAVE ECHO, BOUNCING BALL, BALL PONG, RISING BALL, SCATTER DELAY); de klank zelf blijft staan |
| ARP | de ARP-pagina; nog een keer: terug naar HOME (het lampje brandt zolang de arpeggiator speelt) |
| REC | de TAPE-pagina (bandrecorder); daar: opnemen. Op de FREE EG-pagina neemt REC FREE EG op (bij NOTE nog een keer om te stoppen) |
| SCL | de PAN-pagina (nog een keer: terug naar HOME): KNOB 1 BASIS (stereo-spreiding per noot), KNOB 2 ORBIT (de stemmen bewegen mee met hun plaatje); de stemmen staan getekend waar ze in het stereobeeld klinken |
| SEQ | de KARMA-pagina (de frasegenerator); nog een keer: de LOGIC-pagina (ARP en KARMA samen); nog een keer: terug naar HOME (het lampje brandt zolang KARMA speelt) |
| PLAY | start en stopt de ARP of KARMA: op de ARP-pagina de ARP, op de SEQ-pagina KARMA, op andere pagina's de laatste van de twee die je opende. Knippert groen in het tempo zolang er een speelt, rood op de ARP- en SEQ-pagina's als er niets speelt. ARP en SEQ zelf: wat je als volgende kunt starten knippert in het tempo: eerst SEQ; speelt KARMA, dan ARP (met LOGIC samen); speelt de ARP, dan SEQ; spelen ze allebei, dan branden ze allebei. Staat de ARP of KARMA aan maar zijn er geen noten, dan flitsen de toetsen op elke tel: kies noten. Spelen ze samen (LOGIC), dan starten ze bij een nieuw akkoord allebei opnieuw vanaf dat akkoord, tegelijk |
| OCT- / OCT+ | octaaf −3..+3 |
| SAVE | (knippert altijd, één keer per maat op de eerste tel; alle knipperende LEDs volgen de BPM) alle instellingen in flash (terug na herstart; dat gebeurt ook vanzelf: 3 s na een wijziging, zodra er niets speelt, zodat uitzetten niets kost; bij het opstarten gelden de vaste regels nog steeds, zoals PLAY uit), én de klank zoals hij nu is over de preset die speelt (de naam blijft); PRESETS en program change laden daarna die opgeslagen versie. Via USB: `python3 tools/fm1_remote.py presets presets.txt` of de knop *Presets → C* op de Remote-pagina geeft alle 512 als regels voor de tabel in `firmware/src/ui.c` (de opgeslagen gemarkeerd `SAVED`); `reset-presets` of *Opgeslagen presets wissen* zet de ingebouwde terug |
| (blokjes rechtsonder) | knoppen met twee pagina's (ENV + MOD ENV, FX + SHIMMER) tonen rechtsonder twee blokjes: de pagina waar je bent oranje gevuld, de andere een leeg vakje |
| LFO | de FREE EG-pagina (nog een keer: terug naar HOME); daar neemt REC op |
| EDIT | de EDIT-pagina (nog een keer: terug naar HOME); SELECT zet het octaaf van de klank (-3 .. +3: alle oscillatoren, de FX stemmen mee; de presets zetten het op het register van hun instrument, bassen -2, cello en gitaren -1, piccolo en glockenspiel +1; opgeslagen met SAVE); KNOB 1 welke parameter het MOD WHEEL (MIDI CC 1) beweegt (de lijst van FREE EG), KNOB 2 hoe ver (DEPTH; wiel omhoog = tot 127, wiel omlaag = de oude waarde terug; speelt FREE EG op dezelfde parameter, dan regelt het wiel de diepte van FREE EG: wiel omlaag = stil, omhoog = FREE EG's DEPTH; pas zodra er een mod wheel binnenkomt), KNOB 3 VELOCITY op de klank (OFF, BRIGHT: zacht = donkerder, SWEEP: zacht = minder MOD ENV-sweep, BRIGHT + SWEEP), KNOB 4 hoeveel |
| GLO | de MIDI-pagina; nog een keer: de **MULTI-pagina** (multitimbraal: KNOB 1 MULTI aan/uit, KNOB 2 het MIDI-kanaal van part B, KNOB 3 de bank en KNOB 4 het programma van B; program change en bank select op B's kanaal kiezen B's geluid; A is de klank op het paneel, op het RX-kanaal. Elk part 2 van de 4 stemmen en 2 resonatoren, de SHIMMER (van A) gedeeld, opgeslagen met SAVE); nog een keer: de **SOUND-pagina** (KNOB 1 SPEAKER BASS 0..127, standaard 4: de boventonen van de bas erbij gemaakt, zodat je hem op de kleine interne speaker hoort (het oor hoort de grondtoon die erbij hoort); KNOB 2 LEVEL MATCH aan/uit, standaard aan: elke preset op hetzelfde gemeten volume, -22 LU (nooit boven -1 dBFS); een opgeslagen preset krijgt de waarde van zijn plek; KNOB 3 BEND RANGE: MIDI pitch bend 0..12 halve tonen op en neer, standaard 12); nog een keer: terug naar HOME. De MIDI-pagina: KNOB 1 TRS MIDI IN aan/uit, KNOB 2 USB MIDI (OFF, IN, OUT, IN + OUT), KNOB 3 RX-kanaal (OMNI, 1..16), KNOB 4 TX-kanaal (1..16); SELECT de tweede core aan/uit (vanaf de volgende start) |
| Snel draaien | elke knop en encoder (KNOB 1–4, SELECT, ALGORITHM, PRESETS) versnelt tot 4×, net als BPM (van 0 naar 127 in één zwaai); één klik blijft één stap |
| MASTER | volume |
| MIDI in (USB en TRS) | note on/off op het RX-kanaal (of elk: OMNI), program change = preset in de bank van bank select (CC 0/32 = 0..3; program 1 = de eerste); clock voor de arpeggiator; CC's volgens de XG/GM-indeling (zoals een Yamaha MU90): 1 MODULATION = mod wheel (zie EDIT), 7 MAIN VOLUME (100 = normaal), 10 PAN (balans), 11 EXPRESSION, 64 HOLD (noten klinken door; bij PIANO blijven de dempers omhoog), 66 SOSTENUTO, 67 SOFT PEDAL (nieuwe noten zachter), 71..74 = KNOB 1..4 van de HOME-pagina (circuit: CHAOS, SHAPE, OUTPUT, RING; noise-programma: X, Y, LEVEL, RING), 91 REVERB = SHIMMER MIX, 93 CHORUS = FX MIX, 94 VARIATION = SHIMMER, 0/32 BANK SELECT (zie PRESETS), 120/123 alles uit, 121 RESET ALL CONTROLLERS; pitch bend (tot een octaaf op en neer: BEND RANGE op de SOUND-pagina; de FX buigen mee). Aftertouch: niet ingedrukt klinkt alles zoals zonder aftertouch, indrukken geeft tot twee keer zoveel adem. Niet: portamento, RPN/NRPN |
| USB | class-compliant MIDI + USB-audio-ingang (opnemen in je DAW) |
| OCT- + OCT+ 5 s ingedrukt | UBOOT (update-modus) |

**De noise-oscillators.** Draai op HOME aan ALGORITHM: na CHUA komen 30 noise-programma's naar de Noise Plethora (en als laatste BIG BANG, zie D hieronder)
van Befaco (drie banken van tien, zoals op de module). Het zijn eigen fixed-point versies van wat de handleiding per
programma beschrijft, gemaakt voor een keyboard: de noot zet de hoofdfrequentie, KNOB 1 en 2 zijn X en Y van het
programma (de cel toont wat ze doen), KNOB 3 het niveau, KNOB 4 de RING van de gate. Elke stem draait een eigen kopie,
door dezelfde ADSR en low pass gate (en de FX) als het circuit. Het scherm tekent de uitgang tegen zichzelf een paar
samples eerder, een "attractor" van de ruis; met SELECT op TIME zie je de golfvorm.

| Bank | Programma's (X / Y) |
| --- | --- |
| A — textures | A0 RADIO OH NO (osc 2 / PWM) · A1 RWALK FLANGE (loopsnelheid / flanger) · A2 XMOD RING SQR · A3 XMOD RING SINE (osc 2 / FM) · A4 CROSS MOD RING (verhoudingen / FM) · A5 RESONOISE (mod-verhouding / fold) · A6–A8 GRAIN GLITCH 1–3 (korrel / FM) · A9 BASURILLA (LFO-snelheid / breedte) |
| B — HH clusters | B0 CLUSTER SAW (spreiding / aantal stemmen) · B1 PW CLUSTER (ontstemming / pulsbreedte) · B2 CR CLUSTER · B3 SINE FM CLUST · B4 TRI FM CLUST (ontstemming / FM) · B5 PRIME CLUSTER (spreiding / FM) · B6 PRIME C NOISE (spreiding / ruis-FM) · B7 FIBONACCI · B8 PARTIAL CLUST (spreiding / toon) · B9 PHASING CLUST (spreiding / drift) |
| C — harsh & wild | C0 BASURA TOTAL (LFSR / galm) · C1 ATARI · C2 WAKING FILOM. (osc 2 of box / FM) · C3 P S H (snelheid / galm) · C4 ARRAY ROCKS (mod-verhouding / husselen) · C5 EXIST. PAIN · C6 WHO KNOWS (sweep-snelheid / diepte) · C7 SATAN WORKOUT (FM / crush) · C8 RWALK BITCRUSH · C9 RWALK LFREE (box / galm) |
| D — orde | D0 **BIG BANG** (eigen ontwerp): de harmonische reeks, 16 boventonen, gewogen als de standaard golfvormen. KNOB 1 WAVE: sinus → driehoek → zaagtand → blok (exact, en vanzelf zonder aliasing: een boventoon boven de grens valt weg) · KNOB 2 ENTROPY: de boventonen zwerven weg van de reeks (tot ±50 %) en ruis komt erbij, van orde naar wanorde · KNOB 3 BANG: elke noot begint als een oerknal in volledige wanorde en koelt af naar ENTROPY, in 30 ms .. 4 s (0: geen knal) · KNOB 4 EXPAND: de reeks uitgerekt, boventoon n op n^(1..1,5): zuiver → piano → klok. (BANG en EXPAND staan op de plek van LEVEL en RING; ze worden bewaard in CHAOS en OUTPUT van het circuit, ook voor MIDI CC 73/74.) Presets: BANG SAW LEAD, BANG SQUARE, BANG BASS, COOLING PAD (bank 3), BIG BANG, EXPANDING BELL, HEAT DEATH, PRIMORDIAL SOUP (bank 4) |
| | D1 **PIANO** (eigen ontwerp, zoals BIG BANG een klank uit zijn boventonen): de toon van een piano uit wat hij is. Tot 24 deeltonen van de aangeslagen snaren, elk waar een piano hem heeft: n·√(1+B·n²), de stijfheid van het staal (scherp, hoe hoger hoe meer; B per noot zoals een echte piano). Zo luid als de hamer hem achterlaat: zijn plek op de snaar (een achtste: de 8e en 16e ontbreken), zijn vilt (hoe harder de aanslag en HAMMER, hoe meer boventonen). Elk in twee fasen zoals bij een piano: de *prompt sound* die snel wegvalt en de lange *aftersound* (de grondtoon ~20 s in de bas, ~1 s bovenin); hoge deeltonen sterven eerder. De twee of drie snaren per noot iets uit elkaar gestemd: elke deeltoon zwelt en zakt in zijn eigen tempo (STRINGS). Bij de aanslag de bons van de hamer op de klankbodem en het tikje van het vilt. Elke noot wordt opnieuw aangeslagen; loslaten dempt via de release. KNOB 1 HAMMER: zacht vilt .. hard. KNOB 2 STRINGS: één snaar .. een vleugel .. honky-tonk. KNOB 3 DECAY: × ¼..4. KNOB 4 STIFF: inharmoniciteit × 0..2,5 (51 = een piano). Presets: GRAND PIANO, BRIGHT PIANO, HONKY-TONK (bank 1), MELLOW GRAND, ROCK PIANO, SALOON PIANO (bank 2). |

Ze zijn op hetzelfde niveau gezet als het circuit (bij X = Y = 64, gemeten in de test) en blijven in elke hoek van X en
Y begrensd en zonder DC.

**De MOD ENV (ENV, tweede keer).** Druk op ENV terwijl je al op de ENV-pagina staat: een AD-envelope die
direct op de toonhoogte van het noise-programma zit (geen oscillator, geen VCA ertussen): de noot begint hoger en
zakt naar zijn toon (of zwelt omhoog, met SKEW), gestart door elke noot (hij loopt zijn vorm af, ook als je de toets eerder loslaat).

| Bediening | Functie |
| --- | --- |
| KNOB 1 LENGTH | de hele duur, 0,5 ms .. 10 s |
| KNOB 2 SKEW | waar de piek valt: 0 = meteen (FALL: een aanslag die wegvalt), 64 = in het midden, 127 = aan het eind (RISE: aanzwellen, dan weg) |
| KNOB 3 HEIGHT | hoe ver de toon op de piek omhoog gaat, tot +36 halve tonen (3 octaven); 0 = geen envelope |
| KNOB 4 BEND | de kromming, twee kanten op: in het midden STRAIGHT (recht); rechtsom (+) exponentieel: traag omhoog en dan snel, snel omlaag met een lange staart (een kick- of zap-achtige sweep); linksom (−) logaritmisch: snel omhoog en lang bovenin, dan pas omlaag |

Er is geen aparte FM-oscillator meer: de MOD ENV is de FM, direct op de toon
(SKEW 0 en een korte LENGTH: een zap of kick, zie MELODIC TOM; SKEW hoog: een toon die omhoog zwelt). Het scherm toont
de vorm en rechts hoe ver de nieuwste stem nu omhoog staat.
Nog een keer ENV: terug naar HOME.

**BASIS (SCL, KNOB 1).** De startplek van elke noot, zoals een orkestopstelling: twaalf zitplaatsen verspreid over
het stereobeeld, en elke nieuwe noot neemt de volgende plaats (de spelers om de beurt, los van welke stem hem speelt).
De plaatsen volgen de gulden snede: opeenvolgende noten zitten ver uit elkaar en samen vullen ze het beeld gelijkmatig.
BASIS schaalt de plaatsen: 0 is mono, 127 de volle breedte; ook noten die al klinken schuiven mee als je draait. De
resonator van een noot klinkt op dezelfde plek als die noot; de shimmer krijgt het midden en maakt er zijn eigen stereo
van. De pan-wet is een balans: het midden blijft op vol niveau, de andere kant gaat zachter.

**ORBIT (SCL, KNOB 2).** De spreiding, via de fase en op de harmonische reeks: vier all-pass filters per stem staan op
de even boventonen van de noot (2, 4, 6, 8 × de grondtoon) en draaien die elk een halve slag; de oneven boventonen
blijven zoals ze zijn. Het verschil gaat naar de zijkanten (links in fase, rechts in tegenfase), zodat de twee helften
van de reeks uit elkaar gaan, zoals de spelers van een sectie een akkoord spreiden. De filters bewegen mee met het
plaatje van de stem (de x van de attractor, of het zwerven van een noise-programma: ±25 %), dus de spreiding ademt
met de chaos. ORBIT bepaalt hoe sterk. Mono (links + rechts) blijft de stem zelf, dus niets valt weg op een
mono-speaker. ORBIT werkt ook met BASIS op 0. De PAN-pagina tekent het stereobeeld van links naar rechts: de twaalf
plaatsen van BASIS (de volgende noot krijgt de hoge streep) en de stemmen die klinken, met hun noot.

**De arpeggiator (ARP).** ARP opent de ARP-pagina; PLAY zet hem aan of uit (het lampje brandt zolang
hij aan staat). Aan: de noten die je vasthoudt (toetsen en MIDI in) klinken niet zelf, de arpeggiator speelt ze.
Zolang er MIDI clock binnenkomt (USB of TRS, 24 per kwartnoot) loopt hij daarop mee (START begint het patroon
opnieuw); komt er 0,5 s geen clock, dan loopt hij op zijn eigen BPM. Naar de arpeggiators van de Yamaha AN1x en DJX.
Wat hij speelt gaat over USB-MIDI naar buiten (als USB OUT aan staat). ARP en KARMA (SEQ) staan alleen tegelijk aan met LOGIC (zie onder).

| Bediening | Functie |
| --- | --- |
| KNOB 1 BPM | 40..250 (onder MIDI clock staat er EXT en het gemeten tempo); de PRESETS-knop doet hier ook de BPM, net als op de SEQ- en TAPE-pagina's |
| KNOB 2 SUBDIVIDE | de tien van de AN1x: 3/8, 1/4, 3/16, 1/6, 1/8, 3/32, 1/12, 1/16, 1/24, 1/32 (standaard 1/8) |
| KNOB 3 GATE | hoe lang elke noot klinkt: 8..100 % van de stap (GATE FIT: zolang de ARP of KARMA speelt is de attack van elk geluid hoogstens een kwart van de gate, zodat ook trage pads op korte stappen meteen spreken; op de toetsen hun eigen attack) |
| KNOB 4 SWING | 50 % (geen) .. 83 % (de meeste), zoals de AN1x: elke tweede stap later |
| ALGORITHM | het patroon (zie hieronder) |
| SELECT | LATCH (zoals bij KARMA, standaard aan): rechtsom aan (de noten blijven spelen als je loslaat, een nieuw akkoord vervangt ze), linksom uit |

**GROOVE (ARP twee keer).** De ARP VELOCITY SEQ: een kort patroon (2 tot 8 stappen, met een eigen lengte, zodat het
tegen het arp-patroon in schuift) dat per stap de velocity bepaalt, 1..9 zacht tot hard; een 0 is geen noot op die stap
(een rust, het arp-patroon loopt door). ALGORITHM kiest het patroon: ACCENT 4, ACCENT 3, OFFBEAT, RAMP UP/DOWN, TRESILLO,
WALTZ, GHOSTS, PUMP, SWELL, FIVE, SEVEN, BACKBEAT, en met rusten SKIP, OFF SKIP, 3 OF 4, CLAVE, GALLOP, BREATH, FIVE RESTS,
STUTTER, SPARSE, HOLES. KNOB 1 SEQ LENGTH (1..8): hoeveel van de acht getoonde stappen
(het patroon herhaald) de velocity seq loopt; de stappen erachter worden donker. Een ander patroon kiezen zet SEQ LENGTH op
de eigen lengte van dat patroon. KNOB 2 DEPTH: hoe sterk de velocity's werken (de rusten altijd). Daaronder de ARP GATE SEQ, in dezelfde stijl: per stap de
gate, met GATE (KNOB 3 op de ARP-pagina) als basis: 1..9 is x 0,2 tot x 1,8 (5 is GATE zelf, hoogstens 100 %). KNOB 4
kiest het patroon (OFF, LONG SHORT, SHORT LONG, SHORT x3, GALLOP, DOTTED, TRESILLO, RAMP UP/DOWN, BOUNCE, STAB,
LEGATO 1/4, SWELL, STUTTER, WALTZ, BREATH), KNOB 3 GATE LENGTH (1..8) hoeveel van de acht stappen meelopen, zoals
SEQ LENGTH. De pagina toont
de stappen als staafjes, de stap die speelt licht op.

De patronen: de dertig Arpeggiator Types van de AN1x (uit zijn Data List) en twee van de DJX. 1–15: UP, DOWN,
UPDW A (de uitersten één keer), UPDW B (de uitersten twee keer) en RANDOM, elk over 1, 2 of 4 octaven. 16–30: TECHNO-A
(Euro), TECHNO-B (UK, met velocity), TECHNO-C (Japans), TECHNO-D (Duits), DA HOUSE (bas links, akkoorden rechts),
SYNCOPA A, SYNCOPA B (grote octaafsprongen), SYNCO ECHO, TEKK ECHO A en B, PULSE LINE, BASSLINE A–D. Daarna ECHO en
SWEEP van de DJX. De Data List geeft van 16–30 alleen de naam en één regel commentaar, de handleidingen geen noten: die
stappen zijn mijn invulling van die beschrijvingen ("moving filter" bij TEKK ECHO als een velocity-zwaai). Daarna
zestien naar de Arpeggio Styles van de Roland MC-303 (ook hier: naam en beschrijving uit de handleiding, de stappen
zijn eigen werk): GLISSANDO (halve toon voor halve toon van de laagste naar de hoogste noot en terug), SYN BASS,
SLAP BASS (duim laag, pop een octaaf hoger, gedempte tussennoten), WALK BASS, RHYTHM GTR (akkoordstoten met
gedempte chops), STRUM GTR (het akkoord echt aangeslagen: neer en op, 12 ms per snaar), 3 FINGER (Travis: de duim
wisselt laag, de melodie op de hoogste noot), PIANO BACK (bas en akkoord), CLAVI CHORD, WALTZ, SWING WALTZ (in
triolen: zet SUBDIVIDE op 1/12), REGGAE (de skank op de tweede en vierde tel, kort), PERCUSSION, HARP, SHAMISEN (de
hoogste noot met loopjes eronder) en BOUND BALL (een stuiterende bal: steeds snellere herhalingen; mooi op 1/32).
Gedempte stappen klinken een kwart stap, wat de GATE ook is. Alle stappatronen (16–48) blijven in het octaaf van je
akkoord: wat erboven of eronder zou vallen (hun octaafsprongen, de plaatsen voorbij de hoogste noot) wordt terug
gevouwen naar je laagste noot .. een octaaf hoger (of je hoogste, als het akkoord breder is), zodat het register
hetzelfde blijft als je van patroon wisselt. Wil je wel over meer octaven: UP/DOWN/UPDW/RANDOM OCT2 en OCT4. Het scherm toont de laatste 32 stappen als een rol.

**FREE EG (LFO).** Neemt een knopbeweging op en speelt die af op een parameter, zoals de Free EG van de AN1x. LFO
opent de pagina; daar zet KNOB 4 het doel (TARGET: CHAOS, SHAPE, RING, NOISE X/Y, MOD ENV HEIGHT, FX STRUCTURE/BRIGHTNESS/
DAMPING/MIX, SHIMMER, BASIS, ORBIT, ARP GATE/SWING, KARMA K1..K4, ...), KNOB 2 de lengte en KNOB 3 de diepte. Druk REC en draai aan
KNOB 1: de beweging wordt opgenomen (128 punten) en je hoort hem meteen op het doel. SELECT rechtsom speelt hem af
(linksom uit: de parameter krijgt zijn oude waarde terug).

**FREE EG in de presets.** Een deel van de presets zet zelf een contour in FREE EG (LENGTH NOTE, rond de waarde van
de preset), zodat elke noot leeft zoals een speler hem speelt: de boventonen van de fluit komen erin doordat de
luchtstraal korter wordt (PICCOLO, FLUTE, RECORDER, OCARINA, WHISTLE: FX STRUCTURE), een stoot lucht bij het aanzetten
die overgaat in een rustige adem (PAN FLUTE, SHAKUHACHI, BLOWN BOTTLE, CALLIOPE, CHIFF LEAD: FX POSITION), koper dat
openbloeit doordat de lippen zich spannen (TRUMPET, TROMBONE, TUBA, FRENCH HORN, ...: FX STRUCTURE), een aanzwellende
streek (de strijkers: FX BRIGHTNESS), het riet dat aanzet (hobo, fagot, klarinet, accordeons), een sax die laat in een
lange noot gaat grommen (SCREAM), een koor dat van oe naar aa naar ee opent (FX POSITION), een pad waarvan de shimmer
aanzwelt. Een preset zonder contour zet FREE EG weer uit (een eigen opname laat hij staan, tot een preset met een
contour hem vervangt). Het sterretje bij de presetnaam en SAVE kijken naar de waarde waaromheen FREE EG beweegt.

| LENGTH | Wat |
| --- | --- |
| NOTE | elke noot (ook van de arpeggiator en KARMA) start hem opnieuw; hij speelt één keer, zo lang als je opnam (REC nog een keer stopt, max ~6 s), en houdt dan de laatste waarde |
| 1..8 STEPS | een lus van 1 tot 8 zestiende noten |
| 1..4 BARS | een lus van 1 tot 4 maten (4/4) |
| PHRASE | precies zo lang als de frase van KARMA (5/4, 6/8, 3 tegen 4: wat de GE ook is) en elke keer dat de frase opnieuw begint begint hij mee; zonder KARMA één maat |
| LFO 0.1 .. 32 HZ | de opname als vrijlopende lus (een LFO van je eigen vorm), los van het tempo, op elk doel |
| AM X1/2 .. X4 | de opname als lus op audiosnelheid, 1/2 tot 4 keer de noot, elke sample op het niveau van de stemmen (amplitudemodulatie: zijbanden, van tremolo-achtige ruwheid tot belachtige klanken); opnemen gaat dan op 1 Hz, zodat je de golfvorm rustig met KNOB 1 tekent |
| FM X1/2 .. X4 | dezelfde lus als frequentiemodulatie van de oscillator (CHUA: de stapgrootte, NOISE: de filters), 1/2 tot 4 keer de noot: zijbanden op de harmonischen, de toonhoogte blijft de noot; DEPTH tot ~110 blijft tonaal (van zacht naar metaalachtig), daarboven slaat het om naar ruis |

STEPS en BARS lopen op de BPM van de ARP-pagina, of op MIDI clock als die binnenkomt (START begint de lus opnieuw);
de opname begint aan het begin van een lus en stopt vanzelf na één lengte. DEPTH 127 speelt de opname precies af, lager
schaalt hij rond de waarde die de parameter had; zolang hij speelt neemt hij de eigen knop van die parameter over.

**KARMA (SEQ).** Een frasegenerator naar KARMA van Korg (Kay Algorithmic Realtime Music Architecture, Stephen
Kay; de KARMA Basic Guide). De noten die je vasthoudt klinken niet zelf: een *Generated Effect* (GE) maakt er een
muzikale frase van, in de maat, zolang je ze vasthoudt (of met LATCH). Speel een ander akkoord en de frase volgt het
meteen. De architectuur is die van KARMA, de code en de frases zijn nieuw (KARMA's eigen GE's zijn niet openbaar).

Een GE is, zoals bij KARMA, een paar kleine generatoren die samenwerken:

| Generator | Wat hij doet |
| --- | --- |
| RHYTHM | een patroon van stappen: zware, lichte en spook-aanslagen, rusten, overbindingen, lange noten; in 1/32, 1/16, 1/8-triolen, 1/8 of 1/4 |
| NOTE SERIES | waar de frase van gemaakt is: het akkoord, het akkoord met de grondtoon erboven, of de toonladder die het akkoord suggereert (majeur of mineur, diatonisch of pentatonisch), over 1–4 octaven |
| INDEX | de weg door die noten: op, neer, op en neer, willekeurig, een *random walk*, een spiraal (laag, hoog, steeds meer naar het midden) of *Direct Index* (elke stap noemt zijn noot) |
| CLUSTER + STRUM | hoeveel noten per aanslag (een akkoordstoot), uitgespreid als een gitaar: omlaag, dan omhoog |
| MELODIC REPEAT | elke noot echoot een paar keer, getransponeerd en zachter (cascades, echo's in toonhoogte), afwisselend links en rechts |
| VELOCITY / GATE | een accentpatroon en een lengte per GE |
| PAN | per noot: afwisselend, willekeurig, een zwaai over de frase, of naar toonhoogte (laag links); BASIS (SCL) zet hoe breed |
| ENV | een vorm over de frase (op, neer, op-en-neer) op een parameter van de klank: RING, FX BRIGHTNESS, CHAOS, OUTPUT, SHIMMER MIX ...; als hij stopt krijgt de parameter zijn waarde terug |

| Bediening (SEQ-pagina) | Functie |
| --- | --- |
| PLAY | KARMA aan/uit |
| ALGORITHM | de GE (24, zie hieronder) |
| KNOB 1..4 | de vier *Realtime Controls* van die GE; de cellen tonen wat ze doen. 64 is de GE zoals hij geschreven is |
| SELECT | LATCH (standaard aan): rechtsom aan (de frase loopt door als je loslaat, een nieuw akkoord vervangt het oude), linksom uit |
| PRESETS | het tempo (BPM 40..250, hetzelfde als KNOB 1 op de ARP-pagina); op deze pagina kies je dus geen preset |

De Realtime Controls: DENSITY (0 niets, laag alleen de zware aanslagen, 64 het patroon, hoger ook de spooknoten en
boven 96 noten in de rusten), RANGE (−3..+3 octaven), REPEAT (meer of minder echo's), ACCENT (dynamiek, boven 96 ook
toeval), GATE, RANDOM (af en toe een andere noot, boven 96 octaafsprongen), CLUSTER, TRANSPOSE (octaven), ENV (diepte
van de envelope) en STRUM.

De GE's: PULSE ARP (de klassieke 16e-arp met accenten en spooknoten), CASCADE (dalende kwarten die echoën), HARP GLISS
(een glissando door de toonladder), STRUM GUITAR, FINGERPICK (Travis-achtig: bas, dan de akkoordtonen), TECHNO STAB
(akkoordstoten, RING zwelt over de frase), ACID LINE (303-achtig, met overbindingen en octaven), BASS WALK (een
wandelende bas door de toonladder), OCTAVE BASS, TRANCE GATE, RAIN DROPS (pentatonisch, willekeurig, met echo's),
SPIRAL, BROKEN 6/8, GAMELAN (een kotekan-achtig pentatonisch patroon met echo een octaaf hoger), ECHO CANYON (twee
noten, vier echo's), FUNK CLAV, POLY 3 OVER 4 (een figuur van drie zestienden tegen de maat), SWELL PAD (lange
akkoorden, SHIMMER MIX zwelt), CHAOS MACHINE (de toonladder over vier octaven terwijl CHAOS oploopt: het circuit raakt
steeds verder uit zijn baan), SCROLL HOPPER (OUTPUT golft over de frase: van de scroll naar de spiraal en terug), NOISE
PERC (voor de noise-programma's: korte tikken laag en hoog), BIRD CALLS (trillers in 1/32 en stilte: mooi met de
SYRINX-waveguide), MINIMAL 5/4 (Reich-achtig, 20 zestienden, met echo een octaaf hoger) en LULLABY. Daarna tien
naar de stijlen van de Roland MC-303: REGGAE SKANK (bas en korte skank-akkoorden op de achtertel), WALTZ (bas, akkoord,
akkoord; de bas wisselt grondtoon en kwint), SWING WALTZ (rechte achtsten: de swing komt, zoals bij alle GE's, van SWING op de ARP-pagina), PIANO COMP (bas en gesyncopeerde akkoorden),
SLAP POP (duim en pop, octaven), GUITAR CHOP (korte aangeslagen akkoorden), SHAMISEN (de hoogste noot met loopjes,
pentatonisch), SEQ LINE (een 16e-sequencerlijn met octaafsprongen, de helderheid golft mee), BOUNCING BALL (elke
nieuwe noot stuitert uit: steeds snellere, zachtere herhalingen, van links naar rechts) en THREE FINGER. En twaalf met akkoorden: HOUSE PIANO (M1-house: akkoordstoten op de achtertel), GOSPEL COMP (bas en volle akkoorden met spookslagen), BOSSA NOVA (de syncopen van de gitaar over grondtoon en kwint), NEO SOUL (lui, gerolde akkoorden, achter de tel), CHARLESTON (de 1 en de en van 2), MONTUNO (salsapiano: octaven en tertsen, gesyncopeerd), DUB CHORDS (akkoorden op de achtertel die wegechoën), ROCK EIGHTS (rechte achtsten, de backbeat luider), HALFTIME (grote akkoorden, twee per maat, openend), OOM PAH (een mars: bas, akkoord), AFROBEAT CHOP (korte in elkaar grijpende chops, links en rechts) en CHORD STAIRS (het akkoord klimt door zijn omkeringen). De GE's met een
envelope (TECHNO STAB, ACID LINE, TRANCE GATE, SWELL PAD, CHAOS MACHINE, SCROLL HOPPER, MINIMAL 5/4) spelen dus ook
met de klank, niet alleen met de noten.

Het scherm toont het ritme van de GE (hoogte = gewicht, het spelende stapje licht op) en eronder de laatste 32 noten
als een rol; onderin de klok en de lengte ("20 x 1/16").

**KARMA, de ARP en FREE EG samen.**
- *Eén tegelijk* (LOGIC OFF). KARMA en de ARP pakken allebei de noten die je vasthoudt; zet je de een aan, dan gaat de
  ander uit en de vastgehouden noten gaan mee over. Zet je hem uit, dan klinkt het akkoord gewoon.
- *Samen: LOGIC* (SEQ twee keer). KNOB 1 LOGIC: OFF (één tegelijk, zoals hierboven), OR (standaard: allebei spelen alles), AND
  (een noot van de een klinkt alleen waar de ander op dezelfde tel ook speelt), XOR (alleen waar de ander niet speelt:
  ze vullen elkaars gaten), NOR (alleen in de gaten van allebei: de ARP speelt daar zijn laatste noot), ALT (om de
  beurt: elke tel met een slag gaat naar de volgende), S/R (een slag van de ARP zet aan, een van KARMA zet uit: aan speelt
  de ARP, uit KARMA). KNOB 4 INVERSE: wat de logica tegenhoudt komt erdoor en andersom. Beide spelen dezelfde vastgehouden noten op één klok, die van KARMA: de ARP begint mee met
  de frase (op de zestiende), en de vergelijking gebeurt op het rechte raster; SWING komt daarna, voor elk apart. Een
  stap die tegengehouden wordt is een rust, het patroon loopt door. KNOB 2 NOTE QUANT zet alles wat de twee maken in een toonsoort: CHORD (standaard: alleen de akkoordtonen), AUTO (de
  toonladder van het akkoord, majeur of mineur, de akkoordtonen altijd erin), of een eigen toonladder: MAJOR, MINOR,
  DORIAN, PHRYGIAN, LYDIAN, MIXOLYDIAN, LOCRIAN, HARM MINOR, MEL MINOR, PENTA MAJOR, PENTA MINOR, BLUES, WHOLE TONE,
  HIRAJOSHI, DOUBLE HARM; of OFF. KNOB 3 ROOT: de grondtoon, AUTO (de laagste vastgehouden noot) of C .. B. PLAY op deze pagina zet ze samen aan en uit. De pagina laat de laatste tellen
  zien: wat de ARP speelt, wat KARMA speelt en wat erdoor komt.
- *Eén tempo.* KARMA loopt op de BPM van de ARP-pagina, of op MIDI clock als die binnenkomt (START begint de frase
  opnieuw), en neemt de SWING van de ARP-pagina over voor zijn zestienden (of achtsten). Een nieuwe frase begint op
  de volgende zestiende (KARMA's *Quantize Trigger*); vanuit stilte meteen.
- *FREE EG.* LENGTH PHRASE loopt precies met de frase van KARMA mee (opnemen begint aan het begin van de frase) en de
  TARGETs KARMA K1..K4 zijn de vier Realtime Controls. Zo neem je bijvoorbeeld een DENSITY-beweging op die over de
  frase opbouwt, of een REPEAT die alleen aan het eind uitwaaiert. FREE EG NOTE start bij elke noot die KARMA maakt
  opnieuw: een eigen envelope per noot van de frase. Als FREE EG en de envelope van een GE dezelfde parameter willen,
  wint FREE EG.
- *MIDI uit.* Wat KARMA en de ARP maken gaat over USB-MIDI naar buiten op het TX-kanaal (als USB OUT aan staat op de
  GLO-pagina), zoals KARMA zijn gegenereerde noten uitstuurt; de toetsen zelf dan niet.

**Elke parameter op één plek.** RING staat alleen op HOME (KNOB 4), BASIS en ORBIT alleen op de PAN-pagina (SCL), de
oscillator kies je alleen met ALGORITHM op HOME.

**De low pass gate.** Op de ENV-pagina is de gate een rond rood lampje (het LED van een vactrol): het brandt fel als de gate open staat en dimt mee met de envelope; ernaast de cutoff. De ADSR stuurt per stem een low pass gate aan, zoals een vactrol dat doet: de stuurspanning volgt
de envelope snel omhoog (~2 ms) en trager omlaag (~30 ms), en zet zowel het volume als de cutoff (25 Hz .. 15 kHz). Het
filter is een resonant state-variable low pass (zero-delay feedback, stabiel bij elke cutoff en resonantie). Met RING
hoog rinkelt het: een "ping" als de gate opengaat, en een staart die doorklinkt terwijl hij dichtgaat. Probeer
CLAVINET of MUTED GUITAR (sustain 0: elke noot is een aanslag).

**De resonatoren (FX).** De resonatormodellen van Mutable Instruments Rings, gevoed door de oscillators zoals de
IN-ingang van Rings zijn resonatoren voedt. Polyfoon, zoals Rings op polyfonie 4: vier resonatoren, elk gestemd op een
noot. Elke stem voedt alleen de resonator van zijn eigen noot, dus een akkoord klinkt als akkoord. Een nieuwe noot
neemt de resonator die die noot al heeft, anders een vrije (de langst ongebruikte), anders de oudste; die klinkt dan
verder op de nieuwe toonhoogte, net als bij Rings. Resonatoren die stil zijn rekenen niet mee, en de tweede core neemt
de helft van de klinkende voor zijn rekening. Het scherm toont de resonator van de nieuwste noot. De DSP is de fixed-point port van
Rings en DaisySP (Emilie Gillet, Electrosmith; MIT) die Felucca al had (`phys_dsp.c`, `phys_symp.c`).

| Model (ALGORITHM) | Wat | KNOB 1 STRUCTURE | SELECT |
| --- | --- | --- | --- |
| MODAL | 12 gestemde modes: klokken, staven, platen | rek van de boventonen (34 harmonisch, 70 klok, 116 staaf) | aanslagpositie |
| SYMP STRING | een snaar met drie meeresonerende snaren | het akkoord: OCT 5TH 4TH MAJ MIN SUS 7TH OCTAVES | hoeveel ze meenemen |
| STRING | een Karplus-Strong-snaar | onder ~32 een gebogen kam (sitar), daarboven dispersie (piano) | — |

KNOB 2 BRIGHTNESS, KNOB 3 DAMPING (zoals Rings: rechtsom langer), KNOB 4 MIX (0 = uit, dan rekent er niets). Het
scherm tekent het model als één 3D-draadmodel in vectorstijl, in het midden (dunne lijnen in perspectief, langzaam
draaiend, de achterkant gestippeld), met linksboven een pictogram en de naam (de klok, de snaar, de sitar, de fluit,
de zangvogel, de havik, een hoofd bij VOICE, de trompet, de viool, de klarinet of sax en de trom). Wat het model doet
zit in de animatie zelf: MODAL een draaiende klok waarvan de wand beweegt in de modes (2,0), (3,0) en (4,0), elk zo
sterk als de echte partial van de resonator; SYMP de bespeelde snaar met daarachter elke resonantie als snaar van zijn
lengte (1/f); STRING de eigentrillingen n = 1..4 achter elkaar en hun som vooraan; VOICE het spraakkanaal als buis
met de echte oppervlaktefunctie van de VOWEL en de lengte van de SIZE; DRUM het vel dat in zijn mode beweegt, op een
ketel of een romp; FLUTE de fluit zelf (kopstuk, middenstuk,
voetstuk, kleppen, lipplaat) met de staande golf en zijn knopen erin en de luchtstraal over het mondgat; SYRINX en HAWK
de luchtpijp met de syrinx en de twee bronchiën als buizen, de labia trillend, elk met zijn eigen stem.

**De waveguides (FX, achter de resonatoren).** Draai op de FX-pagina verder met ALGORITHM: na MODAL, SYMP STRING en
STRING komen de waveguides en de trom, eigen code (naar de klassieke modellen: het vocal tract van Kelly en Lochbaum, de fluit
van Cook / STK). Net als de resonatoren polyfoon (één per noot) en gevoed door de stemmen: de oscillator is de bron.
De luidheid van de stemmen is de "adem": de fluit en de syrinx blazen alleen zolang je speelt.

| Model | Wat | KNOB 1 | SELECT |
| --- | --- | --- | --- |
| VOICE | een menselijk spraakkanaal (het koor): een buis van 8 .. 12 secties met de oscillator als stembanden; de lengte geeft de formanten | SIZE: CHILD, SOPRANO, ALTO, TENOR, BASS (12 .. 18,5 cm) | VOWEL: U, O, A, E, I (morphend) |
| FLUTE | een luchtstraal over een buis op de toonhoogte van de noot | JET: laag = grondtoon (buigt iets omhoog), hoger = overblazen naar octaaf en duodecime | BREATH: de ruis in de adem |
| SYRINX | het stemorgaan van een vogel: twee fluitjes twee octaven boven de noot, door de snavel | CHIRP: hoeveel de toon stijgt met de adem | TWO VOICE: de tweede fluit 0..7 halve tonen hoger (zweven .. kwint) |
| HAWK | de schreeuw van een roofvogel ("kee-eeee-arr"), gebouwd op SYRINX: twee fluitjes een kwarttoon uit elkaar (ze zweven: de ruwheid), 2,5 octaaf boven de noot, de toon stijgt eerst iets en zakt dan een half octaaf over de schreeuw; ruisiger adem en een hogere, wijdere snavel | SCREAM: hoe lang de schreeuw duurt (0,1 .. 2,7 s, vanaf het moment dat er na stilte weer geluid binnenkomt) | RASP: hoe rauw (een snelle, willekeurige tremolo) |
| BRASS | lippen die in een buis zoemen: een korte luchtstoot per periode, smaller naarmate je harder blaast (luid is helder, zoals koper), door een buis van één periode (zijn modi de boventonen) en een beker die de hoge tonen uitstraalt | LIP: los en zacht (hoorn) .. strak en schetterend (trompet) | MUTE: open beker .. harmon-demper (een smalle band, het nasale "wah") |
| BOW | een strijkstok op een snaar (naar STK Bowed / Smith): twee vertragingslijnen, de wrijving van het haar (stick-slip), de kam en een klankkast (een lage resonantie en de 'bridge hill' rond 2,5 kHz); de luidheid van de stemmen is de snelheid van de stok | PRESSURE: licht en luchtig (flageolet) .. gedrukt en krassend | BOW AT: sul ponticello (bij de kam) .. sul tasto (boven de toets) |
| DRUM | een aangeslagen vel (tom, taiko, pauk): acht van zijn modes, elk een resonator, gestemd op de noot; hoe harder de slag, hoe strakker het vel: de toon begint hoger en zakt terug (de "doem") | KETTLE: vrij vel (Bessel-verhoudingen: tom, taiko) .. ketel (de modes bijna 1 : 1,5 : 2: een gestemde pauk) | STRIKE: midden (een doffe dreun) .. rand (een zingende toon) |
| PIANO | een pianotoon (naar de waveguide-piano's van Smith / Bank / Välimäki): twee snaren per noot die iets uit elkaar gestemd zijn (ze zweven, en de toon zakt in twee fasen weg), de stijfheid van het staal (de boventonen iets te hoog, meer naar boven toe: per noot een keten van tot acht all-pass-filters, uitgerekend op de inharmoniciteit B van een echte piano, `tools/piano_dispersion.py`, en elke noot zuiver gestemd), één keer aangeslagen door een echte hamer: een vilten massa die met de snelheid van de toets (0,4 .. 5 m/s, de velocity) tegen de snaar wordt gegooid, het vilt een veer die stugger wordt naarmate hij verder wordt ingedrukt (F = K c^2,5), de snaar die meegeeft en na een achtste periode de golf terugkaatst van het verre uiteinde die de hamer wegwerpt; zo wordt de klap korter en scherper naarmate je harder speelt (luid = helder, vanzelf) en hoger op het klavier (lichtere hamers, harder vilt: 11 g in de bas, 5 g bovenin; BRIGHTNESS maakt het vilt harder of zachter); alleen de hamer drijft de snaren aan (het geluid van de stemmen kleurt alleen de aanslag, de eerste ~20 ms van het vilt), dus een vastgehouden toets klinkt uit zoals bij een piano en wordt nooit gestreken; de aanslag klopt ook op de kast (een korte stoot door de klankbodem: de bons van een harde noot); de plaats van de hamer laat de boventonen daar weg; de demper valt als je de toets loslaat, behalve met het sustainpedaal (MIDI CC 64); een klankbodem | UNISON: zuiver .. honky-tonk (0 .. 12 cent uit elkaar) | HAMMER: waar de hamer slaat (1/16 .. 1/5 van de snaar) |
| ACCORDION | vrije rieten (accordeon, bandoneon, harmonium): elke riet een tong die door zijn spleet zwaait, de lucht gaat alleen door als hij uit de spleet is (een klep: per zwaai een luchtstoot; hoe harder de balg duwt, hoe wijder de zwaai en hoe korter de stoot: luider en scheller, het snerpen van een riet), door de rietkamer (een resonantie, BRIGHTNESS: 500 Hz .. 3 kHz, van de donkere cassotto tot het open rietblok). De balg is de luidheid van de stemmen (en aftertouch) | MUSETTE: hoe ver de rieten van de musette uit elkaar staan (0 .. 30 cent: droog .. nat, het zweven) | REGISTER: CLARINET (één riet, 8'), VIOLIN (twee iets uit elkaar), MUSETTE (drie: zuiver, laag, hoog), BASSOON (16'), MASTER (16' 8' 8' 4'); DAMPING = CHAMBER: hoe scherp de rietkamer resoneert |
| BAGPIPE | de doedelzak: de chanter een dubbelriet op een nauwe kegel (de REED-waveguide, het riet hard: luid, schel, snerpend) en de bourdons (één voor het hele instrument, niet per noot): een bas en twee tenoren, cilinders met enkelriet (oneven boventonen), twee octaven en een octaaf onder de toonsoort; de tenoren iets uit elkaar (ze zweven); ze klinken zolang de zak vol is (een noot vast, daarna ~0,7 s terwijl hij leegloopt) | SOFT REED: het riet zachter | DRONES IN: de toonsoort van de bourdons (A .. G#); DAMPING = DRONES: hoe luid (0: geen) |
| KALIMBA | een kalimba (mbira, sanza), opnieuw gebouwd naar de natuurkunde ervan (Chapman 2012, Rossing): een stalen tong op een brug, het lange eind vrij. De tong is een ingeklemde staaf met modi 1 : 6,27 : 17,5 (verre van harmonisch: geen bel en geen snaar); de grondtoon zingt 1,5..9 s (lage tongen het langst), de tweede sterft ~7× sneller, de derde meteen; een harde tokkel begint een paar cent te hoog en zakt terug (~60 ms). Het korte eind achter de brug rinkelt mee, hoog en kort (~2,6..4,4 kHz: de "ting"). De duim drukt de punt opzij en laat los: de tong begint vanuit die buiging (elke modus vanuit zijn deel ervan; harder en BRIGHTNESS: meer boventonen), met het tikje van de nagel; het geluid van de stemmen kleurt alleen die tokkel, dus de tong klinkt uit, ook als je de toets vasthoudt. Een holle kast met klankgat: de kracht op de brug laat de lucht (~210 Hz) en het blad (~470 Hz, ~1,25 kHz) meeklinken. STRUCTURE de vorm van de tong (de kale staaf .. een verzwaarde, gebogen punt: 1 : 5,3 : 13,5), DAMPING hoe lang (× ½..2), BUZZ (SELECT) de rammelaars van de mbira (kroonkurken en schelpen die meerammelen als de tong ver uitslaat). |
| DIDGERIDOO | de lippen zoemen in een lange, wijder wordende buis (de BRASS-waveguide, laag: de drone), en wat er een didgeridoo van maakt: de mond en keel van de speler ervoor, twee formanten over de drone (POSITION = MOUTH: oe .. aa .. ie, F1 250..700 Hz, F2 600..2400 Hz), die met de adem iets opengaan; DAMPING = hoeveel grom in de formanten. FREE EG op POSITION geeft de 'wa-wa'. De stemmen zijn de adem (circulair: vastgehouden noten dronen door) | LIPS: laag de drone, hoog een toot (overgeblazen) | MOUTH: de klinker |
| SHAKUHACHI | de FLUTE-waveguide, aan het eind aangeblazen: een lange luchtstraal en veel adem (de muraiki) | MURAIKI: hoeveel ademruis | MERI: de kin omlaag, de toon tot een halve toon omlaag gebogen en donkerder (FREE EG op POSITION: de buiging) |
| REED | een riet op een buis (naar STK Clarinet / Saxofony): het riet als klep, een cilinder (klarinet: oneven boventonen) of een kegel (sax, hobo, fagot: alle boventonen) | REED: zacht en donker .. hard en schetterend | BORE: onder 16 de cilinder (klarinet), daarboven de kegel, het riet op 6 .. 45 % van de lengte (laag een sax, hoog de dubbelrieten) |

**Blaasdruk op aftertouch.** Zodra er aftertouch binnenkomt (channel pressure of polyfonische aftertouch, via USB- of
TRS-MIDI van een keyboard dat het stuurt; het eigen klavier van de FM-1 heeft het niet), is aftertouch de adem (de
PRESSURE van een Yamaha VL1): niet ingedrukt klinkt een noot zacht en donker (0,35 x), helemaal ingedrukt luid en helder
(1,35 x). Voor BRASS, BOW en REED is dat de blaasdruk of stoksnelheid, voor de andere FX-modellen hoe hard ze aangeslagen
worden. **SCREAM** (een doel van de modwheel en FREE EG, onderaan de lijst op de EDIT-pagina) legt het eigen geluid
van de stemmen op de adem, van een vleugje tot het instrument erdoor gedreven: met het CHUA-circuit op hoge CHAOS
blaast de chaos het instrument (de "scream" van de VL1).

KNOB 2 BRIGHTNESS (mondopening/snavel/demping van de hoogte), KNOB 3 DAMPING (hoe lang de buis naklinkt), KNOB 4 MIX.
Combineer gerust: een noise-programma als adem in de FLUTE, een noise-tik als stok op de DRUM (TIMPANI, TAIKO,
MELODIC TOM), de ruis van een zaagtandcluster door de VOICE (CHOIR AAHS).

**TAPE (REC).** Een 4-sporen bandrecorder met een tapelus, opgenomen van de eigen uitgang van de synth en eronder
afgespeeld. De band loopt op het tempo waarop hij is opgenomen: verander je de BPM (of het MIDI-clocktempo), dan loopt
hij sneller of langzamer en gaat de toonhoogte mee, zoals bij varispeed op een echte bandrecorder; de lus blijft
hetzelfde aantal tellen. Lo-fi zoals tape: 11 kHz, 4-bit ADPCM (7,2 s per spoor), de hoogte rolt af rond 5 kHz.

| Bediening | Functie |
| --- | --- |
| KNOB 1..4 | een leeg spoor: draaien = dat spoor armen (ARMED); een opgenomen spoor: zijn volume (de fader) |
| ALGORITHM | ook armen, voor een spoor dat al is opgenomen (om het opnieuw op te nemen) |
| SELECT | wisselt tussen de twee menu's: TAPE (links) en SPEED (rechts) |
| PRESETS | de BPM (dezelfde als ARP en KARMA): snelheid en toonhoogte van de band volgen |
| PLAY | (knippert in het tempo zolang ARP of KARMA aan staat) de band starten (vanaf het begin van de lus) / stoppen |
| REC | één ronde opnemen op het ARM-spoor (de band start als hij stilstaat); nog een keer: stoppen. Met PLAY ingedrukt: het ARM-spoor wissen |
| OCT- + OCT+ | (ze knipperen op de TAPE-pagina's) beide kort indrukken = BOUNCE: alle sporen, elk op zijn fader, samen op spoor 1 (op 100); sporen 2..4 leeg voor nieuwe opnames, spoor 2 geARMd. (5 s vasthouden blijft UBOOT) |

**SPEED (REC, SELECT rechtsom).** Draai SELECT rechtsom op de TAPE-pagina: het SPEED-menu (linksom: terug naar TAPE;
REC en PLAY werken hier ook). KNOB 1 de globale BPM (40..250; ARP, KARMA en de band volgen; vrije varispeed). KNOB 2 SPEED: de snelheid van de
band in verhoudingen uit de boventoonreeks (reine stemming), elk op een stap van de chromatische toonladder: 1/2 (half
speed, -12), 8/15, 9/16, 3/5, 5/8, 2/3 (-7), 45/64, 3/4 (-5), 4/5, 5/6, 9/10, 15/16, 1/1, 16/15, 9/8, 6/5, 5/4 (+4),
4/3, 45/32, 3/2 (+7), 8/5, 5/3, 9/5, 15/8, 2/1 (dubbel, +12). Kies je er een, dan springt de globale BPM naar het tempo
dat daarbij hoort (het tempo van de band x de verhouding) en loopt de band precies op die verhouding: zuiver op de
toon en in de maat met ARP en KARMA (draai je daarna aan de BPM, dan is het weer vrije varispeed).
KNOB 3 LENGTH van de lus: 1 tel, 2 tellen, 1 maat, 2 maten (standaard), 4 maten (zo lang als de band past: 7,2 s, dus 4 maten vanaf ~133 BPM, daaronder 2 maten, dan licht de waarde op);
KNOB 4 de toonhoogte nu in halve tonen; onderin staat het tempo waarop de band is opgenomen.

De eerste opname op een lege band legt zijn tempo vast; zijn alle vier sporen gewist, dan is de band weer leeg. MIDI
START zet de band terug naar het begin van de lus. Opnemen vervangt wat er op het spoor stond; het spoor dat je opneemt
hoor je live. Het scherm toont de recorder in 3D: de spoelen draaien zo snel als de band loopt, de wikkels groeien en
krimpen over de lus, en op de band bij de kop lichten de vier sporen op met hun geluid (het ARM-spoor gestippeld).

**Op afstand (USB).** Een computer kan de FM-1 over USB-MIDI bekijken en bespelen (firmware/src/remote.c,
tools/fm1_remote.py; SysEx F0 7D 43 48 ...): `ping`, `state` (de pagina, de tekst op het scherm, preset, BPM, belasting,
tape, FX), `screen out.png` (het plaatje), `button REC`, `turn "KNOB 1" 5`, `note 60 100 0.5`, `cc 1 64`, `program 12`,
`watch 10 map/` (elke halve seconde een plaatje). Nodig: `pip3 install mido python-rtmidi Pillow`. De USB-audio-ingang
kan daarnaast met de gewone systeemtools worden opgenomen (ffmpeg of sox).

**De shimmer reverb (FX, tweede scherm).** Een stereo galm (4 vertragingslijnen met een Householder-matrix) met een
toonhoogteverschuiver in de terugkoppeling. Elke ronde door de galm (60–110 ms) schuift een deel van de staart een
interval omlaag en gaat terug de galm in, met behoud van energie: de staart zakt, de hoge octaven vervagen terwijl de
lage aanzwellen, een diepe onderstroom onder de klank (tot een high-pass rond 40 Hz de bodem afsnijdt).

| Bediening | Functie |
| --- | --- |
| KNOB 1 SIZE | de ruimte: 60–110 ms lange lijnen (glijdt bij draaien) |
| KNOB 2 DECAY | nagalmtijd 0,4–30 s |
| KNOB 3 SHIMMER | hoe snel het klimt: 0 is een gewone galm, 127 zo'n 2–3 octaven per seconde |
| KNOB 4 MIX | droog/nat; 0 = uit (dan rekent er niets) |
| ALGORITHM | het interval, omlaag: -OCT, -5TH, -OCT-5TH, -2 OCT |
| SELECT | TONE: donker (1,5 kHz) tot open (16 kHz) |

SPACE (met ALGORITHM, samen met het INTERVAL: HALL -OCT, HALL -5TH, ..., LESLIE -2 OCT) kiest het gebouw waarin de
galm klinkt: HALL (de gewone galm), TEMPLE (een stenen kamer: dichte vroege reflecties, de resonantie van ~110 Hz van
de oude orakelkamers en zijn buren, donker), CATHEDRAL (het schip: echo's van de pilaren op 23..89 ms, de lange lage
modes 49 / 98 / 147 Hz), MOSQUE (de koepel: een fladderecho dat heen en weer kaatst, zijn modes 165 / 330 / 495 Hz,
helder). Daarna de **klankkasten** van de instrumenten: het geluid door het hout, zoals het body-filter bij commuted
synthesis (acht resonanties per kast, aangeslagen door het droge geluid; de galm een achtste zo luid, als een kleine
kamer eromheen): PIANO BODY (de zangbodem: modes van 92 Hz tot 1,1 kHz, de reflecties van de klep), GUITAR BODY (de
lucht in de kast op 100 Hz, het blad op 204 / 395 / 550 Hz ...), VIOLIN BODY (A0 275 Hz, de B1-modes 460 / 540 Hz, de
brugheuvel rond 2,5..3 kHz), CELLO BODY (hetzelfde, ruim een octaaf lager), HARP BODY (een kast: brede, zachte modes van
90 Hz tot 2 kHz), ACCORDION (de kast met de rietkamers: een neuzige, holle klank, 150 Hz tot 3,4 kHz), BAGPIPE (de zak
en de bourdons: scherpe resonanties op Bes 116,5 / 233 / 349,5 / 466 Hz die meeklinken, en de schalmei-formanten tot
3,5 kHz), PIPE ORGAN (de orgelkast en de windlade, 55 Hz tot 2 kHz, met de volle galm van een kerk eromheen) en LESLIE
(de draaiende luidspreker van het hammondorgel: de hoge tonen uit een draaiende hoorn met zijn dopplereffect, de lage
uit een draaiende trommel, twee microfoons; SIZE is de snelheid, van 0,8 Hz (chorale) tot 7 Hz (tremolo)). De
snaarpresets gebruiken ze: piano's en klavecimbels de PIANO BODY, gitaren, banjo, sitar en
shamisen de GUITAR BODY, viool, altviool, fiddle en pizzicato de VIOLIN BODY, cello, contrabas en akoestische bas de
CELLO BODY, harp, koto, dulcimer, santur, yang qin, kalimba en mbira de HARP BODY; de accordeons, bandoneon en de rietorgels de ACCORDION, BAGPIPE en DRONE PIPES de BAGPIPE, de kerkorgels de PIPE ORGAN, de drawbar-, percussie-, rock- en synth-orgels de LESLIE (ROCK ORGAN, ROTARY ORGAN en CHUA ORGAN snel, de rest langzaam). De SHIMMER-presets (PRESETS-knop)
hebben er elk een: STONE TEMPLE, ORACLE CHAMBER, CATHEDRAL, GOTHIC CHOIR, MOSQUE DOME, DOME DEPTHS, PIANO BODY, GUITAR
BODY, VIOLIN BODY, CELLO BODY, HARP BODY, ACCORDION BOX, BAGPIPE, PIPE ORGAN, LESLIE SLOW, LESLIE FAST, naast UNDERWORLD,
SUB HAZE, SMALL ROOM. Het scherm toont het ornament van die ruimte als een
raam dat langzaam draait: een roosvenster (CATHEDRAL), een girih-ster (MOSQUE), een lotus-mandala (TEMPLE), golvende
ringen (HALL). De ringen van binnen naar buiten zijn de galmstaart van nu naar eerder, elk zo fel als de staart toen
was: het geluid stroomt naar buiten door het patroon; elke knop werkt over zijn hele bereik zichtbaar mee: SIZE de grootte van het
raam (en 4..7 ringen), DECAY hoe ver de ringen terug in de staart reiken (0,8..7,6 s), SHIMMER hoeveel stappen de
buitenste ringen klimmen (0..3, elk met meer en fijnere elementen in de verhouding van het INTERVAL), TONE hoe fel de
buitenkant blijft, MIX hoe fel het geheel is. Het ornament beweegt op de BPM (of de MIDI clock): elke ring stapt één element verder op de onderverdeling van de tel, de binnenste op de kwarten, dan de achtsten, de buitenste op de zestienden, met een snelle zachte beweging en dan stil; de ringen om en om de andere kant op. De uitgang is nu stereo; met de shimmer uit blijft hij mono en bit voor bit hetzelfde.

**De DELAYS (SHIMMER, de laatste drie SPACEs).** In plaats van een galm een delay in het tempo (de BPM, of de MIDI
clock), in het geheugen van de galm (op een kwart van de samplerate, 11 kHz: tot ~2,5 s, donker en zacht als een
tape-echo). Elke delay is een patroon van taps op één lijn: elke tap een tijd na de noot, een plek van links naar
rechts, een niveau en een eigen filter (een resonerend laagdoorlaatfilter: de echo's om en om donkerder, helderder); het
eind van het patroon wordt teruggevoerd, zodat het patroon zachter terugkomt. Met ALGORITHM kies je de delay en zijn
patroon:

| Delay | Patronen |
| --- | --- |
| PING PONG | STRAIGHT (links na een stap, rechts na twee), DOTTED (anderhalve stap), TRIPLET (2/3), FILTER (links donker, rechts helder) |
| PATTERN DLY | TRESILLO (3 + 3 + 2), DUB (gepunteerd en de stap erna, donker), GALLOP, CLAVE (de son-clave 3-2, een maat) |
| BOUNCE DLY | BALL (een vallende bal: elke stuit eerder en zachter, rollend naar rechts), BALL PONG (stuiterend van links naar rechts), RISE (de bal achteruit: steeds langzamer en luider, over het stereobeeld), SCATTER (taps verspreid, elk een eigen filter) |
| MOUTH | het geluid door een mond, zoals een talkbox: vier formanten van het spraakkanaal maken er een klinker van. KNOB 3 (VOWEL, in plaats van SHIMMER) beweegt de mond langs het pad van ALGORITHM: U O A E I, WAH (U naar A), YAI (A naar I), OUI (O U I); SIZE de mond (groot: lager, klein: hoger), DECAY en TONE het kamertje eromheen. Met FREE EG op SHIMMER praat hij: de LFO-presets TALK en YOI YOI. Presets op de FX-pagina: TALK BOX, WAH MOUTH, YAI VOICE, GIANT MOUTH. Op het scherm een mond die opengaat met de klinker en meebeweegt met het geluid |

| Bediening (delay) | Functie |
| --- | --- |
| KNOB 1 TIME | de stap: 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T, 1/8. (gepunteerd), 1/4 (gehalveerd als het patroon niet in de lijn past) |
| KNOB 2 FEEDBACK | hoeveel van het patroon terugkomt (0 .. 95 %) |
| KNOB 3 FILTER | de cutoff van de taps, 150 Hz .. 2,6 kHz (elke tap zijn eigen afwijking daarvan), 127 = OPEN (geen filter) |
| KNOB 4 MIX | zoals bij de galm |
| SELECT | RESO: de resonantie van de filters (geen .. een fluittoon) |

Het scherm toont het podium van voren, links .. rechts: elke tap een ring op de vloer waar hij klinkt (zo groot als hij
luid is), en een bal die vanuit het midden (de noot) van tap naar tap stuitert in het tempo, elke sprong zo hoog als hij
lang is; de ringen lichten op als de bal landt. Presets (PRESETS op de SHIMMER-pagina): PING PONG 1/8, DOTTED PONG,
TRIPLET PONG, FILTER PONG, SLAPBACK, TRESILLO ECHO, DUB SIREN, GALLOP, CLAVE ECHO, BOUNCING BALL, BALL PONG, RISING BALL,
SCATTER DELAY.

Het scherm toont de toestand (x, y, z) van de nieuwste stem; speelt er niets, dan een "spookcircuit" dat op de huidige
instellingen meedraait. Dat spookcircuit levert ook de beginstand van nieuwe stemmen (al op de attractor, met een kleine
afwijking per stem zodat akkoorden niet in de pas lopen).

## Waar begin je

Alles wordt als één bestand gecompileerd: `firmware/src/felucca.c` bepaalt de volgorde.

| Bestand | Wat |
| --- | --- |
| `firmware/src/synth.c` | het Chua-circuit: diode, RK2-solver in Q20 fixed point, stemmen, uitgang, de trace voor het scherm |
| `firmware/src/noise.c` | de 30 noise-oscillators (naar Befaco's Noise Plethora) |
| `firmware/src/shimmer.c` | de shimmer reverb: FDN-galm met octaafverschuiver in de terugkoppeling |
| `firmware/src/waveguide.c` | de waveguides: VOICE (Kelly-Lochbaum), FLUTE, SYRINX, HAWK, BRASS, BOW, REED; de trom DRUM |
| `firmware/src/resonator.c` | de FX-resonatoren rond de Rings/DaisySP-port (`phys_dsp.c`, `phys_symp.c`, MIT) |
| `firmware/src/lpg_tables.h` | gegenereerd door `tools/gen_lpg_tables.py`: ADSR-tijden, cutoff, resonantie |
| `firmware/src/chua_tables.h` | gegenereerd door `tools/gen_chua_tables.c`: α, omlooptijd en RMS per (SHAPE, CHAOS) |
| `firmware/src/ui.c` | de attractor tekenen, knoppen, presets, LEDs, instellingen opslaan |
| `firmware/src/keys.c` | toetsen en MIDI in → noten (draait in de audio-interrupt, elke 32 samples) |
| `firmware/src/audio.c` | de audio-interrupt: `out[]` vullen met Q15 stereo |
| `firmware/src/main.c` | opstarten en hoofdlus (met watchdog, boot-guard, UBOOT en update) |
| `firmware/src/panel.c` | welke matrix-knop welke opdruk heeft |
| `firmware/src/core.h` | gedeelde constanten en de `sys`-struct (hoofdlus ↔ audio) |
| `firmware/src/storage.c` | flash-opslag met A/B-kopieën; `OBJ_USER0..7` zijn vrij |
| `firmware/src/gfx.c`, `lcd.c` | tekenen: tekst (Inter Tight), rechthoeken, lijnen; kleuren via `T_BG`, `T_TEXT`, `T_THEME` … |
| `firmware/src/usb.c`, `midi_uart.c` | USB-MIDI/-audio en TRS MIDI (ongewijzigd uit Felucca) |
| `firmware/src/ota.c`, `ota_hw.c`, `firmware/loader/` | het update-pad: **laten staan** |
| `firmware/hal/` | registers: toetsmatrix, LEDs, encoders, LCD, I2S, ADC, flash, tweede core |

Regels van het huis (uit Felucca, en `tools/build.py` controleert ze): registertoegang alleen in `hal/`, alles in de
audio-interrupt is fixed point (geen 64-bit delingen: die heeft de toolchain niet), en de audio-interrupt moet binnen
~2,9 ms klaar zijn.

**CPU en de tweede core.** CHUA speelt 4 stemmen, net zoveel als er FX-resonatoren/waveguides zijn (één per noot):
met de waveguides erbij was 8 te zwaar. Een vijfde noot neemt de oudste stem over. De AC79 heeft een tweede core; CHUA
verdeelt de stemmen en de resonatoren (om en om) over beide. De tweede core start alleen bij het opstarten, dus **SELECT op de GLO-pagina** zet hem aan of
uit voor de volgende start (meteen opgeslagen; standaard aan). (Op de MIDI-pagina staat of hij aan staat.) Op sommige
FM-1's leest de tweede core verkeerd (zo gevonden door X0X en Jangada): dan merkt de firmware dat, gaat verder op één
core en zet de instelling uit (`2ND CORE STOPPED: OFF`). De uitvoer is bit voor bit gelijk, met één of twee cores
(getest, ook als echte thread onder ThreadSanitizer). `NVOICE` in `core.h` zet het aantal stemmen.

**Tabellen opnieuw maken** (na een andere diode of ander bereik), ~1 minuut:

```
cc -O2 -o build/gen_chua_tables tools/gen_chua_tables.c -lm && build/gen_chua_tables > firmware/src/chua_tables.h
```

## Installeren vanaf GitHub

Elke push naar `main` bouwt de firmware op GitHub (Actions), draait de tests en zet de installer online:

**https://plieuw.github.io/hawk-chua/** → open in Chrome of Edge, sluit de FM-1 met een USB-datakabel aan, druk op
*Installeren*. Volgende versies gaan op dezelfde manier; terug naar de officiële firmware kan op dezelfde pagina.

Werkt op beide uitvoeringen van de FM-1: de eerste met een Puya P25Q80-flashchip en de nieuwere (verkocht vanaf
het najaar van 2026, logo op de voorkant gedrukt) met een UC25HQ80; op beide worden presets en instellingen opgeslagen.

- **Handleiding (PDF):** https://plieuw.github.io/hawk-chua/HAWK-CHUA-handleiding.pdf (ook in `docs/`).
- **In de browser spelen, zonder FM-1:** https://plieuw.github.io/hawk-chua/webapp/try/ — dezelfde broncode
  (synth, toetsen, scherm, LEDs) als WebAssembly (`web/emu/`, `sh web/emu/build.sh`; clang + lld). Toetsen met muis,
  touch of het toetsenbord (A W S E D …, Z/X = OCT), een MIDI-keyboard via Web MIDI (CC 20–26 = de draaiknoppen,
  CC 27 = volume), SAVE bewaart in de browser. Opzet naar Felucca's (inmiddels verwijderde) browserversie: elke
  knop/toets-wissel blijft 24 ms staan; hier draait de firmware in een Worker die de audio vooruit rendert.
- Het `.fwsc`-bestand zelf staat bij elke run onder *Actions → firmware → Artifacts*.
- Een tag `v1.0` (één cijfer per deel) maakt een release-build en een GitHub Release met het bestand.
- Eenmalig instellen: *Settings → Pages → Build and deployment → Source: GitHub Actions*.

## Bouwen, testen, installeren

Zie [BUILDING.md](../BUILDING.md). Kort:

```
tools/get_toolchain.sh                 # JieLi-toolchain (eenmalig)
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
pip3 install Pillow fonttools mido python-rtmidi
./build.sh                             # -> build/hawk-chua.fwsc
tests/run_tests.sh                     # host-tests, geen FM-1 nodig (+ build/host/ui/sheet.png en basis.wav)
python3 tools/fm1_install.py build/hawk-chua.fwsc
```

Op macOS draait de toolchain in Docker (zie BUILDING.md); op Linux x86-64 direct.

**Veiligheid.** De installers praten met de draaiende firmware (`ota.c`). Haal dat er niet uit, anders kun je de
volgende build alleen nog via UBOOT installeren. Lukt opstarten twee keer achter elkaar niet binnen 30 s, dan gaat de
FM-1 vanzelf naar UBOOT. Herstel na een mislukte installatie: [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
Terug naar de officiële firmware: `python3 tools/fm1_install.py FM-1.fwsc` met het V15-bestand van M-VAVE.

## Licentie

De interactieve uitleg (circuit, diode, bifurcatiekaart, speelbaar in de browser met dezelfde tabellen) staat in
`docs/chua.html`.

GPL-3.0-only, net als Felucca ([LICENSE](../LICENSE), [LICENSING.md](../LICENSING.md), [CREDITS.md](../CREDITS.md)). Deel je
een build, deel dan ook de broncode. De namen `FELUCCA_*` in de code zijn nog die van het origineel. De USB-naam is
"HAWK CHUA FM-1" (de update-loader: "HAWK CHUA FM-1 OTA"), met "FM-1" erin zodat elke FM-1-updater hem vindt
(`firmware/src/usb.c` STR1/STR2; geef een eigen fork een eigen VID/PID vóór je iets uitbrengt). De updaters
(`web/fm1ota.js`, `tools/fm1_install.py`) zoeken eerst op bekende poortnamen en proberen daarna elke poort die op de
FM-1-handshake antwoordt, dus ook vanaf andere firmwares; `--port` kiest er zelf een.
