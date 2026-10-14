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
| Nativní `tests/dirsync_tests.vcxproj` | 205 úspěšných kontrol po doplnění místní matice cest a přerušení; zahrnují i původní porovnávací tabulku TC, masky, konflikty a ochranu protějšků |
| Nezměněný hostitel ze základu 0.16 | Samostatně sestaven Debug i Release x64; plugin načten přes běžné menu Release hostitele |
| Computer Use, světlé schéma | 16 řádků na původní TC sadě. Obsahové porovnání odhalilo rozdíl stejných velikostí a časů. Souhrn 5 kopií doprava / 20 B, 4 doleva / 18 B, 4 nerozhodnuté. Po skrytí pravolevých kopií 0 těchto kopií a 4 skryté řádky |
| Computer Use, Windows Dark Mode schéma hostitele | Tmavý titulek, dialog, controly a seznam. Sjednocení levé selA a pravé selB dalo dva páry při zamčených kořenech. Maximalizace rozšířila seznam a přesunula spodní ovládání. Ruční směr změnil pouze vybraný řádek |
| Důkaz bez zápisů do vstupů | Všech 30 souborů TC fixture po UI testech má původní SHA-256, délku a mtime |

Build a runtime jsou mimo checkout pod `C:\Samandarin\builds\dirsync`. Protokoly vstupů: `fixture-before.json` a `fixture-after-check.json`. Zkušební hostitel má vlastní `configstorage.ini` s RegFile a sousední `config.reg`; neupravuje konfiguraci instalovaného Salamanderu. Testovací instance byla po smoke testech ukončena.

Při přípravě se Debug hostitel zastavil na diagnostice evidence GDI handlů v `src/iconlist.cpp` ještě před otevřením pluginového dialogu. Kontrolní instance `debug-baseline` se stejným Debug hostitelem, bez registrace pluginů a bez jakéhokoli SPL, vyvolala tutéž diagnostiku `handles.cpp:864 / iconlist.cpp:483`; záznam je `debug-baseline-result.json` ve výše uvedeném build adresáři. UI ověření proběhlo v Release hostiteli; problém Debug hostitele nebyl touto změnou opravován. Izolovaná distribuce též hlásila chybějící běžící `salmon.exe`; hlášení bylo při smoke testu potvrzeno. Tato pomocná distribuce není instalační balíček.

## Doplňující místní ověření cest a Stop/Close (15. října 2026)

Ověřen stejný Release x64 hostitel 0.16 a plugin z commitu `8e1ac316`, s vlastní přenosnou konfigurací a Windows Dark Mode schématem. Ovládání skutečných panelů a náhledu proběhlo přes Computer Use. SMB nebylo použito. Nové fixture jsou oddělené od původních TC dat pod `C:\Samandarin\builds\dirsync\validation-20261015`.

| Případ | Výsledek a hranice důkazu |
| --- | --- |
| Unicode kořeny z obou panelů, více komponent `žluťoučký 東京\vnořená 😀` | SDK snapshot zachoval přesné kořeny; `ascii.txt` i `žluťoučký 東京 😀.txt` se obsahově porovnaly jako Equal. Bez blokovaných řádků. |
| Kořen kratší než 260 UTF-16 jednotek, ale delší než 260 UTF-8 bajtů | Kořeny s komponentou 70× `漢`: vlevo 129 UTF-16 / 269 UTF-8, vpravo 130 / 270 (bez terminátoru). Navigace obou panelů, přesný snapshot i obsahové porovnání jednoho páru prošly. |
| Dlouhé kořeny zadané přímo v pluginovém dialogu | Pět komponent po 42× `階` + číslice: vlevo 278 UTF-16 / 698 UTF-8, vpravo 279 / 699. Oba páry, včetně Unicode názvu, Equal. To je důkaz wide dialogu a enginu, nikoli úspěšného panelového snapshotu těchto kořenů. |
| Stejný 278jednotkový kořen přes hostitelský Change Directory | Hostitel se ukončil ještě před otevřením náhledu. Kontrolní Release instance bez jakéhokoli SPL a bez načtených pluginových modulů zopakovala pád se stejným `ucrtbase.dll`, offsetem `0x11858` a výjimkou `0xc0000409`. Oba hostitelské EXE měly shodný SHA-256. Není to bezpečně hlášené omezení; jde o známý pád hostitele, který tento plugin nedokáže zachytit před svým spuštěním. |
| Nativní wide engine | Navíc prošly přímo zadané dlouhé kořeny, UTF-8-only dlouhé kořeny, výběr emoji názvu a přesné porovnání vybraných 244jednotkových ASCII i CJK názvů souborů. Tyto výběry jsou nativní `Options.selection`, ne další UI důkaz `GetPanelSelectedItem/NameW`. |
| Stop při skenu | 40 000 místních prázdných adresářů, 500 neodpovídajících vylučovacích masek pro dostatečně dlouhý běh. Před Stop byl vidět průběžný počet 27 352 / compared 0. Následoval Cancelled bez řádků a bez návrhů; Compare a vstupy byly opět dostupné. |
| Close při novém skenu po Stop | Nový běh začal, byl vidět průběžný počet přes 26 000 / compared 0. Close zavřel dialog a vrátil ovládání panelům. Hostitel pokračoval; plugin šel znovu otevřít. |
| Stop během obsahového porovnání | Dva shodné 64GiB sparse soubory `a-large.bin` a za nimi malý pár `z-after.txt`. Při čtení bylo scanned 4 / compared 0. Stop dal Cancelled s jediným blokovaným řádkem velkého páru; pozdější pár nebyl porovnán. |
| Close během znovu spuštěného obsahového porovnání | Nové čtení téhož páru začalo; Close vrátil ovládání panelům. Další otevření pluginu a úspěšné porovnání krátkého páru prokázalo opětovnou použitelnost. |

