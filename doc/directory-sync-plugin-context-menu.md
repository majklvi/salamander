# Kontextové menu výsledků — průzkum a návrh

Stav 16. října 2026. **První rozsah menu je implementovaný; níže zůstává původní průzkum a návrh, na konci je aktuální evidence.** Aktuální funkce a omezení popisuje [návod](directory-sync-plugin.md). Kontextová nabídka schránky v tabulkových souhrnech je samostatná existující funkce.

## Aktualizace 2026-10-17: okamžité odstranění

Návrhové příkazy nyní výslovně říkají „Označit ke kopírování…“ a „Označit k přeskočení“. Oddělená sekce **Odstranit vybrané…** s levou/pravou/oběma stranami je implementovaná: bez Shiftu do Koše, se Shiftem při aktivaci strany trvale; vždy vlastní potvrzení výběru. Není součástí synchronizačního plánu a nepoužívá Uchovat originály. [Aktuální návrh a skutečné NTFS/GUI ověření](directory-sync-plugin-immediate-delete.md). Následující oddíly zachovávají historický průzkum a starší rozsah.

## Provedený průzkum

Skutečný Computer Use průchod v referenčním správci souborů, verze **11.58 x64**, dialog Synchronize directories. Použita pouze místní izolovaná NTFS sada `C:\Samandarin\builds\dirsync-menu-20261016\left` a `right`. Čtyři dvojice souborů, levostranný a pravostranný Unicode soubor a podadresář `sub` s jedním souborem na každé straně: osm výsledných souborových řádků, dvanáct fyzických souborů. Ověřeno obousměrné i asymetrické porovnání, Subdirs a by content; maska `*.*`.

Průzkum prováděl jen porovnání a změny návrhů v paměti. Synchronize ani položky Delete, Edit, Rename, kopírování vlastností, tisk a shellové nabídky nebyly spuštěny. Po zavření dialogu byly ověřeny všechny původní hashe a časy dvanácti souborů proti `before.json`: beze změny. Původní hlavní panely nebyly přesměrovány; aplikace zůstala spuštěná a byla vrácena do původního minimalizovaného stavu.

## Viditelné příkazy nabídky

Nabídka má následující skupiny, oddělené standardními separátory. Klávesy byly přečteny přímo z nabídky; funkčně se ověřovaly jen níže výslovně uvedené příkazy.

| Skupina | Příkazy a zobrazené klávesy |
| --- | --- |
| Návrhy kopírování | Mark for copying (default direction) — Ctrl+D; Unmark for copying — Ctrl+M; Mark for copying → (left to right) — Ctrl+L; Mark for copying ← (right to left) — Ctrl+R; Mark for copying in reverse direction — Ctrl+W |
| Vlastnosti pro přenos | Copy file properties → (left to right)… — Ctrl+Alt+L; Copy file properties ← (right to left)… — Ctrl+Alt+R |
| Tisk a porovnání | Print selected… — Ctrl+P; Compare left & right — Ctrl+F3 |
| Práce s jednotlivou stranou | View left — F3; View right — Shift+F3; Edit left — F4; Edit right — Shift+F4; Rename left — F6; Rename right — Shift+F6 |
| Shell a vlastnosti | Left context menu — Alt+Down; Right context menu — Alt+Shift+Down; Left properties — Alt+Enter; Right properties — Alt+Shift+Enter |
| Mazání | ← Delete left; → Delete right; Delete on both sides; bez zobrazené klávesové zkratky |

## Skutečně ověřené chování

