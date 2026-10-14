# Directory Synchronization: první etapa

Stav k 15. říjnu 2026: implementována kostra nativního pluginu `dirsync`, sken, porovnání a náhled **bez provádění změn**. Navazuje na [schválený návrh](directory-sync-plugin-design.md), [technický audit](directory-sync-plugin-sdk.md) a [pozorování TC](directory-sync-plugin-tc-observations.md).

## Rozsah

- Existující SDK Samandarin-0.16, minimální SDK verze 106. Žádné změny SDK hlaviček, virtuálních metod nebo hostitelského kódu. Registrace projektů, build adres a balíčku je přidána běžným způsobem.
- `src/plugins/dirsync/dirsync.cpp`: vstup pluginu, snapshot fyzických panelů a sjednoceného výběru na hlavním vlákně, lokalizovaný modalní dialog, worker, Stop/Close a hostitelské darkmode přes sdílenou knihovnu. Návrat zachovává označení panelů.
- `engine.cpp/.h`: samostatný engine bez SDK callbacků. Iterativní enumerace přes široká WinAPI, dlouhé prefixy, metadata v UTC a proudové binární porovnání. Otevření obsahu vyžaduje stabilní velikost/mtime proti skenu a při čtení nepovoluje běžné souběžné zapisovače.
- Tolerance 3 s včetně, bez hodinové korekce. Čas mimo toleranci má přednost před obsahovým porovnáním, jak určuje ověřená tabulka TC. Ignorované datum a stejně staré rozdílné páry vyžadují ruční směr. Asymetrie nenavrhuje přepsání novějšího pravého páru; pravé jednostranné položky mají pouze návrh mazání.
- Základní masky: `*`, `?`, `**`, kořenový `\`, vyloučení za `|`, více masek oddělených mezerou nebo středníkem, uvozovky pro mezery v názvu. Regex a uložená hledání jsou odmítnuta. Vyloučený podstrom se neprochází. U vybrané složky se zahrnou přímé děti i bez hlubší rekurze.
- Nečitelné/neúplné podstromy a nepodporované reparse, cloudové či EFS položky jsou blokované a chrání protějšek. Konflikt soubor/složka blokuje i potomky. Nejednoznačné párování jmen nemá automatickou akci. Prázdná složka musí být skutečně prázdná, nikoli pouze prázdná po filtraci.
- Náhled odděluje stav, akci a důvod. Podporuje výběr více řádků, ruční směr/přeskočení a filtry. Blokované a konfliktní řádky zůstávají viditelné. Souhrn počítá jen viditelné návrhy a uvádí počet skrytých řádků. Změna kořenů, masek nebo porovnávacích voleb zneplatní celý starý náhled.
- Close během práce nejprve požádá o přerušení a vyčká na worker. Částečný výsledek po Cancel obsahuje jen blokované řádky. Běžný unload při otevřeném dialogu je odmítnut; force release přeruší a joinuje worker před návratem.

Veškeré názvy Copy/Delete jsou **pouze návrhy**. V pluginu není executor, kopírování, přepis, mazání ani obnova; není dostupné tlačítko pro provedení. Složka obnovy a její nastavení přijdou až se zápisovým backendem D4/V6.

## Ověření

| Kontrola | Výsledek |
| --- | --- |
| Debug plugin + english.slg | Čisté sestavení Win32, x64 a ARM64; x64 též přes hlavní solution |
| Release plugin + english.slg | Sestavení x64 |
| Nativní `tests/dirsync_tests.vcxproj` | 162 úspěšných kontrol: osm kombinací TC, obě hranice 3 s, masky, výběry, skutečné binární čtení, zámek zdroje, junction a ochrana jejích protějšků, vyloučený soubor proti složce a naopak, konflikt s potomky, prázdné složky bez rekurze, stejné/vnořené/device kořeny, cancel a Unicode cesta nad 260 UTF-16 jednotek |
| Nezměněný hostitel ze základu 0.16 | Samostatně sestaven Debug i Release x64; plugin načten přes běžné menu Release hostitele |
| Computer Use, světlé schéma | 16 řádků na původní TC sadě. Obsahové porovnání odhalilo rozdíl stejných velikostí a časů. Souhrn 5 kopií doprava / 20 B, 4 doleva / 18 B, 4 nerozhodnuté. Po skrytí pravolevých kopií 0 těchto kopií a 4 skryté řádky |
| Computer Use, Windows Dark Mode schéma hostitele | Tmavý titulek, dialog, controly a seznam. Sjednocení levé selA a pravé selB dalo dva páry při zamčených kořenech. Maximalizace rozšířila seznam a přesunula spodní ovládání. Ruční směr změnil pouze vybraný řádek |
| Důkaz bez zápisů do vstupů | Všech 30 souborů TC fixture po UI testech má původní SHA-256, délku a mtime |

Build a runtime jsou mimo checkout pod `C:\Samandarin\builds\dirsync`. Protokoly vstupů: `fixture-before.json` a `fixture-after-check.json`. Zkušební hostitel má vlastní `configstorage.ini` s RegFile a sousední `config.reg`; neupravuje konfiguraci instalovaného Salamanderu. Testovací instance byla po smoke testech ukončena.

Při přípravě se Debug hostitel zastavil na diagnostice evidence GDI handlů v `src/iconlist.cpp` ještě před otevřením pluginového dialogu. Kontrolní instance `debug-baseline` se stejným Debug hostitelem, bez registrace pluginů a bez jakéhokoli SPL, vyvolala tutéž diagnostiku `handles.cpp:864 / iconlist.cpp:483`; záznam je `debug-baseline-result.json` ve výše uvedeném build adresáři. UI ověření proběhlo v Release hostiteli; problém Debug hostitele nebyl touto změnou opravován. Izolovaná distribuce též hlásila chybějící běžící `salmon.exe`; hlášení bylo při smoke testu potvrzeno. Tato pomocná distribuce není instalační balíček.

## Sestavení a testy

Pluginový projekt: `src/plugins/dirsync/vcxproj/dirsync.vcxproj`; jazykový projekt je jeho závislost. Používá stávající shared props a platformy solution. Nativní test je pojmenován `*_tests.vcxproj`, takže jej najde existující `tools/run_native_tests.ps1`.

Příklad přímého lokálního sestavení (cestu k MSBuild zvolit podle instalace):

```powershell
& $msbuild src/plugins/dirsync/vcxproj/dirsync.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\
& $msbuild src/plugins/dirsync/tests/dirsync_tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\
& C:\Samandarin\builds\dirsync\tests\Debug_x64\dirsync_tests.exe
```

Pro načtení jsou potřeba `dirsync.spl` a sousední `lang/english.slg`; zkopírovat celou složku pluginu. Installer obsahuje příslušné položky a automatická registrace je ověřena v oddělené testovací distribuci přes `plugins.ver`. Sestavení celého Inno balíčku nebylo provedeno.

## Meze a další etapy

- Omezení V5 zůstává přijaté: panelový kořen přichází jako bajty, dekódování je UTF-8 nejdříve a ACP při neplatném UTF-8. Ztracenou nebo nejednoznačnou historickou informaci nelze tímto adaptérem zpětně rekonstruovat. Selhání SDK/převodu/otevření musí být viditelné; backend nemá nový obecný limit MAX_PATH. Skutečný SDK snapshot Unicode/UNC a všech dlouhých variant vyžaduje další runtime matici; nativní wide test jej nenahrazuje.
- Statické vynechání reparse points a kontroly před/po čtení nepředstavují dokončenou ochranu před všemi závody výměny rodiče. Před zápisy je nutné prokázat identity, aliasy a držení rodičů podle V6. Přesná bezpečnost komerčních SMB serverů se z místního NTFS nevyvozuje.
- Nativní cancel byl ověřen na vstupu; Stop/Close během pomalého SMB I/O, nucený unload a kritický shutdown vyžadují řízené lifecycle testy. UI smoke nepokrývá všechny DPI, překlady, klávesové a přístupnostní scénáře. První jazyk je English v samostatném přeložitelném SLG.
- Profily, otevření externího porovnávače a vykonání plánu nejsou součástí této první etapy. Plán je jen v paměti a po novém porovnání se znovu vytvoří.
- SMB runtime nebylo podle pokynu uživatele použito. ARM64/Win32 jsou sestavené, ale GUI běželo pouze na x64. Příslušné CI se vyhodnotí až pro publikovaný aktuální commit; lokální build není výsledek vzdáleného draft PR.

Resource-only jazykový projekt má vypnuté inkrementální linkování. Při opakovaném Debug buildu MSVC 14.51 došlo k internímu pádu IMAGE::EmitRelocations; po této projektové úpravě prošla čistá sestavení všech tří architektur.
