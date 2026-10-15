# Directory Synchronization: kopie pouze do nových cílů

Navazující [došetření UI Automation a dokončené GUI Stop/Close](directory-sync-plugin-ui-automation.md) uzavírá běžný GUI případ na aktuální binárce; velké UIA průchody zůstávají odděleným známým omezením.

Navazující ověření [Stop/Close a chyb po publikaci](directory-sync-plugin-copy-cancellation.md): 375 copy assertions a 429 lifecycle kontrol Debug/Release x64. Nový záznam odděluje úspěšné nativní testy od omezeného GUI měření.

15. října 2026. Na výslovný pokyn uživatele implementována první zápisová etapa podle [mapování adaptéru](directory-sync-plugin-core-adapter.md). Podporuje pouze kopie souborů s chybějícím protějškem, v obou směrech. **Neprovádí přepis existujících cílů ani mazání uživatelských položek.** Obnova D4 ještě není implementována, protože tato etapa nemění původní cílové soubory. SDK, hostitel i veřejná rozhraní zůstávají beze změn; SMB se netestovalo.

## Chování a rozsah

V dialogu je nové lokalizované tlačítko **Copy new files...**. Nejprve proběhne stávající validace náhledu. Samostatné potvrzení uvádí počet způsobilých viditelných souborů, velikost hlavních dat a počet vynechaných ostatních návrhů. Kopírují se viditelné zařazené akce, nikoli jen zvýrazněné řádky. Existující cíle, návrhy mazání, konflikty, nerozhodnuté a samostatné adresářové akce se nevykonají. Pro soubory lze vytvořit potřebné chybějící rodičovské složky; oba zvolené kořeny musejí existovat.

Worker před zápisem znovu ověří zmrazený náhled proti původním kořenům z jeho snapshotu. Po každém běhu je starý náhled zneplatněn; další akce vyžaduje Compare. Zjištěná změna plánu před zahájením nic nekopíruje. Po první chybě nebo Stop se další soubory nezačínají. Již dokončené nové soubory zůstávají na místě. Žádný automatický rollback, Retry přes starý plán nebo transakce stromu.

Průběh zobrazuje pořadí souboru, přenesené bajty včetně ADS, součet bajtů přečtených při Verify z obou stran a počet dokončených souborů. Stop používá stejný cancel/join životní cyklus jako sken; CopyFileExW dostává z callbacku PROGRESS_CANCEL. Zavření během práce požádá o zastavení a vyčká na dokončení workeru. Přijde-li Stop až po publikaci aktuálního souboru, neodstraňuje jej. Souhrn rozlišuje dokončené, chybové/zastavené a nezahájené položky, Win32 chybu, případné zveřejnění před chybou konečné kontroly a zbylý staging. Výsledky jsou zatím v paměti/UI; trvalý protokol a obnova patří do následující etapy D4.

## Privátní adaptér a D5

[copy.cpp](../src/plugins/dirsync/copy.cpp) a [copy.h](../src/plugins/dirsync/copy.h) nemají závislost na SDK. Používají existující wide path helpery a obyčejné Win32 API. Kořeny získávají z kontrolovaného interního snapshotu, nikoli z nových neověřených parametrů. `SnapshotRoots` je interní funkce porovnávacího enginu, nikoli změna pluginového SDK.