| Situace | Pozorování |
| --- | --- |
| Pravé kliknutí na zvýrazněný soubor | Otevře nabídku; předchozí výběr zachová. |
| Dva řádky zvýrazněné přes Shift+Down; pravé kliknutí na jeden z nich | Oba zůstaly zvýrazněné. Volba směru doprava nastavila šipku u obou, včetně původně shodného souboru. Ostatní řádky se nezměnily. |
| Pravé kliknutí na jiný, dosud nezvýrazněný řádek | Původní dvojici nahradil jen nově kliknutý řádek. |
| Shift+F10 při zaměření seznamu | Otevřelo stejnou nabídku pro aktuální výběr; Escape ji zavřel bez změny návrhu. |
| Pravé kliknutí v prázdné části seznamu při existujícím výběru | Otevřelo nabídku pro dosavadní zvýrazněný řádek. Neoznačilo automaticky všechny výsledky. |
| Pravé kliknutí na skupinové záhlaví `sub` | Zvýraznilo oba zobrazené souborové řádky této skupiny a otevřelo nabídku. Edit/Rename, shellové kontexty/vlastnosti a párové porovnání byly neaktivní; View left/right zůstaly dostupné. Nejde o důkaz provádění operací nad adresářem. |
| Odlišný soubor na obou stranách; Mark for copying → | Jen změna značky na zelenou šipku. Žádný přenos se nespustil. |
| Soubor pouze vlevo | View/Edit/Rename/Context menu/Properties pro pravou stranu byly neaktivní, stejně jako reverse direction a kopírování vlastností. Oba explicitní příkazy směru zůstaly dostupné. Volba směru zprava doleva **zrušila původní značku**, nevytvořila přenos z neexistujícího souboru. |
| Filtrovaná sada | Po skrytí směru doprava zůstaly tři viditelné soubory. Ctrl+A a Ctrl+M odstranily značky jen těmto řádkům. Po opětovném zobrazení doprava zůstaly skryté návrhy včetně ručně označené shodné dvojice zachované. |
| Asymetrie, pravostranný Unicode soubor | Porovnání navrhlo červené ×. Nabídka měla dostupné příkazy existující pravé strany a Delete right; levostranné protějšky byly neaktivní. Explicitní směry kopírování byly stále dostupné — jejich provedení zde ověřeno nebylo. |

Dostupnost Compare left & right u některých jednostranných řádků sama o sobě nedokládá, jak příkaz chybějící stranu řeší. Z dostupnosti položek Delete nelze odvodit potvrzení, okamžik mazání ani jejich vztah k hlavnímu Synchronize; tyto položky se záměrně neaktivovaly. Neověřovala se klávesa Menu, stav bez jakéhokoli výběru, archivy, SMB, reparse/ACL blokování ani jiné verze aplikace. Nejde o požadavek kopírovat všechny funkce do pluginu.

## Doporučená první podoba pro plugin

Použít nativní popup menu přes současné SDK, standardní font, hostitelské barvy a separátory. Žádný nový vlastní vizuální styl. První část menu **upravuje pouze návrhy zvýrazněných viditelných souborových řádků**; přenosy, přepisy a mazání nadále spouštějí samostatná existující tlačítka se svým potvrzením.

```text
Restore default proposal                 Ctrl+D
Skip                                    Ctrl+M
Copy to right                           Ctrl+L
Copy to left                            Ctrl+R
Reverse copy direction                  Ctrl+W
Mark right-only files for deletion
─────────────────────────────────────────────
Copy paths                              ▸
    Relative paths                      Ctrl+C
    Left full paths
    Right full paths
```

Jde o navržené anglické popisky, nikoli nové existující příkazy. Zkratky by platily jen při zaměření seznamu výsledků, aby nezasahovaly do textových editací. Položka pro mazání musí zřetelně říkat **Mark**: mění pouze návrh, sama nemaže.

Pravidla rozsahu a aktivace:

- Pravé kliknutí na zvýrazněný řádek zachová výběr; na jiný řádek nejprve vybere právě tento řádek. Shift+F10 / Menu použijí aktuální výběr. Pravé kliknutí do prázdna zachová výběr; bez výběru se příkazy pro návrhy deaktivují. Nikdy se implicitně nepoužijí všechny viditelné výsledky.
- Směr se nabídne jen pro existující zdroj a podporovaný pravidelný soubor. V asymetrii bude Copy to left i Reverse neaktivní podle D3, přestože referenční nabídka explicitní směry zobrazuje dostupné. Chybějící zdroj se nebude tiše interpretovat jako Skip.
- Mark right-only files for deletion se aktivuje pouze v asymetrii pro soubory existující pouze vpravo. Žádné Delete left/both, mazání složek ani okamžitá destruktivní operace v první nabídce.
- Blokované a konfliktní řádky zůstanou nepřepisovatelné stejně jako nyní. Při smíšeném výběru se uplatní jen způsobilé řádky a stavový řádek uvede počet změněných a vynechaných. Skryté řádky zůstanou beze změny; Skip lze explicitně změnit zpět na platný návrh.
- Restore default proposal obnoví návrh z právě platného porovnání, nikoli z nové historie nebo skrytého opětovného skenu. In-memory kopie výchozích návrhů bude patřit k aktuálnímu výsledku a zneplatní se s ním.
- Kopírování cest použije Unicode schránku, jednu cestu na řádek a jen existující příslušnou stranu. Nezpřístupní neexistující cestu jako skutečný soubor. U neplatného výsledku se změny návrhů zakážou; pro neškodné zkopírování zobrazených cest není nutný nový sken.
- Náhled pluginu má ploché relativní cesty, nikoli skupinová záhlaví referenčního dialogu. Kliknutí na adresářový řádek proto nebude bez samostatného návrhu implicitně označovat celý podstrom.

