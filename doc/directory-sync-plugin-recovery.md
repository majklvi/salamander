# Directory Synchronization: uchování originálů a jednoduchý protokol

**Navazující stav:** na nový pokyn uživatele implementovány [přepisy podle D4/D5](directory-sync-plugin-overwrite.md), které při ON používají právě tuto obnovu a při OFF ji vynechají. Aktuální společné testy: 2 663 kontrol adaptéru, 1 029 lifecycle kontrol Debug/Release x64, 1 213 layout kontrol. Níže je samostatně zachován rozsah a evidence předchozí etapy; její tvrzení o neimplementovaných přepisech jsou historická. Mazání, SMB testy a SDK/host změny zůstávají mimo rozsah.

15. října 2026, větev `feature/directory-sync-plugin`, výchozí commit `072b47db`. Implementována samostatná část D4/D5 podle [nejmenšího adaptéru](directory-sync-plugin-core-adapter.md): **uložení nezávislé ověřené kopie původního cílového souboru do složky obnovy**. Původní soubor se nepřejmenovává, nepřepisuje ani nemaže. Tato etapa neprovádí žádný návrh synchronizace; přepisy, mazání, SMB testy a změny SDK/hostitele zůstávají mimo rozsah.

## Ovládání a rozsah

Po Compare zadat existující **Recovery folder** mimo oba porovnávané stromy a použít **Save originals...** (Alt+V). Samostatné potvrzení uvádí počet viditelných způsobilých návrhů, velikost hlavních dat, počet vynechaných návrhů a obnovovací složku. Cancel nevytvoří nic. Akce bere viditelné návrhy, nikoli pouze zvýrazněné řádky.

Způsobilý je pouze návrh kopie, kde na obou stranách existuje podporovaný běžný soubor: Copy to right ukládá původní pravý cíl, Copy to left původní levý cíl. Skip, nový cíl, mazání, adresáře, konflikt, nerozhodnutá či blokovaná položka se neukládají. Pro návrh budoucího mazání tedy zatím nejde uchování z UI spustit. Také prázdný soubor je soubor a lze jej uložit.

Na následný pokyn uživatele doplněna volba **Keep originals (before overwrites/deletions)**, výchozí **zapnutá**, přepínatelná i přes Alt+K. Při vypnutí se obnovovací složka a Save originals deaktivují; vypnutý stav navíc kontroluje samotná obsluha příkazu. Vypnutí nevytváří kopie, neodstraňuje starší zálohy a neovlivňuje kopírování do nových cílů. Žádné automatické zálohování při synchronizaci dosud zapojené není: zapnutá volba sama nic nekopíruje. Přepisy a mazání dál nejsou implementované. Budoucí executor bude muset tuto volbu převzít a při vypnutí jasně uvést, že původní data nezachová; staging a ostatní běžné kontroly zůstanou.

Volba i ručně zadaná obnovovací složka jsou **společným zapamatovaným nastavením pluginu**, nikoli profilem dvojice stromů. Používá se existující `FUNCTION_LOADSAVECONFIGURATION`, LoadConfiguration/SaveConfiguration a CallLoadOrSaveConfiguration na UI threadu. Zapnutí/vypnutí se ukládá při změně, aktuální složka také při zavření dialogu. `KeepOriginals` je DWORD 0/1; `RecoveryRootUtf16` je výslovně UTF-16 REG_BINARY s ukončovací nulou, aby cesta neprošla ACP převodem starého SDK. Neznámé či neplatné nastavení použije bezpečný výchozí stav; velikost a terminace cesty se kontrolují. Chyba uložení se uživateli oznámí jako platnost jen pro nynější relaci. O fyzické uložení konfigurace hostitele se dál stará jeho standardní mechanismus; žádný vlastní profilový soubor nebo SDK změna.

Není výběrový dialog; obnovovací složka při spuštění uchování musí už existovat. Odmítá se její překryv s kterýmkoli vstupním stromem v obou směrech (tedy i společný předek), ověřují se skutečné cesty a podporovaný typ rodičů. Reparse, cloud a EFS zůstávají nepodporované. Před zápisem se znovu ověří celý snapshot a před přenosem běžně zkontroluje identita a metadata ukládaného originálu. Pozorovaný rozpor zastaví operaci. Nejde o záruku proti libovolné souběžné výměně stromu.

