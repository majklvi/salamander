# Synchronizační plugin a technický průzkum SDK 0.16

Doplněk k [návrhu chování a rozsahu](directory-sync-plugin-design.md). Stav k 14. říjnu 2026: pouze průzkum zdrojů, bez implementace, kompilace a runtime testů.

Audit proběhl v `C:\Samandarin\directory-sync-plugin` na commitu `a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449`. Odkazy níže jsou připnuté na tento commit forku `majklvi/salamander`; API z jiných rozpracovaných větví se nepředpokládají.

## Závěr a hranice první verze

Nativní C++ plugin pro místní disky a Windows síťové cesty je proveditelný. Doporučeným základem je rozšíření nabídky pluginů, vlastní datový model porovnání a vlastní backend nad širokými Win32 API. Nejde o potvrzení, že existující SDK již poskytuje bezpečný synchronizační engine.

Před zápisovou částí je nutné uzavřít tři technická rozhodnutí:

1. Jak získat jednoznačnou Unicode kořenovou cestu z panelu: SDK 0.16 nemá `GetPanelPathW`.
2. Jak provádět schválený plán s dlouhými cestami, obnovou a opětovným ověřením identity: `SafeFile` ani interaktivní Copy nejsou hotovým řešením.
3. Zda první vydání zůstane u disků/UNC: přístup k cizímu FTP/SFTP či archivnímu pluginu nemá obecné veřejné provider API.

Tato doporučení odpovídají rozhodnutím D1–D5 a ověřením V5–V6 hlavního návrhu. Nejsou schválením zúženého rozsahu.

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

Možné řešení V5:

- Nejprve ověřit všechny vstupní cesty panelu na release base. Na multibyte hranici použít pravidlo repozitáře: platné UTF-8 nejdříve, ACP pouze při neplatném UTF-8; netvrdit bez ověření, že tím mizí každá nejednoznačnost historických dat.
- Pokud je pro záruku potřeba wide snapshot, navrhnout minimální doplněk SDK. Novou virtuální metodu lze pouze připojit na konec rozhraní a odpovídající implementace. Starý vtable prefix musí zůstat beze změny; změna vyžaduje kontrolu ABI proti releasu.
- Interně vlastnit `std::wstring` nebo obdobné heapové buffery. `NameW` použít přímo, pokud existuje. Nezavádět nové path buffery velikosti `MAX_PATH`.
- Na Win32 hranici řešit široká API i rozšířený prefix pro dlouhé lokální/UNC cesty. Otestovat nejen název, ale každou komponentu, UI, protokol, dočasnou kopii a obnovu.

Počáteční podpůrný buffer `SAL_MAX_PATH` sám nezaručuje korektnost celého řetězce. UTF-8 bajtová délka a UTF-16 délka jsou různé veličiny; obě patří do testovací matice.

## Proč nepřevzít existující kopírování bez auditu

| Kandidát | Ověřená vlastnost na 0.16 |
| --- | --- |
| `CSalamanderSafeFileAbstract` | Nabízí uživatelské řešení chyb, ale implementace `SafeFileOpen` odmítá `strlen(fileName) >= MAX_PATH` a volá `CreateFile`; [safefile.cpp, ř. 44–56](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/safefile.cpp#L44-L56). `SafeFileCreate` má další limit na [ř. 383](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/safefile.cpp#L383). |
| `CSalamanderForOperationsAbstract` | Progress a metoda `MoveFiles`, která přesouvá všechny soubory ze zdrojové složky. Není to API pro libovolný potvrzený seznam copy/delete operací; [spl_com.h, ř. 849–897](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/shared/spl_com.h#L849-L897). |
| Salamatrix `CopyInteractive` | Předává `SALCMD_COPY` do existujícího interaktivního příkazu. `UseExistingDialog == FALSE` vede k chybě; `TargetHint` se v tomto předání nepoužije. [salamatrix_commands.h, ř. 206–240](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/src/plugins/salamatrix/salamatrix_commands.h#L206-L240). |

Doporučení je samostatný backend s jasným kontraktem: kontrola zdroje/cíle, řízené kopírování, dočasný soubor na cílovém filesystému, dokončení a náhrada, případná obnova, přerušení a výsledek každé operace. Konkrétní Win32 API a metadata jsou předmětem V6; nelze předem slíbit stejnou atomickou náhradu na všech SMB serverech.

Opětovné přečtení atributů před operací není úplné řešení závodů. Backend musí specifikovat práci s handly, identitou a změnami rodičovských adresářů. Plánovač nesmí zaměnit chybu skenu za nepřítomnost protějšku.

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
| Původ a ABI | Respektovat [AGENTS.md](../AGENTS.md), zachovat licence převzatého kódu a uvést inspiraci v [third_party.md](third_party.md). SDK rozšiřovat pouze s ověřením binární kompatibility. |

## Ověřování a zbývající technické otázky

Nativní testy plánovače mohou používat samostatný projekt `tests/*_tests.vcxproj`; runner je pod `src` automaticky vyhledává ([run_native_tests.ps1, ř. 155–159](https://github.com/majklvi/salamander/blob/a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449/tools/run_native_tests.ps1#L155-L159)). To dovoluje testovat rozhodnutí bez běžícího Salamanderu. Zápisy vyžadují integrační testy s řízenými chybami v dočasných stromech.

Před vydáním prokázat zejména:

- Přesný Unicode snapshot kořenů a výběru; ASCII, Unicode komponenty, UTF-8 přes 260 bajtů, UTF-16 přes 260 znaků a dlouhé UNC.
- Zastaralý plán, záměnu položky či rodiče za reparse point, odpojení sítě, chyby oprávnění, cancel a plný disk.
- Správné pořadí kopírování/mazání/obnovy; ochranu vyloučených položek a skutečné prázdnosti složek.
- Chování pro shodné/vnořené kořeny a dostupné aliasy. Detekce libovolných síťových aliasů není doložena; stanovit, které identity backend umí porovnat a jak naloží s nejistotou.
- Politiku reparse points včetně kořene a jeho rodičů. Vynechání všech reparse položek se může týkat i cloudových placeholderů, nejen symlinků/junctions; uživatel musí vidět důvod.
- Načtení/uvolnění pluginu, zavření hostitele při práci, obnovu panelů, překlady, tmavé/světlé schéma, klávesnici a DPI.

Aktuální průzkum tyto runtime vlastnosti nepotvrzuje. Neproběhl build ani testovací běh; jde o vstupy pro další návrh. Publikování a validační draft se řídí postupem v hlavním dokumentu a kořenových instrukcích forku. Úspěch dokumentačního CI s přeskočenou kompilací není důkaz funkčnosti pluginu.
