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

## Nucené uvolnění a ukončení hostitele (15. října 2026)

Ověřen aktuální plugin z `7bcb6a74`, bez změn produkčního kódu, SDK nebo hostitele. Přibyly pouze níže popsané regresní testy. GUI běželo v odděleném Release x64 hostiteli 0.16 přes Computer Use; instalované uživatelské instance nebyly ukončovány. Použito jen místní NTFS.

| Případ | Výsledek a hranice důkazu |
| --- | --- |
| Skutečný `Plugin::Release(FALSE)` při běžícím workeru | Testovací DLL odmítla běžný unload při skenu i skutečném čtení obsahu; odmítnutí nenastavilo cancel. |
| Skutečný `Plugin::Release(TRUE)` při skenu a porovnání | Přerušení, join workeru a blokování částečných návrhů před návratem. Následné okamžité `FreeLibrary` odmapovalo DLL; nové načtení fungovalo. Obsahové handly nezůstaly otevřené. Každá fáze třikrát v každém sestavení. |
| Běžné ukončení během skenu v GUI | Při aktivní enumeraci (přes 5 900 položek, compared 0) první Alt+F4 zavřel náhled a vrátil ovládání panelům. Druhý Alt+F4 ukončil hostitele; proces již neběžel. |
| Běžné ukončení během porovnávání obsahu v GUI | Při skutečném čtení (přes 11 miliard bajtů, completed pairs 0) stejné chování: nejprve zavření náhledu, potom ukončení hostitele. Bez pozorovaného pádu; návratový kód procesu nebyl měřen. |
| Násilné ukončení procesu při skenu | Po zachycení Scanning 6 725 / compared 0 byl ukončen pouze ověřený vlastní PID 17312 přes `Stop-Process -Force`; proces zanikl. |
| Násilné ukončení procesu při čtení | Po zachycení 10 513 678 336 přečtených bajtů / completed pairs 0 byl stejně ukončen vlastní PID 6284; proces zanikl. Toto ukončení obchází `Release`, nedokazuje provedení cleanup kódu pluginu. |

Automatický `dirsync_lifecycle_tests.vcxproj` sestaví samostatný `dirsync_lifecycle_probe.dll`, který zahrnuje skutečný `dirsync.cpp` a engine. Testovací exporty nejsou součástí dodávaného SPL a DLL se neregistruje do hostitele. Test počká na nenulový počet skenovaných položek nebo přečtených bajtů v stále běžícím workeru, zavolá reálnou metodu Release a následně DLL skutečně uvolní. Prošel Debug i Release x64: **347 kontrol v každém sestavení**, včetně přípravy fixture, tedy nikoli 347 samostatných scénářů. Šest cyklů načtení/uvolnění na sestavení; lokálně se force Release vždy vešel do testovaného limitu 3 s. To není záruka pro zaseknuté I/O nebo SMB. Projekt automaticky najde stávající native runner díky názvu `*_tests.vcxproj`; vzdálené CI nových lokálních souborů zatím neproběhlo.

Hranice nuceného unloadu: testovací Preview nemá připojené GUI okno a neběží uvnitř modalního menu callbacku. Test proto prokazuje ukončení workeru a skutečné odmapování jeho kódu, nikoli bezpečnost reentrantního `FreeLibrary` při aktivním dialogovém callbacku. Běžné GUI při modalním náhledu správce pluginů neumožňuje otevřít. Audit `CPluginData::Unload` v `src/plugins1.cpp` potvrzuje volání Release před FreeLibrary; smlouva force je v `src/plugins/shared/spl_base.h`. Ochrana běžného ukončení při `AlreadyInPlugin > 0` je v `src/mainwnd3.cpp`. Nebyla obcházena ani měněna. Kritický `WM_ENDSESSION` má samostatnou hostitelskou větev; odhlášení, restart a vypnutí Windows nebyly provedeny. Násilné ukončení procesu je nenahrazuje.

