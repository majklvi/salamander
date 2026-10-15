# UI Automation a dokončení GUI Stop/Close

15. října 2026. Navazuje na [předchozí ověření přerušení](directory-sync-plugin-copy-cancellation.md). Tento záznam uzavírá tehdy otevřený průchod GUI Stop/Close během skutečného kopírování. Pouze izolovaná místní NTFS data; bez přepisů, mazání uživatelských položek, SMB a změn SDK/hostitele.

## Došetření prodlev

Prodleva je reprodukovatelná na samostatném standardním Win32 report seznamu bez načteného pluginu, SDK, kopírovacího workeru a darkmode knihovny. Diagnostický program má 4 000 řádků a 9 sloupců. Varianta virtual používá LVS_OWNERDATA a jednoduchý LVN_GETDISPINFOW callback; varianta stored vloží běžné položky a jejich texty do controlu. Finální měření nemění titulek ani jinou vlastnost UI periodicky. WM_TIMER každých 250 ms zapisuje heartbeat a počet LVN_GETDISPINFOW notifikací do vlastního TSV souboru.

| Varianta, 4 000 řádků | Největší mezera mezi heartbeat záznamy | Počet LVN_GETDISPINFOW notifikací |
| --- | ---: | ---: |
| Virtuální Win32 seznam | 19 547 ms | 132 264 |
| Běžný seznam s uloženými položkami | 19 188 ms | 128 001 |

Během prodlevy Process.Responding vracel false a diagnostický stack hlavního vlákna vedl přes UIAutomationCore, SendMessageW a COMCTL32. Po dokončení průchodu oba procesy zase odpovídaly; pozdější stack běžného seznamu končil v obvyklém GetMessageW. Požadavek Computer Use na úplný strom běžného seznamu skončil po 11 503 ms bez stromu, přestože nativní měření potvrdilo pozdější návrat do message loopu. U pluginového seznamu se dvěma řádky proběhl dostupný strom za 50 ms.

Tím je doloženo, že dlouhé synchronní průchody přístupností standardního report controlu v této relaci mohou blokovat jeho hlavní vlákno i bez pluginu. Problém není specifický pro LVS_OWNERDATA ani pro kopírovací worker. Přesný interní algoritmus UIAutomationCore ani konkrétní chyba dodavatele tím nejsou určené. Není správné každý timeout nástroje vydávat za deadlock aplikace. Opakované požadavky na velký strom jsou věrohodné vysvětlení dřívějších mnohem delších prodlev, nikoli samostatně změřený počet odbavených požadavků v původním běhu.

Nevznikla změna produkčního seznamu, provideru, SDK ani vypnutí přístupnosti. Stejné omezení vykázaly obě standardní varianty. Pro další testování během citlivé fáze použít malý počet větších souborů a cílený screenshot/klávesu, nikoli opakované úplné průchody tisíců řádků po timeoutu. Dlouhá odezva při masivním UIA průchodu velkým seznamem zůstává známým omezením této Windows/UIA relace; malé GUI testy nejsou příslibem rychlé odezvy pod takovým zatížením.

Evidence: `C:\Samandarin\builds\dirsync\uia-copy-20261015\baseline.cpp`, `baseline.vcxproj`, `baseline-results.json`, finální `baseline-virtual.tsv` a `baseline-stored.tsv`. Zdroj je vlastní minimální reproduktor; není součástí distribuovaného pluginu ani automatických regresních testů. Starší pokus s periodickým titulkem nebyl použit pro uvedené mezery. Samostatné diagnostické procesy byly po uložení měření uklizeny.

## Skutečné GUI Stop a Close na aktuální binárce

Použit soukromý Release Samandarin 0.16 v `C:\Samandarin\builds\dirsync\run` s vlastní RegFile konfigurací a darkmode. Před finálním běžným testem byl hostitel normálně ukončen, plugin a jazykový modul obnoveny z posledního produkčního Release buildu a znovu načteny. Testované SPL i SLG měly shodný SHA-256 s build výstupem:

- SPL: `F27B18C7D89820EDB38655939D654B4B799A9DEE3DC84499D997E5789E3411AE`.
- SLG: `701CD3E2BC87FEFD64C6A4E62CA7264BFBDCD5B3B14BFFD16F8100CFD3EE1EC4`.