Přenos používá existující CopyOne: vlastní CREATE_NEW rezervace stagingu, CopyFileExW, přesné Verify hlavních dat a sady ADS včetně jmen/délek a obsahu, kontrola metadat, publikace pouze na nový název. Obnova zachová mtime a Read-only/Hidden/System; Archive má běžnou hodnotu nové kopie. Má nezávislá data, není hardlinkem originálu. Oprávnění dědí z obnovovací složky, nepřenáší původní DACL, owner ani SACL. Ruční obnova tedy neslibuje vrácení původních oprávnění. Je na uživateli zvolit vhodně chráněnou obnovovací složku. ADS se nesmějí tiše ztratit na nepodporovaném úložišti.

Průběh ukazuje skutečně přenesené a přečtené bajty a dokončené kopie. Stop/Close používají stávající cancel/join. Nedokončený vlastní staging se uklidí, nebo se jeho cesta ohlásí. Již publikovaná kopie zůstává i při chybě následné kontroly či protokolu; žádný rollback. První chyba zastaví další kopie. Vytvořené prázdné adresáře a neúplný protokol mohou zůstat k ručnímu posouzení. UI po běhu zneplatní náhled a oznámí změnu obnovovací cesty přes existující SDK z UI threadu. Worker SDK nepoužívá.

## Uspořádání a formát 1

```text
<recovery>/pair-<hash dvojice skutečných kořenů>/run-{GUID}/manifest.jsonl
<recovery>/pair-<hash>/run-{GUID}/right/<číslo položky>/data/<relativní cesta>
<recovery>/pair-<hash>/run-{GUID}/left/<číslo položky>/data/<relativní cesta>
```

Hash slouží k seskupení dvojice, nikoli jako důkaz identity či bezpečnostní záruka. Skutečné vstupní cesty jsou v hlavičce. Každý běh rezervuje nový GUID adresář; ani opakování stejného náhledu nepoužije starou kopii. Jednotlivá položka má vlastní podadresář a zachovanou relativní cestu, takže stejné názvy v různých složkách nekolidují. Data/logy starších běhů se nepřepisují ani automaticky nepromazávají.

`manifest.jsonl` je UTF-8 bez BOM, jeden JSON objekt na řádek s LF, bez závislosti na ACP. Po každém záznamu se kontroluje WriteFile i FlushFileBuffers. Je to jednoduchý mapovací protokol pro ruční obnovu, **nikoli transakční journal**.

| Událost | Pole a význam |
| --- | --- |
| `run` | `schema: 1`, `operation: "save_originals_only"`, `left`, `right`, `runDirectory`. |
| `planned` | `item` (od 1), `side` ukládaného cíle, `relative`, absolutní `original` a `saved`, `size`, `mtime` jako 64bitový Windows FILETIME, `attributes` ze snapshotu. Záznam je uložen před kopírováním. |
| `result` | Stejné `item`; `status`: `saved`, `failed`, `cancelled`, `not_started`; číselný Windows `error`, `published` a `remainingStage`. `saved` znamená, že proběhlo Verify i konečné kontroly. `published: true` samo neznamená úspěch. |
| `end` | `status`: `complete`, `failed`, `cancelled`. `complete` až po ověření všech zařazených kopií a úspěšném zápisu celého protokolu. |

Nejprve se zapíší plány, potom výsledky. Při běžné chybě či Stop během přenosu/Verify se zaznamenají i nezahájené položky a konečný stav. Při selhání přípravy adresářů, appendu/flush nebo pádu procesu může protokol skončit dříve, včetně uprostřed řádku; chybějící konec není dokončený běh. Vyhnout se ztrátě přesnosti FILETIME při čtení nástrojem, který všechna čísla převádí na double.

Runtime výsledek rozlišuje úspěšnou kopii (`copy.status == Copied`), zapsaný výsledek položky (`logged`) a dokončený běh (`complete`). Chyba appendu/flush je navíc v `journalError`; chyba přípravy/otevření logu či přenosu v `error`. Pokud selže protokol po publikaci, zachová se známý výsledek kopie, ale `logged` a `complete` zůstanou false. Pro budoucí destruktivní krok se tento neúplný výsledek nesmí použít jako hotová obnova. Záznamy `save_originals_only` se při pozdější implementaci přepisu nesmějí slepě znovu použít; cíl se mohl od jejich vzniku změnit.