GUI fixture: 40 000 prázdných adresářů s 2 000 neodpovídajícími vylučovacími maskami pro pomalejší sken, protější kořen prázdný; pro obsah dva shodné 64GiB sparse soubory. Po všech ukončeních zůstalo 40 000 adresářů a prázdný protějšek; oba soubory zachovaly délku 68 719 476 736 B, UTC mtime `2026-10-15T00:00:00Z` a nulové alokované rozsahy. Oba šly výhradně otevřít ke čtení. Tím je ověřeno uvolnění handlů i nezměněný nepojmenovaný sparse/zero proud. Konfigurace soukromého hostitele byla obnovena ze zálohy.

Protokoly mimo Git: `C:\Samandarin\builds\dirsync\lifecycle-20261015`: `native-debug.log`, `native-release.log`, `scan-normal.json`, `content-normal.json`, `scan-killed.json`, `content-killed.json` a `fixtures-after.json`. Chyba produkčního lifecycle kódu se při těchto kontrolách neprojevila; žádná změna SDK ani zápisová funkce nepřibyla.

## Klávesnice a kontrola rozložení pro různá DPI (15. října 2026)

Ověření vychází z `22cdb3ab` a lokálních oprav níže. Skutečný náhled běžel v soukromém Release x64 hostiteli 0.16 se schématem Windows Dark Mode, ovládaném přes Computer Use. Systémové měřítko se nezměnilo: Settings → System → Display hlásí vzdálenou relaci a zakazuje změnit nastavení displeje; současně uvádí vlastní scale factor. Nebylo vynucováno odhlášení ani obcházena tato ochrana. Efektivní DPI konkrétního dialogu nebylo změřeno, nelze je dovozovat z velikosti screenshotu.

Nalezeny a opraveny dvě chyby ovládání:

- `By &content` a `&Compare` sdílely Alt+C, `&Subfolders` a `Only &selected` sdílely Alt+S. Původní Alt+C pouze zaměřil obsahovou volbu. Anglická šablona nyní používá jedinečné mnemoniky; přibyly také k filtrům, souhrnu a použití návrhu. Nabídka akcí má viditelný popisek Proposal s Alt+P, ukotvený se spodním ovládáním při změně velikosti. Texty zůstávají v přeložitelném SLG.
- Spuštění deaktivovalo ovládací prvek s fokusem a ponechalo dialog bez funkčního klávesového cíle. Po dokončení nefungovaly ani Tab a mnemoniky, dokud uživatel neklikl myší. Start nyní předá fokus na Stop; po dokončení se fokus ze Stop vrátí do výsledků, případně na Compare při prázdném seznamu. Pokud uživatel během práce zaměří jiný aktivní prvek, dokončení mu fokus nepřebírá. Selhání vytvoření workeru obnoví fokus na Compare; tato výjimečná větev nebyla uměle vyvolána.

| Skutečný klávesový/UI scénář po opravě | Výsledek |
| --- | --- |
| Otevření přes Alt+P v hostiteli, D, Enter | Náhled se otevřel bez myši; výchozí levý kořen byl vybraný. |
| Alt+C, potom Alt+Q | Tři výsledky; Alt+Q skryl Equal, zůstaly dva jednostranné řádky a hidden 1. Ověřuje i funkční klávesnici po dokončení. |
| Alt+P v náhledu, Shift+Tab, Home, Shift+Down, Tab | Přechod z comboboxu do seznamu a zpět; označeny oba viditelné řádky. Výběr přežil přesun fokusu. |
| Down a Home v comboboxu, Alt+W | Šipka změnila volbu na Copy to right, Home zpět na Skip. Alt+W změnil oba vybrané návrhy na Skip; neprovedl souborové operace. |
| Alt+Y, Enter | Souhrn se otevřel a potvrdil nulu kopií/mazání a jeden skrytý řádek. Enter zavřel pouze souhrn. |
| Alt+M, zápis `left*`, Enter | Úprava masky ihned zneplatnila starý náhled; Enter z editace spustil Compare a zobrazil jediný levý soubor. |
| Alt+Space, X; Alt+Space, R | Maximalizace a návrat. Seznam a kořeny se roztáhly, Proposal a spodní tlačítka zůstaly pohromadě; všechny sloupce byly při maximalizaci dosažitelné. Nejde o změnu DPI. |
| Alt+O, Alt+C při 64GiB sparse páru, mezerník | Samostatná mnemonika obsahu fungovala. Během čtení byl Stop viditelně zaměřený; při přes 3,5 miliardách přečtených bajtů / completed pairs 0 mezerník přerušil práci. Cancelled, jediný částečný řádek Blocked. |
| Alt+C po Cancelled, Escape při novém čtení | Nový běh začal; při přes 5 miliardách přečtených bajtů Escape zavřel náhled a vrátil ovládání panelům. Hostitel pak šel běžně ukončit. |

