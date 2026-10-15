# Kopie do nových cílů: Stop/Close a chyby po publikaci

15. října 2026. Navazuje na [první etapu kopírování](directory-sync-plugin-copy-new.md). Pouze syntetická místní NTFS data; bez přepisů, mazání uživatelských položek, SMB a změn SDK nebo hostitele.

## Nativní výsledky

Rozšířený `dirsync_copy_tests`: **375 assertions Debug i Release x64**. Počet zahrnuje přípravu a opakované kontroly, nikoli nezávislé scénáře.

| Případ | Výsledek |
| --- | --- |
| Stop po nenulovém, neúplném čtení Verify | Cancelled, cíl nevznikl, vlastní staging uklizen. Doplňuje stávající Stop ve skutečném CopyFileEx callbacku. |
| Po úspěšném MoveFileEx odepřeno FILE_WRITE_ATTRIBUTES na novém cíli | Konečné otevření selhalo s ERROR_ACCESS_DENIED, Failed a published=true, počet úspěšných dokončení 0. |
| Po úspěšném MoveFileEx změněna DACL cíle | Konečná kontrola selhala s ERROR_INVALID_DATA, Failed a published=true. |
| Stop těsně po úspěšném MoveFileEx | Konečná kontrola aktuálního souboru doběhla, Copied; další soubor nezačal, dávka hlásí přerušení. |

Po publikaci zůstala hlavní data PUBLISHED i ADS `:note` = ADS na místě. Zdroj nezměněn, další cíl nevznikl, žádný hlášený ani nalezený staging. Následné výhradní otevření publikovaného cíle prošlo. ACL fixture byla obnovena. Žádné odstraňování publikovaného cíle při konečné chybě ani automatický rollback.

Stávající UI souhrn již používá `published` a lokalizované upozornění, že nový cíl zůstal na místě. Chyba adaptéru vyžadující změnu chování se neprokázala. Doplněna interní fáze Published pro přesný checkpoint; nejde o veřejné SDK. Hook po části Verify i řízené zásahy jsou pouze v testovacích buildech, nikoli v produkčním SPL.

Rozšířený `dirsync_lifecycle_tests`: **429 kontrol Debug i Release x64**, včetně původního skenu/porovnání. Čtyři nové případy používají skutečný Preview, worker a produkční DialogBody: Stop i Close během skutečného copy callbacku po přenesení dat a Stop i Close po přečtení části Verify.

Testovací DLL drží dosažený callback nejvýše 5 s, aby hlavní testovací vlákno neprohrálo závod s malým souborem. Požadavek prochází skutečným Preview::Stop včetně CancelSynchronousIo a potom Join. Všechny čtyři místní Join doběhly pod 3 s: Cancelled pro aktuální položku, NotStarted pro další, bez publikace, zbylého stagingu nebo otevřených zdrojových handlů. Close nastavil closing, Stop nikoli. Následné FreeLibrary prošlo. Nejde o záruku horní meze čekání pro libovolný ovladač/zařízení.

Toto ověřuje integraci handleru, cancel a Join s dialogovým stavem. **Neověřuje celý hostitelský průchod zprávy Done, konečného EndDialog a SDK refresh notifikací.** Testovací exporty a DIRSYNC_COPY_TEST_HOOKS patří jen lifecycle probe projektu.

Produkční sestavení bez testovacích hooků prošlo s warnings as errors: Debug Win32/x64/ARM64 a Release x64. git diff --check prošel.

## Computer Use a limit měření

Soukromý Release hostitel 0.16: `C:\Samandarin\builds\dirsync\run`, vlastní RegFile konfigurace. Fixture `C:\Samandarin\builds\dirsync\copy-stop-close-20261015`: 4 000 souborů po 256 KiB a menší sada 256 souborů po 4 MiB.

První GUI Stop skončil souhrnem **3 945 dokončených, 1 přerušený (error 1223), 54 nezahájených**, s požadavkem nového Compare. Menší pokus doběhl všech 256 souborů před zpracováním Stop, další velký všech 4 000. Tyto dva pokusy nejsou důkazem přerušení/Close během běhu. První běh použil předchozí soukromý copy build; další dva aktuálně sestavený produkční Release plugin a SLG. První výsledek není ověřením identity finální binárky.

Nezávislá kontrola SHA-256 všech vytvořených `.bin` v těchto třech cílech: **0 odlišných obsahů, 0 stagingů**, počty zdrojů zachovány. Evidence `gui-evidence.json` ve fixture. Po ověření a ukončení soukromého hostitele byla pouze nově vytvořená objemná syntetická data této sady uklizena; JSON, diagnostika a záloha konfigurace zůstaly zachovány.

GUI automatizace vykazovala timeouty a opožděné vstupy. Diagnostické snímky hlavního vlákna ukázaly CPU práci v UIAutomationCore/COMCTL32 při procházení poskytovatelů přístupnosti (`uia-stack.txt`); worker přitom dál kopíroval. To ukazuje na souvislost s UI Automation, ale neurčuje přesnou příčinu ani nevylučuje problém přístupnosti seznamu pluginu. **Z těchto GUI pokusů nelze slíbit rychlou odezvu Stop/Close.** Nebyla provedena spekulativní oprava ani vypnutí přístupnosti.

První soukromá instance byla normálně zavřena. Druhá zůstala po dokončené kopii v modálním souhrnu nereagující; po ověření PID/cesty, všech 4 000 cílů a nepřítomnosti stagingu byla ukončena pouze tato testovací instance. Její konfigurace byla obnovena ze zálohy. Původní tři instalované instance (PID 692, 3820, 17148) zůstaly spuštěné. Ukončení po dokončení není test hard kill během kopie ani crash recovery.

## Zbývá

Datová část přerušení, skutečné handlery a Join a zachování cíle při konečné chybě prošly v uvedeném rozsahu. Produkční změny pouze zpřístupňují nové testovací checkpointy; funkční oprava adaptéru nebyla potřebná. Přepisy, mazání a D4 zůstávají mimo tuto etapu.

Dokončit skutečný GUI Close během aktivní kopie a měření odezvy Stop/Close bez rušivého rozsáhlého UI Automation průchodu, včetně návratu do hostitele a refresh notifikací. Přístupnost velkého seznamu vyšetřit samostatně, pokud se prodleva zopakuje při běžném ovládání. Full disk, zařízení, SMB a crash recovery se netestovaly. Výsledky vzdáleného CI pro publikovaný commit eviduje interní draft PR; uvedené počty assertions jsou výsledky místních běhů.