Ruční obnova: podle odpovídajícího `planned` a úspěšného `result` dohledat kopii, ověřit ji a zkopírovat nejprve do **nového jména**. Automatické nahrazení, replay, rollback, obnovení ACL a globální úklid nejsou součástí této etapy.

## Provedené běžné testy

Místní NTFS, bez SMB a bez zaplnění systémového disku. Rozšířen existující nativní test adaptéru; chyby přenosu a logu jsou výslovně řízené injekce, nikoli nový skutečně plný VHD test.

| Scénář | Výsledek |
| --- | --- |
| Oba směry, prázdný i nenulový originál, více ADS včetně Unicode jména a prázdného proudu | Uložena původní cílová data, nikoli navržený nový zdroj; originály/jejich ADS zůstávají beze změny. |
| D5 | Read-only/Hidden/System na kopii; kontrola mtime v přenosu a samostatně v GUI fixture. Původní DACL beze změny, kopie dědí v obnově. |
| Hardlink originálu | Pozdější změna aliasu originálu nezmění uložená data. |
| Opakovaný běh a stejný basename v různých složkách | Různé nové cesty, starší kopie zachována; duplicitní vstupní index se vykoná jednou. |
| Rozsah | Uloženy jen předané viditelné způsobilé návrhy; nový soubor ani návrh mazání se neprovedou. |
| Neplatná obnova, překryv/ancestor, neexistující adresář, reparse kořen | Odmítnutí před vytvořením logu. |
| Změna po Compare, výhradně zamčený originál, nepovolený zápis v obnově | Chyba/zastaralý snapshot; originály zachovány a další kopie nezahájena. |
| Stop při skutečném Copy callbacku a při skutečném čtení Verify | Přenos přerušen, bez publikace neúplné kopie, výsledky a nezahájené položky v logu. |
| Stop po publikaci prvního souboru | Hotová ověřená a zalogovaná kopie zůstane, další přenos se nezačne. |
| Úmyslně poškozený staging při Verify | Chybový výsledek, nepovolená publikace, originál nedotčen. |
| Odepřený FILE_WRITE_ATTRIBUTES po publikaci obnovovací kopie | Chyba 5 a failed v logu; publikovaná kopie zůstane pro kontrolu, originál nedotčen, další položka nezahájena. |
| Řízený ERROR_DISK_FULL při přenosu s reálnými částečnými daty | Chyba 112, staging uklizen a záznam failed, originál nedotčen. |
| Řízená chyba zápisu prvního planned záznamu | Žádný přenos nezačne; běh zůstane neúplný. |
| Řízený ERROR_DISK_FULL při prvním výsledkovém appendu po publikaci | Kopie zachována, `journalError=112`, žádný end complete, další položka se nezačne. |
| ASCII/CJK/české/emoji názvy, více Unicode předků, dlouhé basename, UTF-8 >260 B a UTF-16 >260 jednotek | Kopie/ADS/protokol bez zkrácení či ACP konverze; manifest platný UTF-8. |
| Ruční kopie z obnovy do nového jména | Vrácena původní data a ADS, žádný přepis. |
| Stop/Close z produkční obsluhy dialogu při recovery Copy i Verify | Nativní DLL s test hookem: worker cancel/join, zalogované přerušení i nezahájené položky, uvolněné handly. |

Celkem **1 281 copy/recovery assertions v Debug i Release x64**, **719 lifecycle checks v Debug i Release x64** (včetně dosavadních scan/content/copy a force Release regresí), **539 engine assertions Debug x64** a **1 049 resource/layout checks Debug x64** pro 96/120/144/192/240 DPI. Layout test není runtime test změny DPI monitoru. Produkční SPL/SLG sestaveny pro Debug Win32/x64/ARM64 a Release x64; runtime proveden pouze x64. SDK/hostitel beze změn.

## Krátké GUI ověření

Přes Computer Use, soukromý `C:\Samandarin\builds\dirsync\run\salamand.exe`, Samandarin-0.16 x64, Release SPL. Windows Dark Mode (experimental) v nastavení barev hostitele. Nainstalované uživatelské procesy nebyly použity. Fixture `C:\Samandarin\builds\dirsync-recovery\gui-20261015`: vlevo `NEW SOURCE`, vpravo `ORIGINAL` a ADS `:note = OLD ADS`; vlevo novější čas, automatický návrh Copy to right.