Rozložení pro další DPI ověřuje nový nativní `dirsync_ui_tests.vcxproj`. Do testovacího EXE se zkompiluje skutečný `lang.rc`; test čte `DLGTEMPLATEEX`, kontroluje jedinečnost mnemonik a Tab styly hlavních prvků, hranice a překrývání ovládání. Pro 96, 120, 144, 192 a 240 DPI (100–250 %) vytváří GDI font předepsaného `MS Shell Dlg` / 8 pt, počítá dialogové jednotky z jeho metrik a měří popisky včetně rezervy na tlačítka a checkboxy. Ve všech pěti případech popisky vešly a prvky se nepřekrývaly. Combo se posuzuje zavřené, nikoli jako rozbalený seznam. Debug i Release x64 prošly: **937 kontrol**, převážně včetně bezpečného čtení resource; nejde o 937 vizuálních scénářů. Test nevytváří ani neautomatizuje okno a najde jej stávající native runner.

**Neověřeno:** skutečné vykreslení a interakce na monitorech s jiným DPI, přesun otevřeného dialogu mezi různými DPI, `WM_DPICHANGED`, dostupnost celého minimálního dialogu na malých pracovních plochách při vysokém měřítku, překlady, vysoký kontrast a čtečka obrazovky. Metriky fontu nenahrazují kontrolu tématu, renderingu checkboxů, scrollbars, focus indikátorů a hostitelského škálování. Plugin nemá vlastní handler `WM_DPICHANGED`; z těchto testů nelze prohlásit dynamické přepínání DPI za podporované. Zbývající monitorové scénáře je třeba dokončit v relaci, která změnu měřítka umožňuje.

Debug plugin/jazyk Win32, x64, ARM64 a Release x64 se po opravách sestavily s warnings-as-errors. Znovu prošel Debug x64 lifecycle test (347 kontrol), který kompiluje změněný `dirsync.cpp`. Aktuální vzdálené CI platí pro `22cdb3ab`, ne pro tyto necommitované opravy.

Důkazy mimo Git: `C:\Samandarin\builds\dirsync\dpi-keyboard-20261015`, `ui-debug.log`, `ui-release.log`, `lifecycle-debug.log`, `before.json`, `after.json`, `sparse-after.json`. Všechny čtyři malé vstupní soubory zachovaly SHA-256, délku a UTC mtime; oba velké soubory zachovaly 64 GiB, mtime a nulové alokované rozsahy a šly výhradně otevřít ke čtení. Velké fixture byly poté odstraněny a konfigurace soukromého hostitele obnovena. SMB, SDK a zápisový backend se neměnily.

## NTFS identity a změny po skenu (15. října 2026)

Místní etapa nad `b8a6f409` doplnila read-only kontrolu hotového plánu. Neobsahuje zápisový executor, backup, staging ani journal. Mutace níže provádí výhradně test nad vlastními syntetickými fixture; plugin otevírá pouze existující položky ke čtení a dotazům.

