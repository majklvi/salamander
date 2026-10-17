# Synchronizační plugin a technický průzkum SDK 0.16

Redakční příprava exportu (17. října 2026): názvy referenčního programu a odkazy na jeho web jsou nahrazeny neutrálním označením. Historické zdroje v plném znění zachovává [archiv původního forku](https://github.com/majklvi/salamander/tree/af0e81bc1bccb6c733025fe31a97d53adf5cb32f/doc). Cesty s prefixem `reference-sync-probe-` jsou dokumentační aliasy, nikoli přejmenované testovací adresáře. Výsledky a meze ověření se nemění.

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.



**Navazující stav (15. října 2026):** na samostatný pokyn uživatele implementována [kopie pouze do nových cílů](directory-sync-plugin-copy-new.md), včetně stagingu, Verify, metadat, průběhu a Stop. Původní návrhové pasáže níže nejsou tvrzením o přepisech či mazání; ty zůstávají neimplementované. SDK/hostitel beze změn, bez SMB testů. Přesné runtime výsledky a odchylku vlastní CREATE_NEW rezervace stagingu uvádí nový protokol.

Doplněk k [návrhu chování a rozsahu](directory-sync-plugin-design.md). Původní audit SDK proběhl 14. října 2026 před implementací. K 15. říjnu je hotová [první etapa bez zápisů a její ověření](directory-sync-plugin-implementation.md); SDK zůstává nezměněné. Samostatné UI ověření předlohy 11.58 je zachyceno v [protokolu pozorování](directory-sync-plugin-reference-observations.md); technické záruky zápisového enginu tím nejsou ověřené.

Audit proběhl v `C:\Samandarin\directory-sync-plugin` na commitu `a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449`. Odkazy níže jsou připnuté na tento commit forku `majklvi/salamander`; API z jiných rozpracovaných větví se nepředpokládají.

## Závěr a hranice první verze

Nativní C++ plugin pro místní disky a Windows síťové cesty je proveditelný. Doporučeným základem je rozšíření nabídky pluginů, vlastní datový model porovnání a vlastní backend nad širokými Win32 API. Nejde o potvrzení, že existující SDK již poskytuje bezpečný synchronizační engine.

D1 je rozhodnuté: první verze podporuje místní a SMB složky, včetně UNC a mapovaných SMB jednotek. FTP/SFTP, archivy a jiné pluginové souborové systémy jsou odložené. D2 potvrzuje variantu A bez historie: plánovač obousměrného režimu pracuje s aktuálním stavem bez databáze minulých synchronizací a neodvozuje mazání z absence protějšku. D3 potvrzuje variantu A: jednosměrně pouze asymetrie podle předlohy, bez samostatného přesného zrcadlení v první verzi. D4 potvrzuje vlastní složku obnovy pro přepisy i mazání, mimo oba synchronizované stromy; chyba uchování blokuje příslušnou destruktivní operaci. D5 potvrzuje variantu A podle tabulky hlavního návrhu: běžná data a vybrané vlastnosti, cílová pravidla oprávnění, podmíněný přenos ADS a výslovné meze speciálních souborů. Před zápisovou částí je nutné uzavřít dvě technická rozhodnutí:

1. Jak zpracovat kořenovou cestu z existujícího SDK 0.16 a bezpečně hlásit jeho omezení: uživatel 15. října 2026 rozhodl SDK ani hostitele pro první verzi nerozšiřovat.
2. Jak provádět schválený plán s dlouhými cestami, obnovou a opětovným ověřením identity: `SafeFile` ani interaktivní Copy nejsou hotovým řešením.

Produktová rozhodnutí D1–D5 jsou uzavřená. Zbývají ověření V1–V6 hlavního návrhu: přesné kombinace porovnávacích voleb a technické provedení v mezích schváleného rozsahu. Návrh bezpečných zápisů musí předcházet jejich implementaci; chování při chybách a skutečné zachování podporovaných vlastností následně prokážou integrační testy.

## Vstup pluginu, panely a vlastnictví dat

| Potřeba | Existující rozhraní a důsledek |
| --- | --- |
| Registrace pluginu | `SalamanderPluginEntry`, načtení jazykového modulu a `SetBasicPluginData`; vzor [Renamer, ř. 311–360](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/renamer/renamer.cpp#L311-L360). |
| Příkaz nabídky | `Connect`/`AddMenuItem` a `GetInterfaceForMenuExt`; [Renamer, ř. 585–619](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/renamer/renamer.cpp#L585-L619). |
| Spuštění a obnova | `ExecuteMenuItem` má kontrakt pro odznačení, `PostChangeOnPathNotification` a evidenci práce s panelem; [spl_menu.h, ř. 82–97](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_menu.h#L82-L97). |
| Kořenové cesty | `GetPanelPath` rozlišuje Windows/archiv/FS a přijímá kapacitu bufferu; [spl_gen.h, ř. 1467–1481](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L1467-L1481). |
| Fokus a výběr | `GetPanelFocusedItem`, `GetPanelItem`, `GetPanelSelectedItem`, `GetPanelSelection`; [spl_gen.h, ř. 1509–1540](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L1509-L1540). |
| Unicode název | `CFileData.NameW` je volitelné přesné široké jméno; [spl_com.h, ř. 234–239](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_com.h#L234-L239). |

Panelové metody mají omezení na hlavní vlákno. Vrácené položky jsou pouze pro čtení; nejde o snapshot s nezávislou životností. Plugin musí ihned zkopírovat potřebné názvy, typy a metadata do vlastních objektů. Worker nesmí držet ukazatele na živé panelové položky.

Kořeny se zachytí podle skutečné levé/pravé strany, nikoli podle momentálního zdrojového/cílového panelu. Výběr je počáteční omezení rozsahu; kompletní strom pro porovnání se následně enumeruje nezávisle na viditelném seznamu panelu.

## Unicode a dlouhé cesty na hranici SDK

`GetPanelPath` v diskové větvi kopíruje `p->GetPath()` do `char` bufferu. Nepoužívá interní `GetPathW()`: [src/zip.cpp, ř. 1612–1693](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/zip.cpp#L1612-L1693). Ani API karet neposkytuje wide variantu: `GetPanelTabPath` také vrací `char*` ([spl_gen.h, ř. 3660–3665](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L3660-L3665)).

Hostitel wide data má: interní `GetPathW()` je v [fileswnd.h, ř. 569–570](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/fileswnd.h#L569-L570). `ChangePathToDiskW` převádí vstup na UTF-8 před voláním běžné cesty ([fileswn2.cpp, ř. 2782–2791](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/fileswn2.cpp#L2782-L2791)); `SetPath` ukládá i wide mirror ([fileswn1.cpp, ř. 1219–1227](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/fileswn1.cpp#L1219-L1227)). Interní metoda však není automaticky dostupnou SDK metodou.

Schválený směr V5 pro první verzi (15. října 2026): plugin musí běžet se současným Samandarin-0.16, bez změn SDK a hostitele. Omezení hranice SDK jsou přijatá a mohou být řešena později během vývoje.

- Nejprve ověřit všechny vstupní cesty panelu na release base. Na multibyte hranici použít pravidlo repozitáře: platné UTF-8 nejdříve, ACP pouze při neplatném UTF-8; netvrdit bez ověření, že tím mizí každá nejednoznačnost historických dat.
- Použít existující `GetPanelPath` s heapovým bufferem a skutečnou kapacitou; neúspěch ani zkrácení cesty nesmějí být považovány za platný kořen. Wide rozšíření SDK je pro nynější rozsah odložené.
- Interně vlastnit `std::wstring` nebo obdobné heapové buffery. `NameW` použít přímo, pokud existuje. Nezavádět nové path buffery velikosti `MAX_PATH`.
- Na Win32 hranici řešit široká API i rozšířený prefix pro dlouhé lokální/UNC cesty. Otestovat nejen název, ale každou komponentu, UI, protokol, dočasnou kopii a obnovu.

Počáteční podpůrný buffer `SAL_MAX_PATH` sám nezaručuje korektnost celého řetězce. UTF-8 bajtová délka a UTF-16 délka jsou různé veličiny; obě patří do testovací matice.

## Proč nepřevzít existující kopírování bez auditu

| Kandidát | Ověřená vlastnost na 0.16 |
| --- | --- |
| `CSalamanderSafeFileAbstract` | Nabízí uživatelské řešení chyb, ale implementace `SafeFileOpen` odmítá `strlen(fileName) >= MAX_PATH` a volá `CreateFile`; [safefile.cpp, ř. 44–56](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/safefile.cpp#L44-L56). `SafeFileCreate` má další limit na [ř. 383](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/safefile.cpp#L383). |
| `CSalamanderForOperationsAbstract` | Progress a metoda `MoveFiles`, která přesouvá všechny soubory ze zdrojové složky. Není to API pro libovolný potvrzený seznam copy/delete operací; [spl_com.h, ř. 849–897](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_com.h#L849-L897). |
| Salamatrix `CopyInteractive` | Předává `SALCMD_COPY` do existujícího interaktivního příkazu. `UseExistingDialog == FALSE` vede k chybě; `TargetHint` se v tomto předání nepoužije. [salamatrix_commands.h, ř. 206–240](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/salamatrix/salamatrix_commands.h#L206-L240). |

Doporučení je samostatný backend s jasným kontraktem: kontrola zdroje/cíle, řízené kopírování, dočasný soubor na cílovém filesystému, povinné uchování přepisovaných/mazaných souborů podle D4, dokončení a náhrada, přerušení a výsledek každé operace. Rozsah metadat určuje schválené D5; konkrétní Win32 API i provedení obnovy řeší V6. Nelze předem slíbit stejnou atomickou náhradu na všech SMB serverech.

Opětovné přečtení atributů před operací není úplné řešení závodů. Backend musí specifikovat práci s handly, identitou a změnami rodičovských adresářů. Plánovač nesmí zaměnit chybu skenu za nepřítomnost protějšku.

Pro schválené D5 je nutné oddělit přenos vlastností od jejich porovnávání. Dokumentace [CopyFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-copyfileexw) popisuje přenos atributů a ADS i výslovnou volbu dovolující nešifrovaný cíl. Varianta A tuto volbu nepoužívá a EFS operace vynechává. Schopnosti zdroje, původního cíle i složky obnovy se musí ověřit; samotné úspěšné volání kopírovací funkce nestačí k prokázání našeho kontraktu.

[ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew) zachovává mimo jiné DACL, kompresi a některé proudy původního cíle a má podmínky i chybové stavy kolem náhrady a zálohy. Nelze předpokládat, že bez dalšího poskytuje požadovanou kombinaci zdrojových ADS a cílových oprávnění. V6 musí prověřit také náhradu souboru s více hardlinkami bez zásahu do dat/atributů jiných jmen a nezávislost uchované kopie podle D4. Nepodporovanou operaci zablokovat místo tiché změny významu.

## Archivy, FTP/SFTP a cizí pluginové filesystémy

`GetPanelPluginFS` výslovně vrací `NULL`, pokud panel patří jinému pluginu. Stejná hranice platí pro pluginová data listingu: [spl_gen.h, ř. 1497–1507](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L1497-L1507).

Metody `CopyOrMoveFromFS` a `CopyOrMoveFromDiskToFS` jsou callbacky FS pluginu pro hostitele, nikoli obecné veřejné API pro ovládání jiného poskytovatele: [spl_fs.h, ř. 608](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_fs.h#L608), [ř. 659](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_fs.h#L659). Obdobně archivní interface definuje callback vlastníka formátu, například [DeleteFromArchive, spl_arc.h, ř. 105](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_arc.h#L105).

Pro úplnou vzdálenou synchronizaci by bylo nutné sjednat provider kontrakt: enumerace, přesné názvy, úplnost výsledku, metadata a časová přesnost, čtení, zápis, mazání, zrušení, obnova a oznamování chyb. Samotný text cesty z panelu ani současný viditelný listing tyto schopnosti nedokládají.

UNC a mapované SMB jednotky jsou v navržené první verzi proto, že je obsluhuje Windows souborové API. To neposkytuje automatickou podporu FTP, SFTP, archivu ani všech speciálních síťových vlastností.

## Co využít jako vzor

| Existující část | Užitečný vzor a omezení |
| --- | --- |
| Vestavěné Compare Directories | Časy/DST, proudové porovnání a rekurze v [mainwnd5.cpp, ř. 59](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/mainwnd5.cpp#L59), [ř. 282](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/mainwnd5.cpp#L282), [ř. 783](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/mainwnd5.cpp#L783). Nejde o samostatný synchronizační plánovač. Obsahové porovnání povoluje jen pro diskové panely a rekurzi odmítá pro plugin FS ([ř. 1143–1156](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/mainwnd5.cpp#L1143-L1156)). |
| WinSCP | Oddělení checklistu `SynchronizeCollect` a `SynchronizeApply` v [FileSystem.cpp, ř. 1020–1044](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/winscp/salamander/FileSystem.cpp#L1020-L1044). Vlastní terminál a VCL/Borland typy brání prostému použití jako univerzální knihovny. |
| Renamer | Životní cyklus samostatného dialogového vlákna a `ThreadQueue`; [menu.cpp, ř. 19–38](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/renamer/menu.cpp#L19-L38). Nepřebírat bez kontroly staré path operace. |
| Split & Combine | Jednoduchá struktura pluginu, resources, progress a aplikace tmavého schématu; [splitcbn.cpp, ř. 126–133](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/splitcbn/splitcbn.cpp#L126-L133). |
| File Comparator | Možná ruční kontrola dvojice souborů. Externí `fcremote` však používá `char argv[4][MAX_PATH]` a omezený přenos cest ([fcremote.cpp, ř. 191](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/filecomp/fcremote/fcremote.cpp#L191), [ř. 270–274](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/filecomp/fcremote/fcremote.cpp#L270-L274)). Dlouhé Unicode cesty v této integraci nejsou ověřené. |

## Doporučené technické rozdělení

1. **Adaptér SDK na hlavním vlákně:** menu, kopie vstupních dat, lokalizace, profily, hostitelské barvy a notifikace změn.
2. **Scanner ve workeru:** vlastněné wide cesty, enumerace, filtry a stav úplnosti každého podstromu. Každá chyba zůstává explicitní.
3. **Čistý porovnávač a plánovač:** žádné SDK a zápisy; vstupem jsou snapshoty, volby a uživatelské změny akcí.
4. **Executor:** pouze potvrzený neměnný plán; kontrola předpokladů, přenos, obnova, závislosti, přerušení a výsledky.
5. **UI:** zobrazení stavů nezávislé na akcích, filtrování výsledků, souhrn a průběh. Aktualizace přes zprávy s jasným vlastnictvím dat.

Hostitelské `InvokeOnMainThread` je na této base dostupné ([spl_gen.h, ř. 3687–3692](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L3687-L3692)), ale vstupní snapshot na UI vlákně je jednodušší než průběžné dotazování živých panelů. Při zavření dialogu nebo unloadu pluginu je nutné zastavit práci a bezpečně dokončit lifecycle workeru; neponechat callbacky na uvolněný plugin.

Po změnách informovat hostitele přes `PostChangeOnPathNotification` ([spl_gen.h, ř. 2121](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L2121)); evidenci použitého panelu zajišťuje `SetUserWorkedOnPanelPath` ([ř. 2494](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_gen.h#L2494)).

## UI, lokalizace a sestavení

| Integrace | Požadavek a vzor |
| --- | --- |
| Tmavé schéma | `USE_DARKMODELIB=1`, hostitelské `SALCFG_USEWINDOWSDARKMODE`; neodvozovat režim od nastavení OS. [renamer.props, ř. 9](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/renamer/vcxproj/renamer.props#L9), [renamer.cpp, ř. 103](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/renamer/renamer.cpp#L103). Ošetřit titulky, controly, vlastní kreslení a chyby. |
| Lokalizace | Všechny viditelné texty v jazykových resources; `LoadLanguageModule` a `LoadStr`. Dle AGENTS použít `FONT 8, "MS Shell Dlg"`. Počítat s dlouhými překlady a vysokým DPI. |
| Projekty | Pluginový `.vcxproj`, `.def`, resources a jazykový projekt. Shared props generují `.spl` a `lang/english.slg`: [plugin_base.props, ř. 7–11](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/vcxproj/plugin_base.props#L7-L11), [lang_base.props, ř. 7–11](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/vcxproj/lang_base.props#L7-L11). |
| Solution a platformy | Přidat plugin i jazykový projekt do `src/vcxproj/salamand.sln`. PR build má Debug Win32/x64/ARM64: [pr-msbuild.yml, ř. 24–44](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/.github/workflows/pr-msbuild.yml#L24-L44). |
| Installer | Adresáře, `.spl`, `lang/*.slg`, případná nápověda a registrace výběru pluginu; vzor [Inno Setup, ř. 737–738](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/doc/runbook-setup/inno_setup_salamander_x64.iss#L737-L738), [ř. 1897](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/doc/runbook-setup/inno_setup_salamander_x64.iss#L1897). |
| Původ a ABI | Respektovat [AGENTS.md](../AGENTS.md), zachovat licence převzatého kódu a uvést inspiraci v [third_party.md](third_party.md). První verze používá nezměněné SDK Samandarin-0.16. |

## Ověřování a zbývající technické otázky

Nativní testy plánovače mohou používat samostatný projekt `tests/*_tests.vcxproj`; runner je pod `src` automaticky vyhledává ([run_native_tests.ps1, ř. 155–159](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/tools/run_native_tests.ps1#L155-L159)). To dovoluje testovat rozhodnutí bez běžícího Salamanderu. Zápisy vyžadují integrační testy s řízenými chybami v dočasných stromech.

Před vydáním prokázat zejména:

- Přesný Unicode snapshot kořenů a výběru; ASCII, Unicode komponenty, UTF-8 přes 260 bajtů, UTF-16 přes 260 znaků a dlouhé UNC.
- Zastaralý plán, záměnu položky či rodiče za reparse point, odpojení sítě, chyby oprávnění, cancel a plný disk.
- Správné pořadí kopírování/mazání/obnovy; ochranu vyloučených položek a skutečné prázdnosti složek.
- Chování pro shodné/vnořené kořeny a dostupné aliasy. Detekce libovolných síťových aliasů není doložena; stanovit, které identity backend umí porovnat a jak naloží s nejistotou.
- Politiku reparse points včetně kořene a jeho rodičů. Vynechání všech reparse položek se může týkat i cloudových placeholderů, nejen symlinků/junctions; uživatel musí vidět důvod.
- Načtení/uvolnění pluginu, zavření hostitele při práci, obnovu panelů, překlady, tmavé/světlé schéma, klávesnici a DPI.

Samotný předimplementační průzkum tyto runtime vlastnosti nepotvrzuje. Aktuální buildy a smoke testy první etapy bez zápisů uvádí [implementační protokol](directory-sync-plugin-implementation.md); níže uvedená samostatná API sonda není integračním testem zápisového backendu. Publikování a validační draft se řídí postupem v hlavním dokumentu a kořenových instrukcích forku. Úspěch dokumentačního CI s přeskočenou kompilací není důkaz funkčnosti pluginu.

## V5: doplněný audit a navržený kontrakt

Dne 14. října 2026 byla trasována cesta `ChangePathToDiskW → UTF-8 → ChangePathToDisk → SetPath → PathToWideMirror → TreeViewTextToWide` a export `GetPanelPath`. Důležitá korekce: `PathW` není v této trase nezávislým původním Unicode vstupem. `SetPath` jej znovu vytvoří z `char*`; dekodér zkusí UTF-8 a až při neplatném UTF-8 použije ACP ([fileswn1.cpp, ř. 35–63](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/fileswn1.cpp#L35-L63)). To odpovídá pravidlům repozitáře, ale není důkazem jednoznačnosti libovolných historických ACP bajtů. Staticky též nelze zaměnit limit bajtového `SAL_MAX_PATH` za limit počtu UTF-16 jednotek.

**Rozhodnutí uživatele z 15. října 2026:** původní doporučení nového wide exportu se pro nynější rozsah nepřijímá. Vyvíjíme pouze plugin schopný běžet se současnou verzí Samandarin-0.16. SDK, vtable ani uchování cest v hostiteli se nemění. Případná omezení se zdokumentují a budou řešena později během vývoje; nejsou důvodem požadovat rozšíření SDK před zahájením pluginu.

Kontrakt adaptéru pluginu nad stávajícím SDK:

- Kořeny získat přes existující `GetPanelPath` pro fyzickou levou/pravou stranu; přijmout jen Windows diskové panely. Použít heapový buffer `SAL_MAX_PATH` a kontrolovat návratový stav. Neúplný vstup nepoužít pro sken ani zápisy.
- Bajtovou cestu dekódovat podle pravidel repozitáře: platné UTF-8 nejdříve, ACP pouze při neplatném UTF-8. Toto pravidlo neodstraňuje každou historickou nejednoznačnost a případnou dřívější ztrátu dat nelze v pluginu zpětně opravit. Nepoužívat náhradní znaky ani tiché zkracování. V dialogu zobrazit zpracované kořeny a při zjištěné chybě převodu/otevření vysvětlit omezení.
- Kořen a obě sady vybraných jmen zkopírovat v jednom nepřerušovaném callbacku na hlavním vlákně; `NameW` preferovat. Následná enumerace a identity se ověřují samostatně, nikoli podle starého seznamu panelu.
- Interně vlastnit wide cesty a používat široká Win32 API včetně dlouhých prefixů. Omezení získání kořene z panelu nesmí zavést nový obecný limit MAX_PATH do backendu. Přesný rozsah podporovaných vstupů ověřit během vývoje a neúspěšné případy uvést v nápovědě.
- Aktuální `LAST_VERSION_OF_SALAMANDER` je **106**; mechanismus kompatibility popisuje [spl_vers.h, ř. 157–235](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_vers.h#L157-L235). Plugin nesmí vyžadovat nově přidělenou SDK verzi ani API z novějších větví. Načtení a použití ověřit na nezměněném Samandarin-0.16.

**Stav V5:** směr je rozhodnutý, kompatibilita a rozsah omezení se prokážou během vývoje. Matice zahrnuje ASCII, Unicode názvy a adresáře, UTF-8 >260 B při UTF-16 <260 jednotkách, UTF-16 >260 a následně UNC. Test musí zaznamenat buď správné zpracování, nebo explicitně hlášené omezení bez zápisu na chybnou cestu. Nížejší úspěch dlouhé cesty ve WinAPI neprokazuje průchod této cesty přes Salamander. Žádné hlavičky SDK ani zdroje hostitele nebyly změněny.

**Místní runtime doplnění 15. října:** [protokol první etapy](directory-sync-plugin-implementation.md) nyní rozlišuje skutečný SDK snapshot od ručního zadání v pluginu. Unicode/emoji kořeny i kořeny 129/130 UTF-16 jednotek s 269/270 UTF-8 bajty prošly přes oba panely a obsahové porovnání. Kořeny 278/279 UTF-16 jednotek prošly přímým zadáním v náhledu; navigace hostitele do 278jednotkového kořene však vyvolala `0xc0000409` v `ucrtbase.dll`. Kontrolní instance téhož Release hostitele bez SPL zopakovala stejný pád. Nejde tedy o dosažené bezpečné hlášení všech omezení ani o prokázanou podporu těchto kořenů přes SDK; oprava hostitele zůstává mimo současný rozsah. SMB/UNC runtime nebylo použito.

## V6: ověřené API vlastnosti a místní sonda

Místní doplnění z 15. října: read-only engine nyní ukládá identity/ChangeTime a ověřuje snapshot před změnou návrhů a souhrnem. Řízené výměny souborů, rodičů a junction, změny po skenu a přesné hranice důkazu jsou v [protokolu implementace](directory-sync-plugin-implementation.md#ntfs-identity-a-změny-po-skenu-15-října-2026). [Návrh provedení a obnovy](directory-sync-plugin-execution-protocol.md) konkretizuje journal a recovery, ale nepovažuje read-only checkpoint za dostatečnou ochranu budoucího zápisu. Navazující [izolovaná NTFS sonda](directory-sync-plugin-ntfs-protocol-probe.md) prokázala konkrétní rename/sharing režimy, testované ACL/ADS a pádovou matici (347 assertions, 53 procesů v Debug i Release x64). Odhalila ovšem nekompatibilitu striktních rodičovských pinů s rename a možnost změnit ADS přes držený hlavní proud. Executor není implementován. Následným rozhodnutím uživatele byla úplná stabilita všech rodičů/proudů odložena nad rámec cílové úrovně jádra; aktuální rozsah V6 určuje sekce níže. SDK ani hostitel se nemění.

Prostředí: Windows 10.0.26200.0, svazek C: NTFS. Samostatná diagnostická sonda mimo repozitář: `C:\Samandarin\reference-sync-probe-20261014-r2\probe-winapi.ps1`. Volá přímo `CopyFileExW` a `ReplaceFileW` přes P/Invoke; neobsahuje plugin ani synchronizační engine. Úspěšný výstup je v `winapi-e82ce1bf599f46c294089a3483bc6286\result.json` pod stejným testovacím kořenem.

| Sonda | Pozorování | Důsledek pro návrh |
| --- | --- | --- |
| Zdroj NEW s ADS source-only/common; cíl OLD s ADS target-only/common; CopyFileExW do stagingu a ReplaceFileW | Výsledkem NEW; common obsahuje NEW-ADS, source-only se přenesl, **target-only obsahuje OLD-ONLY-ADS** | Prostá náhrada nesplní přesnou sadu zdrojových ADS. Nelze po ní bez dalšího hlásit plnou shodu. |
| Zdrojový mtime 2026-10-01 12:00 UTC | Cíl po náhradě má 2026-10-14 21:45:33.6736337 UTC | Čas aplikovat až po všech datových/ADS operacích a následně ověřit. Tento běh sám neurčuje, který vnitřní krok čas změnil. |
| Cílové SDDL před/po náhradě | Beze změny v tomto prostředí | Nejde o pokrytí odlišných explicitních/inherited ACL ani chyby WRITE_DAC. |
| Původní cíl měl druhý hardlink alias.txt; obnova vznikla skutečnou kopií | Po náhradě alias zůstal OLD. Po zápisu ALIAS-CHANGED do aliasu zůstala obnova OLD a nový cíl NEW | Samostatná kopie chrání obsah před pozdější změnou jiného hardlinku; pouhé přejmenování starého inode by tuto nezávislost nedoložilo. |
| CopyFileExW s prefixem `\\?\`, Unicode adresáři i názvem | Cesta 305 UTF-16 jednotek / 623 UTF-8 B úspěšně přenesla NEW i source-only ADS | Potvrzena tato lokální API cesta, nikoli SDK, UI, všechny délky nebo SMB. |

První pokus sondy skončil chybou 3 kvůli předání nepovinného řetězce z PowerShellu. Opakování použilo C# wrapper s nativním NULL pro zálohu a uspělo. Nejde o závadu pluginu ani důkaz selhání ReplaceFileW se správnými parametry. Oba izolované adresáře zůstaly pro kontrolu; neproběhl automatický úklid.

Dokumentace Microsoftu podporuje nález o zachování chybějících pojmenovaných proudů a DACL při [ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew). Tato funkce vyžaduje stejný svazek a má i chybové stavy se změněnými jmény souborů; příznak WRITE_THROUGH není podporovaný. Nelze z neúspěšného návratu automaticky vyvodit, že cíl zůstal původní. Obnovovací úložiště na jiném svazku proto musí být samostatná ověřená kopie.

[CopyFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-copyfileexw) rozlišuje zrušení s odstraněním částečného cíle a zastavení s jeho ponecháním; skryté nebo read-only existující cíle mohou vyvolat chybu. Jde o důvod kopírovat do vlastního stagingu, nikoli přímo do uživatelského cíle. Chování callbacku při cancel nebylo touto sondou testováno. Záruky dokumentované pro SMB 3 nejsou ověřením libovolného NAS.

## V6: aktuální návrh na úrovni jádra

**Rozhodnutí uživatele z 15. října 2026:** cílem je spolehlivost současného jádra Samandarin-0.16. Dřívější požadavky této sekce na úplné držení rodičů, neměnné ADS, handle-based commit a transakční obnovu byly nahrazeny [aktuálním návrhem provedení a obnovy](directory-sync-plugin-execution-protocol.md). Zjištěné API vlastnosti a výsledky sond výše zůstávají platnou evidencí; nejsou závaznou architekturou executoru.

Zůstávají D4/D5: nezávislá ověřená obnova před přepisem/mazáním, podporovaná data/ADS a metadata, cílová pravidla oprávnění, základní kontrola cest a změn po skenu, běžné chyby a Stop, pravdivý výsledek a ruční obnova. Není slíben atomický snapshot stromu nebo celé sady proudů ani ochrana proti všem souběžným či privilegovaným zásahům. Nepodporované položky a pozorované konflikty se nadále blokují; chyba nesmí být zaměněna za chybějící soubor nebo úspěch.

Referenční worker otevírá zdroj i se share WRITE a vytváří nový cíl přes CREATE_NEW; jde o konkrétní postup v [worker.cpp](../src/worker.cpp), nikoli o univerzální záruku všech cest jádra. [SafeFile SDK](../src/plugins/shared/spl_file.h) je kandidát k posouzení pro ošetřené file I/O, nikoli prokázaná veřejná služba celého kopírovacího workeru. Bez rozšíření SDK nejprve zmapovat dostupné metody, Unicode/dlouhé cesty, threading/UI, ADS a metadata. Interní worker není automaticky volatelný z pluginu.

### Přiměřené ověření při vývoji

- Kopie nového souboru a přepis/mazání až s připravenou obnovou; přesná podporovaná data a metadata podle D5.
- Běžná kolize, nedostatečná oprávnění, obsazený soubor, nedostatek místa, Stop a neúspěšný přenos/Verify; chyba obnovy blokuje destruktivní krok.
- Zjištěná změna po skenu, Unicode a dlouhé cesty, nezávislost obnovy na hardlinku a ochrana nepodporovaných položek.
- SMB ověřit až v samostatné později povolené etapě; současný pokyn jeho testy nepovoluje. SDK a hostitel zůstávají beze změn.

Další souběhové reparse/ACL/ADS sondy, nativní relativní backend, crash replay a důkazy odolnosti vůči výpadku napájení jsou odložené, nikoli blokující předpoklady první verze. Dosavadní experimentální pádová matice se nepřepisuje jako neprovedená, ale nevyžaduje další rozšiřování. Úzké technické mapování jádra/SDK je [dokončeno v samostatném dokumentu](directory-sync-plugin-core-adapter.md), včetně přesných míst workeru, omezení lifetime SDK progressu, kandidátních wide API a běžných testů. Nevznikl executor ani nový runtime důkaz. V6 zůstává otevřená pro běžné integrační ověření budoucí implementace, ne jako projekt certifikace filesystemu.

### Doplnění z dokončených testů předlohy V3

V noci ze 14. na 15. října 2026 byly přes Computer Use dokončeny místní testy přerušení zápisu a skutečné chyby Verify; podrobnosti a nezávislé hashe uvádí [třetí série protokolu](directory-sync-plugin-reference-observations.md). Po zastavení přenosu do prázdného cíle referenční program odstranil rozpracovanou kopii a nezačal další soubor. Po změně posledního bytu již zapsané kopie během pozastaveného Verify zobrazil CRC error; Abort ukončilo běh, ale vadnou kopii ponechalo.

To podporuje požadavky posloupnosti V6 výše: ověření stagingu před commitem, explicitní neúspěch po chybě, zachování obnovy a odlišení dokončeného zápisu od úspěšně ověřené operace. Nejde o runtime důkaz navrženého backendu, přerušení přepisu ani podpory SMB.