Žádný testovací hook, umělá pauza workeru ani zvláštní GUI build. Přípravným dvěma pokusům se starší soukromou binárkou se nepřisuzuje identita aktuálního buildu; níže jsou zopakované finální pokusy.

### Stop

Zdroj obsahoval nový `0-done.txt` (DONE + ADS `:note` = DONE-ADS), nový `a-large.bin` o 4 GiB a následný `z-next.txt`. Po Compare a samostatném potvrzení se první soubor dokončil. Snímek průběhu potvrdil druhou položku, nenulový přenos velkého souboru, pouze 24 bajtů Verify z dokončené malé položky a jedno dokončení. Během této fáze odesláno Alt+T.

Souhrn: **1 dokončený, 1 přerušený (error 1235), 1 nezahájený**. Poslední průběh ukazoval 1 880 096 780 přenesených bajtů včetně první kopie, tedy méně než 4 GiB. Následný status vyžadoval nové Compare. Požadavek klávesy a návrat snímku se souhrnem trval **922 ms**.

Na disku zůstal správný malý soubor a jeho ADS. Velký cíl, následující cíl ani staging neexistovaly. Nejde tedy jen o Stop odeslaný až po dokončení dávky.

### Close a návrat do hostitele

Po ukončeném workeru přidán do zdroje nový `00-close.txt` (CLOSE-DONE + Unicode ADS `:poznámka` = CLOSE-ADS) a provedeno nové Compare. Původní `0-done.txt` byl Equal/Skip; nevykonával se znovu. Potvrzené nové kopie byly pouze `00-close.txt`, velký soubor a následující soubor.

První nová malá položka se dokončila. Průběh před Close ukazoval položku 2 ze 3, **450 887 699 přenesených bajtů**, 38 bajtů Verify a jedno dokončení. Kliknutí na tlačítko Close během této fáze zavřelo dialog a vrátilo řízení do hostitele za **686 ms** včetně následného snímku. Neobjevil se koncový souhrn po požadavku zavření.

Pravý panel stejného cílového kořene po návratu zobrazil `0-done.txt` i `00-close.txt`, bez ručního refresh. To spolu s návratem do hostitele ověřuje celý běžný průchod dokončovací zprávy, EndDialog a pozorovatelný refresh panelu. Samotné jednotlivé SDK volání se zvlášť netrasovalo; hostitel může reagovat i na standardní změnové notifikace filesystému.

Nezávislá kontrola: oba malé cíle i ADS mají očekávaný obsah, velký ani následující cíl nevznikly, stagingů 0, hostitel odpovídá. Velký zdroj šel otevřít bez sdílení a všech jeho **1 024 bloků po 4 MiB** odpovídalo očekávanému SHA-256 deterministicky připraveného bloku ([Random] seed 151516, 4 MiB opakovaných 1 024krát). Data zdroje tedy nebyla změněna a file handle workeru nezůstal otevřený. Evidence `final-stop-result.json` a `final-gui-result.json` ve fixture.

Časy 922/686 ms jsou jednotlivá měření celého Computer Use roundtripu se snímkem, nikoli přesná instrumentace interního cancel/join ani záruka pro libovolný disk/ovladač. GUI běhy prokazují přerušení během přenosu. Přerušení uvnitř Verify je doloženo již existujícími nativními integračními testy, nikoli novým samostatným GUI během.

## Stav a úklid

Otevřený běžný GUI Stop/Close případ je v uvedeném rozsahu uzavřen. Funkční chyba kopírovacího adaptéru se nepotvrdila a produkční kód zůstal beze změn. Změny tohoto kroku jsou dokumentační. Známá nákladnost velkých UIA průchodů je výše výslovně oddělená od úspěšného GUI testu.

Obě soukromé instance hostitele v tomto kroku byly normálně zavřeny a původní soukromý config.reg byl obnoven ze zálohy. Instalované instance zůstaly spuštěné. Po ověření byla pouze vlastní objemná 4GiB fixture uklizena; menší fixtures, zdroje reproduktoru a evidence jsou zachované. Bez přepisů, mazání uživatelských položek, SMB testů a změn SDK. Full disk, poruchy zařízení a crash recovery se tímto krokem neověřují.