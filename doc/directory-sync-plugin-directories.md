# Návrh mazání adresářů a zacházení se skrytými položkami

Redakční příprava exportu (17. října 2026): názvy referenčního programu a odkazy na jeho web jsou nahrazeny neutrálním označením. Historické zdroje v plném znění zachovává [archiv původního forku](https://github.com/majklvi/salamander/tree/af0e81bc1bccb6c733025fe31a97d53adf5cb32f/doc). Cesty s prefixem `reference-sync-probe-` jsou dokumentační aliasy, nikoli přejmenované testovací adresáře. Výsledky a meze ověření se nemění.

16. října 2026. Návrh níže byl schválen a implementován; aktuální provedení a výsledky jsou v závěrečné části. Bez SMB testů a změn SDK.

## Zjištění z GUI referenčního správce souborů

Computer Use, instalovaná verze 11.58 x64, Synchronize directories. Izolovaná místní NTFS sada `C:\Samandarin\builds\dirsync-dir-research-20261017\left` a `right` (koncovka je pouze identifikátor sady, datum průchodu je uvedeno výše). Vlevo `common.txt`, vpravo jeho shodný protějšek a sedm jednostranných souborů: běžný, Hidden, System, Hidden+System, soubor ve skrytém adresáři, soubor v `tree\nested` a `excluded-only\keep.bin`. Pět pravých adresářů včetně skutečně prázdného `empty`. Asymmetric, Subdirs a by content zapnuté. Filtr shodných položek byl vypnutý.

| Varianta po Compare | Pozorování |
| --- | --- |
| `*.*`, Empty directories ON | Všech sedm jednostranných souborů včetně H/S a obsahu skrytého adresáře dostalo značku odstranění. Pět pravých adresářů mělo také značku odstranění, včetně neprázdných předků. Potvrzení: Delete 12 file(s); globální Delete all empty directories zakázané. |
| `*.*`, Empty directories OFF | Sedm souborových návrhů; adresáře jen jako skupinová záhlaví bez značek odstranění. Potvrzení: Delete 7 file(s); samostatná volba Delete all empty directories dostupná a nezaškrtnutá. |
| `*.txt`, Empty directories OFF | Šest jednostranných souborů, H/S nadále zahrnuté; `keep.bin` a jeho záhlaví zmizely. |
| `*.txt`, Empty directories ON | Šest souborů a čtyři adresáře se značkami odstranění. `excluded-only` obsahující pouze maskou vynechaný `keep.bin` se nenabídl. |

V souborových řádcích neměly H/S zvláštní viditelnou značku atributu ani odlišnou barvu názvu vůči běžnému jednostrannému souboru: všechny byly modré se stejnou značkou odstranění. Jde o pozorování aktuálního barevného nastavení této instance, nikoli obecnou záruku všech konfigurací.

F1 přímo ze souhrnu uvádí, že globální Delete all empty directories může odstranit i pravé prázdné adresáře, které stále existují vlevo. **Globální úklid není totéž jako odstranění pravostranných položek plánu.** Tento průchod nově zjistil jeho dostupnost při Empty directories OFF v uvedené místní sadě; starší evidence tuto podmínku neurčila.

Žádné potvrzení provedení nebylo přijato, souhrny byly stornované. Nebylo testováno skutečné pořadí odstranění, chování při chybě či Stop ani obnovování adresářů v referenční aplikaci. Z náhledu se takové závěry nevyvozují. Skryté položky byly zahrnuté v aktuální konfiguraci; alternativní INI režimy nebyly v GUI přepínané.

Dokumentované alternativy pro H/S: parametr `SyncHideHiddenSystem` rozlišuje zahrnutí všech položek (0), vynechání při H/S na obou stranách (1) nebo na kterékoli straně (2); přičtení 4 navazuje na nastavení hlavního programu. Jde o nápovědu citovanou ve fóru předlohy (historický zdroj v archivu původního forku), dostupnou také v zveřejněném feedu (historický zdroj v archivu původního forku). Jednotlivé režimy včetně jejich dosahu na adresáře tento průchod nepotvrdil. Obecný popis: Synchronize directories (historický zdroj v archivu původního forku).

## Současné chování pluginu

`engine.cpp` enumeruje přes FindFirstFileW/FindNextFileW a nevynechává Hidden ani System. Pro ně platí běžné masky, rozsah a ochrany; nepodporované položky zůstávají blokované. Současný souborový adaptér má nativní scénáře H/S. Neexistuje volba ignorování H/S ani jejich zvláštní označení v náhledu. Skrytí řádku filtrem výsledků je samostatná věc: taková akce se neprovádí.

## Doporučený nejmenší rozsah

1. **Asymetrie vlevo → vpravo.** Navrhovat odstranění pouze adresářů existujících jen vpravo, se zapnutým zahrnutím adresářů. Kořeny nikdy. Společné prázdné adresáře zachovat. Žádný globální úklid.
2. **Viditelný plán.** Adresář bude vlastním řádkem s ikonou a návrhem odstranění. Souhrn oddělí počet souborů a adresářů. Změna návrhu na adresářovém řádku nebude potají měnit návrhy potomků. Skrytý či Skip potomek se nesmaže; rodič s ponechaným obsahem není způsobilý k odstranění. Při neúplném skenu podstromu rovněž žádný návrh odstranění rodiče.
3. **Běžné provedení.** Nejprve schválené soubory existujícím adaptérem, potom zařazené adresáře od nejhlubšího. Použít RemoveDirectoryW na jednotlivé ověřené adresáře, bez rekurzivního příkazu, následování reparse nebo vynuceného měnění oprávnění. Před odstraněním opět ověřit identitu, nepřítomnost levého protějšku a skutečnou prázdnost. Nový obsah znamená změnu plánu a zastavení, ne jeho dodatečné smazání. Stop ponechá již dokončené operace a zastaví další.
4. **Keep originals ON/OFF.** Při ON před prvním mazáním uchovat všechny zařazené soubory stávajícím postupem a navíc zachytit topologii odstraňovaných adresářů včetně původně prázdných, jejich cesty a podporovaná metadata. Protokol rozliší soubor/adresář a skutečný výsledek. Ruční obnova nejprve vytvoří adresáře a potom vrátí soubory; přesnou obnovu ACL/owner/SACL dosavadní záloha neslibuje. U adresářových ADS je nutné před implementací určit podporu: nepodporovaná data nesmějí být potichu ztracena, takovou položku blokovat. OFF zůstane bez uchování, podle zapamatované volby. Chyba přípravy obnovy zabrání mazání.
5. **Hidden/System zahrnout.** Zachovat současné výchozí chování nezávislé na pouhém zobrazovacím nastavení panelů. Název začínající tečkou sám o sobě není atribut Hidden. V detailu/tooltipu uvést H/S; pro trvalé rozlišení navrhuji volitelný sloupec Atributy. Neměnit význam šedé barvy používané pro Skip ani barvy směru operace. Nové volby ignorování H/S nyní nejsou nutné. Pokud budou přidané později, vynechání levé položky nesmí vytvořit falešný návrh mazání jejího pravého protějšku a vynechaný obsah musí chránit předky.

Body tvoří schválený rozsah. Nezavádí se transakční smazání celého stromu, automatický rollback ani nové SDK. Volitelný sloupec Atributy zůstává možné pozdější rozšíření; nyní jsou H/S v tooltipu.

## Běžné testy při implementaci

- Pravostranný prázdný adresář, vnořený strom a společný prázdný adresář; správné pořadí a oddělené počty.
- Maska vynechávající jediný soubor, Skip nebo filtr ukrývající potomka, vypnutá rekurze a chyba čtení: ponechaný obsah i rodič zůstanou.
- Hidden, System, H+S, skrytý adresář s obsahem a tečkový název; shodné zařazení a srozumitelné atributy.
- Obnova ON/OFF včetně prázdných adresářů; Unicode/dlouhé cesty; nové položky po skenu; reparse a běžné odepření přístupu.
- Stop/Close mezi soubory a adresáři a po prvním odstraněném adresáři; srozumitelný částečný výsledek.

Dosavadní rozsáhlý místní pilot již proběhl. Tento návrh nevyžaduje jeho opakování ani nové obecné kvalifikační kolo před zahájením práce na adresářích.

## Implementace a ověření

Náhled nově nabízí i neprázdné pravostranné adresáře při Asymmetric + Empty folders + úplném rekurzivním skenu. Maska či chyba uvnitř podstromu brání návrhu jeho odstranění. Společné prázdné adresáře zůstávají. Bez rekurze lze nabídnout pouze fyzicky prázdný adresář.

`DeletionRows` sdílí GUI potvrzení a adaptér. Deduplikuje viditelné indexy, kontroluje ponechané potomky pomocí uspořádané množiny cest a vrací soubory před adresáři, adresáře od nejhlubších. Skip či skrytí potomka vyřadí i jeho rodiče; vyřazení rodiče nepřidává ani nemění potomky. Změny návrhu nad adresářem podporují pouze Skip, výchozí návrh a DeleteRight. Souhrn provedení a potvrzení oddělují soubory a adresáře. Příkaz je přejmenován na Delete visible items / Smazat viditelné položky.

Při Keep originals ON stávající přípravná fáze uloží každý odstraňovaný adresář do vlastní nezávislé cesty `saved` včetně původně prázdných adresářů. Ověří typ, podporované H/S/R atributy a mtime; relativní cesta a metadata jsou v manifestu. Adresářové kopie i soubory musejí být kompletní a zalogované před jakýmkoli mazáním. `manifest.jsonl` a planned řádky `delete.jsonl` mají nové aditivní pole `kind`: `file`/`directory`, schéma zůstává 1. Všechny planned řádky mazání jsou flushnuté před prvním odstraněním. Při OFF se adresář obnovy nepoužije a trvalý protokol nevzniká.

Před každým RemoveDirectoryW se ověří bezpeční existující rodiče, chybějící levý protějšek, pravá identita, typ a atributy; časy a počet hardlinků adresáře se po vlastním mazání potomků nemusejí shodovat se skenem. Prázdnost vynucuje samo RemoveDirectoryW. Nový obsah se nepřidává do plánu a způsobí chybu bez rekurzivního fallbacku. Vlastní ADS adresářů jsou zatím nepodporované: kontrolují se před zálohováním/mazáním celé dávky a znovu před odstraněním konkrétního adresáře, při jejich zjištění se operace zastaví. Read-only ani ACL se neobcházejí. Zůstávají stejné praktické meze souběhu jako u stávajícího souborového adaptéru; nejde o atomickou transakci stromu.

H/S se nadále zahrnují do skenu. Tooltip řádku uvádí Hidden/System (česky Skrytý/Systémový) s označením L/R pro každou stranu; významové barvy a Skip zůstávají.

### Výsledky místních nativních testů

- **3 729 kontrol kopírování/obnovy/přepisu/mazání, Release i Debug x64.** Nové scénáře skutečně vytvářejí a odstraňují data v jedinečných vlastních NTFS fixtures: prázdný adresář a vnořený strom; soubory s ADS; H/S adresáře; zachování společného prázdného adresáře; ON/OFF a nezávislé uložené adresáře/metadata; samostatné druhy v obou protokolech.
- Maskou vynechaný obsah, Skip, filtrem skrytý potomek a neúplný nerekurzivní sken zachovávají rodiče. Zvláštní případ ověřuje, že lexikograficky vložený sourozenec nezakryje při hledání vyloučeného potomka.
- Stop před a po odstranění; nový soubor uvnitř adresáře; nově vzniklý levý protějšek; výměna adresáře za junction; původní i nově přidané adresářové ADS; read-only; skutečně odepřené DELETE i DELETE_CHILD; chyba protokolu obnovy před prvním odstraněním. První chyba/Stop zastaví další adresáře a dokončená odstranění zůstávají.
- Unicode v názvech i několika předcích, emoji a cesty překračující 260 UTF-16 i UTF-8 jednotek, ON/OFF.
- **542 kontrol engine**, **1 974 anglických a 1 990 českých kontrol resource/layout**, **2 357 lifecycle kontrol** stávajících cest. Layout kontroly používají metriky 96/120/144/192/240 DPI; nejsou novým průchodem skutečným GUI při těchto DPI. Starší test stejné velikosti a obnoveného mtime jednou selhal při velmi rychlém zápisu; fixture nyní explicitně nastaví odlišný ChangeTime, aby výsledek nezávisel na rozlišení hodin souborového systému.
- Produkční Release x64 a Debug Win32/x64/ARM64 plugin včetně obou SLG sestaveny s warnings-as-errors. Bez změn sdíleného SDK/hostitele.

V této implementační etapě proběhly nativní testy skutečného adaptéru. Následující oddíl doplňuje pozdější Computer Use průchod potvrzeným mazáním a ruční obnovou v GUI.

## 2026-10-16 — GUI mazání a ruční obnova

Computer Use v izolovaném anglickém/light Samandarinu 0.16 x64, hostitel `C:\Samandarin\builds\dirsync-directory-gui-20261016\host`, místní NTFS. Testovaný původní SPL z balíčku etapy mazání adresářů má SHA-256 `03ff894872240e355a5a6e9d57f8058d32ae44316378186dea8faeace7aa051d`. Uživatelské instalované instance nebyly měněné. Nastavení testovacího hostitele je v samostatném souboru config.reg.

Dvě nezávislé sady `on/left,right` a `off/left,right`: společné `common-empty` a `keep.txt`, pravostranné `empty-right`, `tree-漢字/nested/empty-leaf` a `tree-漢字/nested/žluťoučký.txt` (11 B + `:note`, 8 B), dále kontrolní `protected/keep.bin` a `skipped/keep.txt`. Maska `*.txt`, Subfolders + Asymmetric + Empty folders. Soubor skipped/keep.txt byl v GUI přes Ctrl+M nastaven na Skip; jeho rodič zůstal samostatným návrhem, který příkaz vynechal kvůli ponechanému potomku. Jediné zvýraznění tohoto Skip souboru neomezilo operační příkaz: potvrzení vycházelo ze všech viditelných způsobilých návrhů.

### ON a ruční obnova — dokončeno

Po samostatném souhlasu uživatele potvrzeno skutečné mazání. Souhrn: **1 soubor, 4 adresáře, 0 chyb, 0 nezahájených, 1 jiný návrh vynechaný; 5/5 originálů uchováno**. Obnova vznikla mimo oba stromy v `recovery-漢字/pair-ec7c35863ac57666/run-{BCECF64F-4DAB-4BAF-9796-C4C3CB5EAAF5}`. Oba JSONL protokoly končí complete; manifest má jednu file a čtyři directory položky. Kontrola disku potvrdila odstranění jen potvrzené sady a zachování všech kontrolních souborů, společného prázdného adresáře i předků chráněného obsahu. Nové Compare ukázalo 4 řádky / 0 blokovaných; ruční Skip se podle návrhu novým Compare obnovil na výchozí návrh.

Ruční obnova do nového kontrolního adresáře `restore` přes běžné GUI hostitele:

1. Shift+F7 otevřelo restore. F7 vytvořilo `tree-漢字\nested\empty-leaf` včetně předků a druhé F7 `empty-right`, podle relativních adresářových záznamů manifestu.
2. Shift+F7 v druhém panelu otevřelo rodiče souborové cesty saved; výběr souboru a F5 jej zkopíroval do `restore\tree-漢字\nested\žluťoučký.txt`.
3. Nezávislá kontrola potvrdila přesný SHA-256 `c33f1cc7e7e6cf37c8a90b49f79a8528144003c42e56d8523cf28744ac818e48`, obsah `:note` = `ADS DATA`, původní souborový mtime a všechny čtyři adresáře včetně obou prázdných listů. Originální záloha zůstala zachovaná.

**Meze této obnovy:** obnovena struktura, souborová data, ADS a souborový mtime. Adresáře vytvořené F7 mají nové časy; jejich původní časy/atributy se v tomto průchodu ručně nenastavovaly. Není tvrzena přesná obnova adresářových metadat ani ACL/owner/SACL. Nevkládal se celý pomocný strom obnovovacího běhu do původního pravého kořene.

Lokální důkazy mimo Git: baseline.json, gui-evidence.json a on-result.json pod kořenem sady. Baseline obsahuje kontrolní hashe a souborové časy; žádný automatický obnovovací příkaz pluginu nebyl použit.

### OFF — dokončeno

V Preferences vypnuto Keep originals, existující cesta obnovy zůstala uložená a její pole bylo zakázané. Druhá nezávislá sada má stejných 9 řádků a stejnou ochranu maskou/Skip. Otevřené potvrzení správně uvádí **1 soubor, 4 adresáře, 11 hlavních bajtů, 1 vynechaný návrh, OFF — no recovery copy from this run**. Po samostatném souhlasu uživatele skutečně provedeno: **1 soubor, 4 adresáře, 0 chyb, 0 nezahájených, 1 vynechaný návrh**. Nezávislá kontrola NTFS potvrdila nepřítomnost pouze schválené sady, stejné hashe a časy kontrolních souborů i přítomnost common-empty, protected a skipped. Existuje stále jediný původní běh ON; jeho uložený soubor má původní hash i ADS. Nový běh obnovy ani protokol OFF nevznikl. Důkaz off-result.json je vedle baseline.

### Drobné opravy GUI

Anglický titulek potvrzení nyní říká Confirm deletion místo Confirm file deletion, protože běh může obsahovat i adresáře. Při změně návrhu pod ukazatelem mohl owner-data seznam ponechat v cache původní tooltip Delete on right i po přepnutí na Skip; aktualizace modelu nyní přepíše také nativní infotip položky pod ukazatelem. Executor ani SDK se nemění.

Release x64 a Debug Win32/x64/ARM64 se po těchto úpravách sestavily s warnings-as-errors. Anglické/české resource-layout kontroly prošly 1 974/1 990, dosavadní lifecycle sada 2 357. Po řádném ukončení soukromého hostitele načten nový SPL SHA-256 `6e288f5e0cd0ef8b7a5bf354d0594c321f3e3732bd569445175105b32a6f4d9a`. V GUI ověřeno původní Delete on right → Ctrl+M pod nehybným ukazatelem → tooltip Skip i správná ikona. Otevření potvrzení ověřilo nový titulek Confirm deletion; tento dodatečný kontrolní dialog byl zrušen přes Escape bez mazání. Testovací hostitel byl řádně zavřen. ON/OFF datové průchody používají původní modul uvedený výše; následná revize mění pouze GUI. Bez SMB testů a změn SDK.