Původní engine neukládal file ID a před čtením ověřoval jen velikost/mtime. Nově ukládá serial number svazku + 64bit NTFS file index, ChangeTime, počet hardlinků, atributy a metadata. Snapshot vlastní i kořeny, volby, stromy a identity rodičů. Komponenty rodičů se otevírají postupně bez následování reparse pointů a drží po dobu enumerace/čtení; používá se `FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES` a sdílení pouze pro čtení. Samotný atributový handle se při sondě neprokázal jako dostatečná ochrana přejmenování. Piny se neuchovávají po celou dobu zobrazení náhledu. Přibylo odmítnutí ADS a vloženého NUL v kořeni, kontrola kanonických aliasů a oprava lexikálního překryvu kořene svazku s potomkem. Stejná fyzická identita na obou stranách souborového páru je blokovaná.

`Revalidate` provádí nový sken téhož rozsahu bez čtení obsahu a porovnává snapshoty; změna, nová chyba nebo zrušení trvale zablokuje všechny původní řádky až do nového Compare. Nezměněné známé nepodporované položky zůstávají blokované a nebrání validaci nesouvisejících řádků. UI provádí validaci na workeru před Apply a Plan summary. Pracuje s kopií výsledku, aby se neměnila data současně čtená listview. Požadované řádky a akce se zachytí při stisku Apply; pozdější přesun výběru během kontroly žádost nezmění. Ověření neprovádí souborové operace. Neexistuje průběžný watcher: na displeji může zůstat snapshot až do dalšího checkpointu.

| Řízená místní NTFS kontrola | Výsledek |
| --- | --- |
| Nezměněný plán a ruční návrh | Opakovaná validace prošla a zachovala návrh. |
| Zápis jiných čtyř bajtů do stejného objektu, stejná délka a obnovený původní mtime | Změna vlevo i vpravo zneplatnila plán díky novému fingerprintu; samotná porovnávací tolerance se při validaci nepoužívá. |
| Nový objekt se stejným názvem, daty, velikostí a mtime | Validace zablokovala plán. Při výměně mezi skenem a obsahovým čtením byl pár Blocked a readBytes 0. |
| Zmizení zdroje, zmizení existujícího cíle, vznik dříve chybějícího cíle, nový soubor, změna souboru na složku | Každá mutace zablokovala původní plán; další validace jej neobnovila. |
| Změna atributů, mtime nebo vytvoření dalšího hardlinku mimo skenované kořeny | Každá zneplatnila plán. |
| Výměna kořenového adresáře po dokončení Compare | Validace zneplatnila plán; operace fixture zároveň prokázala uvolnění pinů po Compare. |
| Výměna podsložky po skenu za jiný obyčejný adresář se shodně pojmenovaným souborem | Obsahové čtení blokováno podle identity rodiče, readBytes 0. |
| Výměna podsložky po skenu za junction do vlastního `outside` | Čtení blokováno bez přečtení dat z cíle junction, readBytes 0; také následná validace starého plánu selhala. Testovací hook existuje jen při `DIRSYNC_ENGINE_TEST`, není v SPL. |
| Během skutečně připraveného čtení s drženými handly | Pokus o přejmenování kořene, otevření zapisovače dat a otevření rodiče pro GENERIC_WRITE selhal; následné čtení původního páru prošlo. Jde o konkrétní Win32/NTFS pokusy, ne důkaz proti všem privilegovaným nebo mappingovým závodům. |
| Stejný objekt přes hardlink na obou stranách páru | Návrh blokován. Nejde o úplnou analýzu aliasů mezi různými řádky budoucího prováděcího plánu. |
| Kořeny přes `..`, skutečný 8.3 alias téhož kořene, kořen svazku s potomkem; ADS/NUL vstup | Odmítnuty. 8.3 jméno bylo v tomto místním běhu skutečně dostupné; na volume bez krátkých jmen test tuto dílčí větev výslovně oznámí jako nedostupnou. SUBST nebyl testován. |
| Dlouhé kořeny s více CJK komponentami a soubor `žluťoučký 😀.txt` | Nezměněný snapshot prošel, následná změna dat s obnoveným mtime jej zneplatnila. Původní matice dlouhých/Unicode cest také znovu prošla. |
| Cancel před validací i během jejího vlastního průchodu | Plan stale/cancelled, všechny původní řádky Blocked. |