1. Před položkou zkontrolovat relativní cestu, rodiče bez následování jejich reparse pointů, identitu/metadata zdroje a nepřítomnost cíle. Kontrola rodičů je běžný checkpoint, ne kontinuální zámek proti všem závodům. Zdrojový handle má share READ | WRITE podle běžného postupu jádra.
2. V cílovém rodiči vytvořit vlastní `.dirsync-{GUID}.tmp` přes CREATE_NEW. Získat jeho ID a zděděnou DACL. Jde zároveň o rezervaci stagingu a jednoduchý referenční vzor cílových práv; není potřeba další pomocný soubor či native backend.
3. CopyFileExW přenáší do tohoto vlastního rezervovaného stagingu, **nikdy do uživatelského cíle**. Proto zde nemá FAIL_IF_EXISTS: již existující soubor je naše rezervace. Jeho identita se znovu kontroluje. Toto technické přepsání vlastního stagingu nepovoluje přepis existujícího uživatelského cíle. Úklid i případné pozdější kolize vlastní staging identity kontrolují.
4. Obnovit zděděnou cílovou DACL ze stagingové rezervace, nastavit mtime a Read-only/Hidden/System podle zdroje. Archive se nastavuje jako u běžné změněné kopie a samostatně se neporovnává. Nekopírovat vlastníkem řízenou zdrojovou DACL jako přístupovou politiku cíle. Chyba požadovaných práv/metadat znamená nepublikovat.
5. Vyjmenovat přesnou sadu názvů/délek hlavního proudu a ADS na obou stranách, proudově porovnat všechny bajty. Ověřit mtime, vybrané atributy a přístupové záznamy/ochranu DACL; ověřit nezměnění zdroje proti náhledu. Samotný evidenční příznak SE_DACL_AUTO_INHERITED není rozdíl přístupových práv. Podporované metadata jsou D5, nikoli úplný klon filesystému.
6. Těsně před dokončením znovu ověřit staging, základní stav rodičů a nepřítomnost cíle. Publikovat přes MoveFileExW s příznaky **0**: žádné nahrazení, copy přes jiný svazek ani odložené operace. Nově vzniklá kolize selže. Potom zkontrolovat stejný publikovaný ID, konečný čas/atributy a cílovou DACL. Data již byla ověřena před přejmenováním stejného objektu.
7. Při chybě před publikací odstranit jen vlastní staging po ověření jeho ID; jiný objekt se nemaže. Při problému s úklidem uvést cestu k ruční inspekci. Po publikaci se nový cíl při chybě **neodstraňuje**; výsledek ji výslovně hlásí. Vytvořené prázdné rodiče mohou po Stop/chybě zůstat, což potvrzení předem uvádí.

EFS, reparse/cloud/offline položky zůstávají blokované. Nejsou přidané přesné souběhové záruky pro ADS/ACL, oplocky, nativní relativní API, kryptografický journal ani infrastruktura pro replay po pádu. Konkrétní kombinace CopyFileExW a běžných práv/metadat je nyní lokálně otestovaná níže; výsledky původní native sondy se za důkaz tohoto adaptéru nevydávají.

## Provedené ověření

Nový [dirsync_copy_tests](../src/plugins/dirsync/tests/dirsync_copy_tests.vcxproj) běží ve standardním native runneru jako projekt `*_tests.vcxproj`. Na místních syntetických fixtures prošlo **292 assertions v Debug i Release x64**. Počet zahrnuje přípravu a opakované kontroly, nikoli 292 nezávislých scénářů.

| Případ | Pozorovaný výsledek |
| --- | --- |
| Nový nenulový/prázdný soubor, oba směry | Správný obsah, samostatný ID, hlavní data i Unicode ADS přeneseny. |
| Odlišná chráněná zdrojová DACL, dědění v cíli | Nový soubor odpovídá právům nového sourozence v cílové složce, nikoli zdrojové DACL. Původně zavádějící porovnání SDDL rozlišovalo pouze D: versus D:AI; kontrola nyní odlišuje podstatná přístupová/protection data od evidenčního auto-inherited příznaku. |
| mtime a kombinace Read-only/Hidden/System | Zachovány i po publikaci. |
| Hardlinkovaný zdroj | Po změně jeho dalšího jména zůstala nová cílová kopie původní, nezávislá. |
| Existující cíle a návrh mazání v asymetrii | Nevykonány, obsah existujících souborů nezměněn, vynechané návrhy započítány. |
| Jen zmrazený podvýběr viditelných řádků | Ostatní soubor nevznikl. Opakování starého plánu po kopii je odmítnuto jako zastaralé. |
| Cizí cíl vzniklý těsně před publish | Kolize selhala a obsah FOREIGN se nepřepsal. |
| Chybný hlavní proud či přidané ADS ve stagingu, poškozená DACL, změna stagingu po Verify | Rozpoznáno, staging uklizen, nic se nepublikovalo; další soubor nezačal. Injekce probíhá skutečnou změnou syntetického souboru v testovacím hooku. |
| Změna zdroje / cíle po skenu | Změna před během odmítla plán; změna zdroje před Verify způsobila neúspěch, bez publikace. |
| Stop během skutečného CopyFileEx callbacku, při Verify a před publish | Bez publikace rozpracované položky a bez další kopie; vlastní staging nezůstal. |
| Stop po dokončeném prvním souboru | První nový soubor zachován, druhý nevznikl. |
| Skutečné deny FILE_ADD_FILE na cílové složce | ERROR_ACCESS_DENIED, bez publikace. DACL byla ve fixture nastavena před skenem a po pokusu obnovena. |
| Dlouhé cesty a názvy | Potřebné Unicode rodiče vytvořeny; cesta >260 UTF-16, dlouhý název souboru, emoji/CJK/čeština a ADS prošly. Samostatně prošla složka <260 UTF-16 s UTF-8 délkou >260 B. |