Nová řádka a potvrzení/souhrn jsou v tmavém schématu čitelné. Alt+C vytvořilo očekávaný návrh, Alt+V otevřelo potvrzení: 1 původní cíl, 8 B + ADS. Cancel ponechalo obnovovací složku prázdnou. Následné potvrzení uložilo 1 kopii a 1 výsledkový záznam bez chyb. Souhrn ukázal manifest a vyžádal nové Compare. PowerShell ConvertFrom-Json nezávisle přečetl všechny 4 záznamy (`run/planned/result/end`), ověřil cesty, původní i uložené hlavní obsahy/ADS a shodu mtime. Výstup `gui-20261015\verification.json` a skutečný manifest zůstaly v izolované fixture.

GUI binárky SHA-256:

- SPL: `833765DF9BF4047572BADCFD8007B8AA4D74A7C3346BE9B2A57736267C227597`.
- SLG: `F5140F591C57DFAD8954E81CEBCD918B89513104AB39F983D5CD925749C1B715`.

UIA klik na Cancel v modálním dialogu vrátil bod mimo okno; keyboard Escape cílený na vlastníka modal nezavřel. Dokončení proto použilo čerstvý screenshot hlavního okna a skutečné viditelné Cancel/OK. Toto pozorování se nevydává za prokázanou chybu klávesnice pluginu. Stop/Close během recovery zde byly ověřeny nativně, nikoli rychlostně měřeny přes GUI. Soukromý hostitel po testu zavřen a jeho RegFile konfigurace obnovena ze zálohy se shodným hashem.

## Doplněné ověření zapamatování nastavení

Test reálných LoadConfiguration/SaveConfiguration callbacků s izolovanou náhradou SDK registry ověřil 13 případů: výchozí zapnutí, zapamatování vypnutí i opětovného zapnutí, Unicode/emoji cestu s UTF-8 >260 B, UTF-16 cestu >260 jednotek, správné typy hodnot, úspěch/neúspěch uložení a odmítnutí neplatné hodnoty volby či neukončeného, lichého, příliš velkého, vnořenou nulou či vadným surrogate poškozeného řetězce. Test nenastavuje uživatelské registry. Aktuálně **725 lifecycle kontrol** (plus 13 vnitřních kontrol nastavení při každém z 6 scan/content lifecycle cyklů), **1 174 kontrol rozložení dialogu** při 96–240 DPI; copy/recovery sada zůstává 1 281.

GUI přes Computer Use ověřilo skutečné uložení i načtení po úplném ukončení a restartu soukromého Samandarinu-0.16, nejen znovuotevření dialogu ve stejném procesu. Výchozí checkbox byl zapnutý. Zadaná složka `C:\Samandarin\builds\dirsync-recovery\gui-20261015\recovery-漢字` a vypnutí Alt+K přežily restart; checkbox zůstal prázdný, cesta beze změny a edit/Save originals neaktivní, bez chybového dialogu. V RegFile zapsáno `KeepOriginals=dword:00000000` a úplná UTF-16 binární cesta. Obnovovací kopie se v tomto testu nevytvářely. Důkaz uložené konfigurace je `C:\Samandarin\builds\dirsync-recovery\settings-20261015\config-disabled.reg`. Soukromý hostitel zavřen a původní konfigurace obnovena se shodným hashem. Uživatelské nainstalované procesy nebyly měněny.

Testované sestavení nastavení: SPL SHA-256 `09EF092AE08FE42DC8C61C15FFE3A944B58AFBFEE09D3C67580C31A854E9339F`, SLG `2AF5A236F1A6DB0B5007CDAD9DF73025EB63ED11E0EAAD1BF731847FFB6681FA`. Starší počty a GUI hashe výše patří etapě před doplněním volby.

## Co zůstává na další etapu

Neprováděl se přepis/mazání, spojení obnovy s destruktivním executorem, SMB, reparse/ACL/ADS závodové kampaně ani power-loss test. Dosavadní [skutečný VHD test nových kopií](directory-sync-plugin-vhd-results.md) nelze vydávat za nový recovery test. Nový VHD test ani vyšší oprávnění nejsou podmínkou dokončení této běžné etapy.

Další krok po samostatném schválení: navázat úzký přepis na právě uloženou a zalogovanou obnovu, staging nového obsahu a původní cílovou DACL; ověřit běžný soubor s odlišnými ADS a nezměněný další hardlink. Tato etapa takové nahrazení nezavádí.