Vše běželo na NTFS; nové scénáře to ověřují přes GetVolumeInformationW. Nativní engine test nyní prošel v Debug i Release x64 s **539 assertions** včetně původních testů a přípravy fixture, nikoli 539 samostatných scénářů. Debug lifecycle test znovu prošel se 347 kontrolami a resource/layout s 937. Plugin a jazyk prošly Debug Win32/x64/ARM64 a Release x64 s warnings-as-errors.

Computer Use v izolovaném Release x64 hostiteli ověřilo Apply a souhrn po validaci: ruční Copy to right se zachovalo a souhrn ukázal 1 / 4 B. Řízený zápis `AAAA → BBBB → CCCC` do levého testovacího souboru při zachování 4 B a UTC mtime zneplatnil starý plán před souhrnem i před Apply; návrh se změnil na Blocked a dialog požadoval nové Compare. Nezměněný pravý soubor zůstal `AAAA`; žádná navržená kopie se neprovedla. Při přípravě se deaktivace listview v Busy projevila bílým pozadím po opětovném zapnutí ve schématu Windows Dark Mode; finální řešení seznam nedeaktivuje a zachytí žádost před validací. Finální tmavý náhled a změna návrhu byly znovu ověřeny. Soukromý hostitel byl ukončen a jeho konfigurace obnovena.

Protokoly mimo Git: `C:\Samandarin\builds\dirsync\safety-20261015`, `engine-Debug.log`, `engine-Release.log`, `lifecycle-Debug.log`, `ui-Debug.log`, `ui-after.json`. Vzdálené CI se pro tyto lokální změny zatím nespouštělo.

**Hranice důkazu:** validace je metadata checkpoint, není atomický snapshot ani oprávnění k pozdějšímu zápisu. Neověřuje hashe obsahu/ADS, změny po svém posledním pozorování, všechny souběžné změny během skenu, záměrné obnovení ChangeTime, file-ID reuse, zapisovatelné mappingy, case-sensitive NTFS, SUBST, ACL matici ani jiné filesystémy/SMB. Přísné piny navíc vyžadují možnost listovat rodičovské komponenty; konflikt přístupu/sdílení blokuje větev a nepoužije slabší fallback. Nezměněný metadata náhled může označovat stejně velké/stejně staré různé soubory jako Equal, pokud nebylo zvoleno By content; nová validace tuto schválenou porovnávací sémantiku nemění.

[Návrh protokolu provedení a obnovy](directory-sync-plugin-execution-protocol.md) rozlišuje čerstvou validaci, stabilní operační handly, nezávislou obnovu, ověřený staging, journal, commit a obnovu po pádu. Kandidát dvou rename má výslovně popsanou mezeru v existenci cíle a vyžaduje samostatnou NTFS sondu. Bez důkazu těchto kroků zůstávají zápisy zakázané.

## Sestavení a testy

Pluginový projekt: `src/plugins/dirsync/vcxproj/dirsync.vcxproj`; jazykový projekt je jeho závislost. Používá stávající shared props a platformy solution. Nativní test je pojmenován `*_tests.vcxproj`, takže jej najde existující `tools/run_native_tests.ps1`.

Příklad přímého lokálního sestavení (cestu k MSBuild zvolit podle instalace):

```powershell
& $msbuild src/plugins/dirsync/vcxproj/dirsync.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\
& $msbuild src/plugins/dirsync/tests/dirsync_tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\
& C:\Samandarin\builds\dirsync\tests\Debug_x64\dirsync_tests.exe
& $msbuild src/plugins/dirsync/tests/dirsync_lifecycle_tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\
& C:\Samandarin\builds\dirsync\tests\Debug_x64\dirsync_lifecycle_tests.exe
& $msbuild src/plugins/dirsync/tests/dirsync_ui_tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\
& C:\Samandarin\builds\dirsync\tests\Debug_x64\dirsync_ui_tests.exe
```