Regrese: porovnávací engine **539 assertions Debug/Release x64**, stávající lifecycle skenu/obsahového porovnání a immediate FreeLibrary **347 kontrol Debug/Release x64**, UI prostředky **975 kontrol Debug/Release x64** při fontových metrikách 96/120/144/192/240 DPI. Tyto lifecycle testy nejsou novým důkazem force unload během kopie nebo aktivního GUI callbacku. Produkční plugin/jazykový modul sestaven Debug Win32/x64/ARM64 a Release x64 s warnings as errors. Podpis Release probe DLL byl stávajícím post-build pravidlem přeskočen; není to podepsaná distribuce ani výsledek vzdáleného CI.

### Skutečné GUI v izolovaném Release hostiteli 0.16

Použit Computer Use a vlastní RegFile konfigurace v `C:\Samandarin\builds\dirsync\run`, fixture `C:\Samandarin\builds\dirsync\copy-ui-20261015`. Ověřený tmavý dialog nabízí nové tlačítko a samostatné potvrzení pouze jednoho nového souboru 8 B, se započítáním jedné ostatní vynechané položky. Cancel prvního potvrzení nevytvořil cíl. Při dalším potvrzení vznikl `right\new.txt` s obsahem NEW-DATA a ADS `:note` = NEW-ADS; `right\existing.txt` zůstal KEEP-ORIGINAL, zdroj zůstal NEW-DATA a nebyl zbylý staging. Souhrn: 1 dokončený, 0 chyb, 0 nezahájených, 1 vynechaný; následný status požadoval nové Compare. Evidence `C:\Samandarin\builds\dirsync\copy-ui-result.json`.

Hostitel byl ukončen a jeho soukromá konfigurace vrácena do původního stavu. Počáteční app launcher vybral dodatečnou instalovanou instanci; ta byla zavřena bez provádění testů, poté se testovalo pouze v procesu s přesnou cestou soukromé distribuce. Původní tři instalované instance zůstaly spuštěné. Automatizace některé bezprostřední modal refresh vracela opožděně; rozhodnutí se opírala o obnovené snímky a nezávislou filesystem kontrolu. Finální wording potvrzení byl následně jazykově zjednodušen, chování se neměnilo.

## Meze a další krok

Runtime ověření je **místní NTFS x64**. SMB, další filesystémy a širší varianty oprávnění nebyly testované; neúspěšná enumerace proudů nebo získání/nastavení požadovaných práv operaci zablokuje. Nelze slibovat úplnou podporu všech místních/USB filesystémů z NTFS výsledku. Reálný plný disk, porucha zařízení, hard kill během kopie a crash recovery nejsou touto etapou ověřené. Běžné Stop/Verify/ACL/kolize byly ověřeny v uvedeném rozsahu, nikoli nové pokročilé race kampaně.

Není implementována obnova D4, přepis, uživatelské mazání, samostatné kopírování prázdných adresářů, per-item Retry/Skip dialog nebo trvalý protokol. První chyba/Stop ukončí aktuální batch s pravdivým souhrnem; nové porovnání je nutné. Úklid pouze vlastní nepublikované staging kopie není operací mazání navrženou synchronizačním plánem. V této etapě nevznikla změna SDK ani hostitele.

Další přiměřený krok: běžné ověření Stop/Close a chyb po publikaci v této copy-only etapě, poté samostatně navrhnout/implementovat SaveOriginal a běžné D4 testy. Přepisy a mazání do té doby nezapínat. Aktuální změny zatím nejsou publikované; vzdálené CI se vyhodnotí až při požadovaném commitu/pushi.