První rychlý sken se dokončil dříve, než se podařilo doručit Stop; výsledek Complete z tohoto pokusu není započítán jako úspěšný test přerušení. Zátěž s maskami zajišťovala skutečně rozběhnutou enumeraci. Z časování Computer Use nevyvozujeme přesnou latenci ani její horní mez: zachycení obrazovky samo trvá sekundy.

Automatický test přerušení používá vlastní sparse fixture. Počká na nenulové atomické počítadlo skutečně přečtených bajtů; neotevírá konkurenční zapisovač, který by mohl sám vyvolat sharing violation a udělat test nespolehlivým. Poté nastaví cancel a zavolá stejný `CancelSynchronousIo` jako UI. Ověří návrat do 3 s na místním NTFS, blokování všech částečných řádků, neporovnání pozdějšího páru, uvolnění handlů a úspěšný nový běh. Test zachytává výjimku workeru a joinuje jej před vyhodnocením. Celkem 205 assertions zahrnuje také přípravu fixture, ne 205 samostatných scénářů. Testovací projekt se znovu sestavil s Level4 a warnings-as-errors.

Po UI ověření všech 18 malých souborů cesty zachovalo očekávaná data, 9 B a původní UTC mtime; soupis s SHA-256 je `path-fixtures-after.json`. Velký pár zachoval délku 68 719 476 736 B, UTC mtime a nulový počet alokovaných rozsahů (`content-after.json`), tedy celý nepojmenovaný proud zůstal sparse/zero. Protokol nativních testů: `native-tests.log`. Důkaz kontrolního hostitele: `baseline-modules.json` (prázdný seznam SPL) a `host-crash-events.json`. Velké testovací sady lze po ověření odstranit; důkazy a malá matice cest se uchovávají mimo Git.

Opraveny dvě nalezené závady: stavový text během obsahového čtení nyní uvádí `Comparing content` a rostoucí počet přečtených bajtů z obou stran, místo zdánlivě stojícího `Scanning / compared 0`; počítadlo se resetuje při Compare a nic nezapisuje. Ve finálním Release UI bylo vidět přes 8 miliard přečtených bajtů při 0 dokončených párech, Stop opět zablokoval jediný částečný řádek a při novém běhu zavření křížkem vrátilo ovládání panelům. Druhou opravou je pořadí MSBuild nastavení zákazu inkrementálního linkování SLG, viz níže. Aktuální Debug buildy pluginu/jazyka Win32, x64, ARM64 a Release x64 prošly s warnings-as-errors. Pád navigace je evidován pro samostatnou budoucí opravu hostitele. SDK ani hostitel se v této etapě neupravují. Dosavadní vzdálené CI platí pro `8e1ac316`, nikoli pro nové lokální změny.

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

- Omezení V5 zůstává přijaté: panelový kořen přichází jako bajty, dekódování je UTF-8 nejdříve a ACP při neplatném UTF-8. Ztracenou nebo nejednoznačnou historickou informaci nelze tímto adaptérem zpětně rekonstruovat. Selhání SDK/převodu/otevření musí být viditelné; backend nemá nový obecný limit MAX_PATH. Místní runtime případy výše nezaručují všechny délky, historická ACP jména nebo UNC. Hostitelská navigace dlouhého kořene navíc obsahuje prokázaný pád, který plugin před svým spuštěním nemůže ohlásit.
- Statické vynechání reparse points a kontroly před/po čtení nepředstavují dokončenou ochranu před všemi závody výměny rodiče. Před zápisy je nutné prokázat identity, aliasy a držení rodičů podle V6. Přesná bezpečnost komerčních SMB serverů se z místního NTFS nevyvozuje.
- Místní Stop/Close při skenu i obsahu a nativní cancel během čtení jsou ověřené výše. Stop/Close během pomalého nebo zaseknutého SMB I/O, nucený unload a kritický shutdown vyžadují další řízené lifecycle testy. UI smoke nepokrývá všechny DPI, překlady, klávesové a přístupnostní scénáře. První jazyk je English v samostatném přeložitelném SLG.
- Profily, otevření externího porovnávače a vykonání plánu nejsou součástí této první etapy. Plán je jen v paměti a po novém porovnání se znovu vytvoří.
- SMB runtime nebylo podle pokynu uživatele použito. ARM64/Win32 jsou sestavené, ale GUI běželo pouze na x64. Příslušné CI se vyhodnotí až pro publikovaný aktuální commit; lokální build není výsledek vzdáleného draft PR.

Resource-only jazykový projekt má vypnuté inkrementální linkování. Při opakovaném Debug buildu MSVC 14.51 došlo k internímu pádu IMAGE::EmitRelocations. Původní zákaz v `lang_dirsync.props` přepisoval pozdější import sdíleného `lang_debug.props`; čisté sestavení toto neodhalilo. Nastavení je nyní až za importy v `lang_dirsync.vcxproj`, vyhodnocená vlastnost `LinkIncremental` je `false`. Čisté Debug buildy všech tří architektur, následný běžný x64 Build a Release x64 prošly. Sdílené props ani workflow se neměnily.