Pro načtení jsou potřeba `dirsync.spl` a sousední `lang/english.slg`; zkopírovat celou složku pluginu. Installer obsahuje příslušné položky a automatická registrace je ověřena v oddělené testovací distribuci přes `plugins.ver`. Sestavení celého Inno balíčku nebylo provedeno.

## Meze a další etapy

### Izolovaná sonda zápisového protokolu (15. října 2026)

Mimo plugin vznikl samostatný `tests/protocol_probe.vcxproj` bez SDK/engine závislosti, který zapisuje výhradně nová syntetická data. Debug i Release x64 prošly 347 assertions a 53 pomocnými procesy v každém běhu. Ověřeny kill hranice přepisu, nového cíle i mazání, neúplný/poškozený journal, kooperativní Stop, kolize, Verify, nezávislá záloha, ruční obnova do nového jména, testované DACL/atributy a dlouhá Unicode cesta. Skutečné Access denied a sharing chyby jsou oddělené od injekcí plného disku a flush. [Úplné výsledky, reprodukce a zbývající meze](directory-sync-plugin-ntfs-protocol-probe.md).

Sonda současně prokázala dvě překážky převzetí návrhu do produkce: striktní read-only parent sharing odmítá vlastní rename a hlavní datový proud nezamyká ADS. Experimentální native rename potřeboval širší parent sharing; není to schválený bezpečnostní fallback. Finální atributy bylo nutné nastavit až po publikaci, protože rename přidal Archive. V6 zůstává otevřená. Pluginové zdroje, SDK, hostitel a běžné CI se v této etapě nezměnily; zápisový executor ani SMB testy nevznikly.

### Zbývající omezení

- Omezení V5 zůstává přijaté: panelový kořen přichází jako bajty, dekódování je UTF-8 nejdříve a ACP při neplatném UTF-8. Ztracenou nebo nejednoznačnou historickou informaci nelze tímto adaptérem zpětně rekonstruovat. Selhání SDK/převodu/otevření musí být viditelné; backend nemá nový obecný limit MAX_PATH. Místní runtime případy výše nezaručují všechny délky, historická ACP jména nebo UNC. Hostitelská navigace dlouhého kořene navíc obsahuje prokázaný pád, který plugin před svým spuštěním nemůže ohlásit.
- NTFS identity, checkpointy a držení rodičů při čtení mají výše uvedené konkrétní výsledky a meze. Nejde o dokončený zápisový protokol; před zápisy zbývá V6 včetně operačních handlů, kolizí, záloh, ADS/ACL a obnovy po pádu podle samostatného návrhu. Bezpečnost SMB se z místního NTFS nevyvozuje.
- Místní Stop/Close, skutečný force Release workeru s odmapováním testovací DLL a běžné i násilné ukončení vlastního hostitele při skenu/obsahu jsou ověřené výše. Neověřen zůstává force unload s aktivním GUI callbackem, kritický shutdown Windows a pomalé nebo zaseknuté SMB I/O. UI smoke nepokrývá všechny DPI, překlady, klávesové a přístupnostní scénáře. První jazyk je English v samostatném přeložitelném SLG.
- Profily, otevření externího porovnávače a vykonání plánu nejsou součástí této první etapy. Plán je jen v paměti a po novém porovnání se znovu vytvoří.
- SMB runtime nebylo podle pokynu uživatele použito. ARM64/Win32 jsou sestavené, ale GUI běželo pouze na x64. Příslušné CI se vyhodnotí až pro publikovaný aktuální commit; lokální build není výsledek vzdáleného draft PR.

Resource-only jazykový projekt má vypnuté inkrementální linkování. Při opakovaném Debug buildu MSVC 14.51 došlo k internímu pádu IMAGE::EmitRelocations. Původní zákaz v `lang_dirsync.props` přepisoval pozdější import sdíleného `lang_debug.props`; čisté sestavení toto neodhalilo. Nastavení je nyní až za importy v `lang_dirsync.vcxproj`, vyhodnocená vlastnost `LinkIncremental` je `false`. Čisté Debug buildy všech tří architektur, následný běžný x64 Build a Release x64 prošly. Sdílené props ani workflow se neměnily.