## Nejmenší technické mapování a navazující možnosti

`WM_CONTEXTMENU` + hit-test nativního ListView, `LVN_KEYDOWN` pro zkratky a menu z existujícího `CSalamanderGUIAbstract` / `CGUIMenuPopupAbstract`. Cílové indexy se zachytí před otevřením menu. Změny návrhů použijí stejnou čerstvou validaci a pravidla způsobilosti jako `ValidatePreview(Intent::Apply)` / `Apply`, ne druhou nezávislou prováděcí cestu. Restore default vyžaduje jen uchovat původní `Action` u aktuálních řádků; aktuální model `Row` si po ruční úpravě původní návrh samostatně nepamatuje. Formát synchronizační historie ani SDK se kvůli tomu nemění. Pro schránku se využije stávající Unicode postup tabulkových souhrnů.

Prohlížení, porovnání obsahu, editace, přejmenování, kopírování vlastností, tisk a shellové nabídky doporučuji do první implementace menu nezahrnout. SDK má například `ExecuteAssociation` a `ViewFileInPluginViewer`, ale první neznamená interní View a druhá vyžaduje konkrétní prohlížecí plugin. Přímé univerzální volání interního porovnání/editoru tu nebylo nalezeno. Tyto příkazy proto potřebují nejprve konkrétní mapování na současný hostitel, včetně dlouhých Unicode cest a následného zneplatnění změněných výsledků. Není důvod kvůli nim nyní rozšiřovat SDK.

Původní plán ověření první implementace zahrnoval: jeden a více řádků, pravé kliknutí na jiný řádek a do prázdna, klávesnice, filtrované/Skip/blokované položky, asymetrie, Unicode schránka a změněný snapshot. Aktuální provedené testy a jejich meze uvádí následující záznam. Bez mazání složek, SMB testů a změn SDK.

## 2026-10-16 — implementace a ověření první nabídky

Implementováno nativním menu současného SDK: všech šest návrhových příkazů z tabulky a podnabídka tří druhů cest, uvedené zkratky a Ctrl+A. Indexy zvýrazněných viditelných řádků se zachytí před menu a validací. Změny používají stávající background Revalidate/Apply; žádný příkaz menu nespouští executor. Výchozí akce jsou v samostatném vektoru aktuálního náhledu. Při filtrování se zvýraznění převádí podle modelových indexů, nikoli podle pořadí na obrazovce, a skryté řádky se z výběru odstraní. Pro velký výběr je převod přes binární hledání, nikoli kvadratické procházení. Dostupnost kopírování cest nevyžaduje předem sestavovat celý text schránky.

Computer Use v izolovaném Samandarinu 0.16 x64, místní NTFS sada `C:\Samandarin\builds\dirsync-menu-impl-20261016`, šest logických souborových řádků:

| Ověření | Výsledek |
| --- | --- |
| Jeden řádek / dvojice přes Shift+Down, Ctrl+L | Změna právě 1 / 2 návrhů; i shodný soubor lze explicitně označit. Ostatní návrhy zachované. |
| Restore default z menu | U dvojice obnoveno původní Choose direction / Skip bez nového skenu. |
| Shift+F10, Escape a pravé kliknutí do prázdna | Stejné menu; Escape beze změny; existující dvojice zůstala vybraná. |
| Skrytí směru doprava, Ctrl+A a Ctrl+M | Tři viditelné řádky: 2 změněné, 1 už Skip. Tři skryté šipky doprava zůstaly po zobrazení zachované. Výběr se nepřenesl na jejich místa. |
| Pravé kliknutí na jiný řádek včetně velmi dlouhého názvu | Výběr se nahradí jedním kliknutým řádkem. Původní SubItemHitTest u dlouhého názvu nedal správný výběr; finální zásah používá skutečné geometrické meze zobrazených řádků. Oprava opakovaně ověřena v GUI. |
| Jednostranný dlouhý Unicode soubor | Chybějící směr, Reverse a pravá úplná cesta neaktivní. Copy paths → Left full paths vloženo zpět do textového pole a porovnáno: přesně **282 UTF-16 jednotek / 710 UTF-8 bajtů**. |
| Ctrl+C nad stejným souborem | Přesná relativní cesta, **229 UTF-16 jednotek**, žádná ztráta CJK ani českého názvu. |
| Asymetrický pravostranný soubor | Směry a Reverse neaktivní. Skip a následné Mark right-only files for deletion jen změnily návrh; fyzický soubor zůstal. |
| Záměrná změna mtime testovacího newer.txt po skenu a Ctrl+M | Revalidace zneplatnila celý snapshot; všech šest řádků Blocked, žádná zamýšlená změna návrhu neprovedena. Menu změn neaktivní, Copy paths dostupné. |

