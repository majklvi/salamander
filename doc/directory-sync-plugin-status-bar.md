# Stavový řádek — 0.1.4

17. října 2026. Úprava pouze pluginu, bez SMB testů a změn SDK.

Dosavadní dva oddíly s pravým souhrnným textem odděleným svislými čárami jsou nahrazené samostatnými nativními oddíly. Vlevo zůstává stav. V klidu následují řádky celkem, viditelné, skryté a blokované/konfliktní. Při skenu jsou samostatně prohledané a porovnané položky; při čtení navíc přečtené bajty. Souborové operace mají položku X/Y, zkopírované bajty, ověřené bajty a dokončené položky. Zastavování zachovává průběhové údaje.

Šířky vycházejí z měření skutečného textu fontem status baru. Při nedostatku místa se proporcionálně zmenší a ponechají alespoň přibližně 28 % šířky stavu. Nativní tooltip každého oddílu obsahuje celý text. Počet oddílů se mění jen podle fáze, seznam zůstává přilepený ke status baru. Nezavádí se vlastní kreslení ani samostatné nastavení barev.

## Ověření

- Release x64 sestavení pluginu i obou jazykových modulů prošlo s warnings-as-errors.
- 2 419 anglických / 2 451 českých resource/layout kontrol prošlo při 96/120/144/192/240 DPI metrikách. Nové jednotlivé formátovací texty jsou lokalizované a neobsahují oddělovací svislé čáry; staré souhrnné texty byly odstraněny.
- 2 366 lifecycle kontrol prošlo. Rozšířený nativní test používá skutečný msctls_statusbar32: ověřuje počty oddílů pro klid, sken, čtení, kopírování i Stop; při pěti velikostech fontu odpovídajících 96–240 DPI a šířkách 450/900/1600/700 px ověřuje rostoucí hrany oddílů, místo pro stav a úplný dlouhý Unicode tooltip. Zachována kontrola návaznosti seznamu na status bar.
- Jde o automatizované nativní ověření ovládacího prvku a fontových metrik; ruční GUI test přepínání DPI mezi monitory nebyl v této etapě proveden.

Verze registračních údajů, SPL, českého i anglického SLG a balíčku je 0.1.4.

## GUI ověření světlého a tmavého režimu

17. října 2026, aktuální produkční balíček **0.1.4 x64**, commit `26f4a813`, soukromý Samandarin 0.16 x64. Ovládání a snímky přes Computer Use; bez přímé UI Automation přes PowerShell, testovacích hooků, SMB a změn SDK. Český jazyk v obou režimech. Tmavý režim znamenal hlavní schéma hostitele Windows Dark Mode, světlý režim standardní světlé schéma. Změněna byla pouze soukromá souborová konfigurace při vypnutém hostiteli; nakonec obnovena.

Sada `C:\Samandarin\builds\dirsync-status-gui-20261017` má oddělené `dark` a `light` stromy. Každá levá strana obsahovala 16 nových souborů po 64 MiB (celkem 1 GiB), oba protějšky navíc stejný sparse soubor `paired-漢字.bin` o logické velikosti 16 GiB a `keep.txt` 14 B. Velké shodné soubory umožnily zachytit skutečně běžící obsahové porovnání. Synchronizace kopírovala pouze 16 nových souborů, bez přepisů a mazání.

| Stav | Tmavý režim | Světlý režim |
| --- | --- | --- |
| Před porovnáním | Samostatný stav Připraveno a čtyři nulové počty | Totéž, správné světlé pozadí a text |
| Obsahové porovnání | Čtyři nativní oddíly; zachyceno např. Přečteno 29 154 607 132 B, Prohledáno 20, Porovnáno 17 | Čtyři oddíly; zachyceno např. 865 337 372 B a stejné průběžné počty |
| Dokončené porovnání | Pět oddílů: stav, 18 celkem, 18 viditelných, 0 skrytých, 0 blokovaných | Totéž |
| Kopírování a ověření | Pět oddílů; zachycena Položka 4 / 16, zkopírováno 268 435 456 B, ověřeno 434 110 464 B, dokončeno 3 | Zachycena Položka 2 / 16, zkopírováno 67 108 864 B, ověřeno 134 217 728 B, dokončeno 1; další snímek už 9 / 16 |
| Dokončená synchronizace | Výsledek 16 zkopírovaných a ověřených, 0 chyb, 0 nezahájených; stav vyzývá k novému porovnání | Totéž |

Počty přečtených/ověřených bajtů jsou dosavadní pracovní čítače, nikoli procento přenosu; při ověření se čte více než jeden datový obraz. Přístupnost hlásila každý oddíl jako samostatný text. Hodnoty se měnily i na snímcích, nikoli pouze v testovacím modelu. Snímek a přístupnost jsou pořízené postupně, proto mohou ukazovat různé okamžiky téhož průběhu.

### Velikost a kreslení

V obou režimech proběhla maximalizace a návrat z úzkého dialogu širokého 917 px na maximalizovaný náhled široký 2 286 px a zpět. Oddíly zůstaly u dolního okraje, seznam na nich navazoval bez mezery; nevznikly překryvy ani zbytky předchozího textu. V širokém okně zůstaly počty napravo, prostor navíc získal stav. V úzkém okně se vešly i zachycené průběhové počty s velkými bajtovými hodnotami. Dlouhý závěrečný stav se v této konfiguraci vešel.

Ověření změny velikosti je doložené maximalizací/obnovením, nikoli úspěšným plynulým tahem rámu: pokusy o tah nezměnily skutečnou velikost nebo byly odmítnuté kvůli souřadnicím mimo rozsah nástroje. Pokusy o maximalizaci zadané za běhu porovnání zachytily přechod na hotový výsledek; nejde o tvrzení, že byl zaznamenán každý snímek změny velikosti během aktivního přenosu. Skutečné aktivní porovnání a synchronizace jsou ověřené samostatnými snímky výše. Nový ruční test DPI mezi monitory se neprováděl.

### Výsledek a úklid

Status bar nevyžadoval opravu kódu. Následná nezávislá kontrola ověřila SHA-256, velikost a čas změny všech 32 nových cílových souborů proti zdrojům. Velikost a čas původních velkých protějšků zůstaly zachované, obsah obou keep.txt nezměněn. Ověřené nově vytvořené cíle byly následně uklizeny, aby nezabíraly 2 GiB systémového disku; zůstávají zdroje a místní `gui-evidence.json` / `file-evidence.json`. Neprobíhal žádný GUI příkaz mazání.

Při přípravě sady první pokus o prodloužení sparse souboru přes Python truncate ve Windows fyzicky alokoval místo a skončil nedostatkem místa. Tento jediný vzniklý syntetický soubor byl odstraněn a volné místo ověřeno před spuštěním testů; správná sada používá FSCTL_SET_SPARSE a SetEndOfFile. Nešlo o běh pluginu ani výsledek jeho testu nedostatku místa.

Oba soukromé hostitele byly normálně ukončené, dodatečná instance vzniklá launcherem také zavřená. Původní uživatelské instance PID 3820, 16756 a 17148 zůstaly běžet. Verze a binární balíček zůstávají 0.1.4 beze změny.