Regrese v testovací DLL zahrnuje **64 kontrol** skutečných produkčních helperů: matici způsobilosti šesti typů řádků a sedmi příkazů, chybějící strany, asymetrii, Restore/Reverse, smíšený výběr, prázdný výběr, dlouhé Unicode seznamy obou stran a zásah posledního řádku / prázdna / bodu mimo seznam. Celkem prošlo **2 354 lifecycle kontrol**. Resource/font testy Debug i Release prošly **2 477 kontrolami** při 96/120/144/192/240 font DPI; nejde o přepnutí monitoru za běhu.

GUI průchod nespouštěl přenosy, přepisy ani mazání. Vytvoření prázdné složky pickerem je samostatný test níže v GUI evidenci; záměrně změněný čas newer.txt je výslovná fixture změna, ne činnost menu. Testovací hostitel byl běžně ukončen, uživatelské instance nedotčené. Runtime ostatních architektur, přechod DPI mezi monitory, klávesa Menu a všechny kombinace více vybraných cest ve skutečné schránce se tímto průchodem netestovaly; seznamy více cest mají nativní regresi. Bez mazání složek, SMB testů a změn SDK.
Finální kontrola modulů z distribučního ZIP navíc ověřila nativní separátor a Reverse u jediného párového Unicode souboru: právě jedna změna doprava → doleva, bez přenosu dat. Viz [distribuční záznam](directory-sync-plugin-distribution.md).

## 2026-10-16 — přesun příkazů z dolní části okna

Přes Computer Use byl znovu prohlédnut referenční správce souborů 11.58 x64: Commands → Synchronize Dirs nad dřívější izolovanou sadou `dirsync-menu-20261016/left,right`, bez souborových operací. Hlavní dialog má Compare vlevo, filtry uprostřed vpravo, Synchronize a Close vpravo nahoře a seznam až ke stavovému řádku. Změny značek jsou v kontextovém menu. Synchronize otevřelo samostatné potvrzení s **3 kopiemi doprava / 12 bajty**, **0 kopiemi doleva** a **2 pravostrannými soubory k odstranění**; dále Confirm overwrites, Verify a volba kopírování oprávnění. Dialog byl zrušen přes Escape. Jedna zvýrazněná řádka neomezila tyto počty. Příkaz odpovídající našemu samostatnému Save originals ani samostatné tlačítko Plan summary nebyly v prohlédnutém synchronizačním dialogu/menu nalezeny; netvrdíme tím neexistenci jiných funkcí aplikace. Počet/stav je dostupný v jeho stavovém řádku a potvrzení. Přenos nebyl spuštěn, hlavní panely C:/D: zůstaly nezměněné, aplikace byla opět minimalizována.

Plugin nyní nemá dolní Proposal/Apply ani řadu operačních tlačítek. Změny návrhů jsou dostupné přes dosavadní menu/zkratky. Synchronize… nahoře vpravo otevře nativní nabídku tří stávajících oddělených druhů běhu: nové kopie, přepisy, asymetrické mazání souborů. Není to nový smíšený executor. Každá volba nejprve ověří snapshot a ukáže původní potvrzení OK/Cancel. Nezpůsobilé příkazy jsou neaktivní podle aktuálního viditelného plánu.

Kontextové menu má oddělené skupiny: návrhy vybraných řádků; Copy paths; **Visible plan**. Poslední podnabídka obsahuje souhrn, čáru, tři operační příkazy, čáru a Save visible originals. Tyto položky mají záměrně celý viditelný rozsah a fungují i bez zvýraznění. Save je neaktivní při Keep originals OFF. Skryté a Skip návrhy se nadále neprovádějí. Původní tvrzení v historické etapě, že žádná položka menu nemůže spustit executor, se vztahovalo na první implementaci; nyní platí pro návrhovou část a Copy paths. Plan příkazy mají vlastní potvrzení stejně jako dřívější tlačítka.

Nativní regrese obsahuje nyní **77 kontrol** návrhů, cest, hit-testu a dostupnosti plan příkazů: bez snapshotu, prázdný viditelný plán, skryté položky, nový/existující cíl, obnova ON/OFF, asymetrie a zneplatněný/zrušený/chybný/běžící plán. Computer Use ověřil bez výběru deaktivované návrhy, dostupný Plan summary (1 / 6 bajtů doprava), správné separátory, Alt+Y a potvrzení jednoho viditelného přepisu bez výběru. Potvrzení bylo zrušeno; žádný executor se v tomto GUI průchodu nespouštěl.
