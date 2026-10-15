# Directory Synchronization: izolovaná NTFS sonda provedení a obnovy

15. října 2026. Navazuje na [návrh protokolu](directory-sync-plugin-execution-protocol.md). **Plugin zůstává read-only.** Sonda je samostatný EXE bez vazby na engine, SPL, hostitele nebo SDK. Není zařazena do distribuce ani běžného CI; její zápisy se týkají pouze čerstvě vytvořených syntetických dat. SMB se nepoužilo.

## Následné rozhodnutí o rozsahu (15. října 2026)

Tento protokol uchovává výsledky již provedené experimentální sondy. Uživatel následně schválil jako cíl úroveň běžných souborových operací současného jádra, viz [aktuální návrh](directory-sync-plugin-execution-protocol.md). Úplná ochrana rodičů a celé sady ADS při libovolném souběhu, native relativní backend a transakční journal jsou **odložené**, nikoli podmínka zahájení běžného vývoje. D4/D5, základní kontroly a pravdivé ošetření chyb zůstávají platné. Níže uvedené překážky se vztahují k původnímu přísnějšímu protokolu; neznamenají, že běžný plugin musí nejprve získat silnější záruky než jádro.

Navazující rozpracované concurrency rozšíření nebylo sestaveno ani spuštěno; při zúžení rozsahu se odstranily pouze jeho vlastní necommitnuté zdroje a zapojení. Není zde nový výsledek reparse/ACL/ADS souběhových testů. Publikovaná sonda a její reprodukce zůstávají beze změn.

## Výsledek a význam

Debug i Release x64 prošly **347 assertions a 53 pomocnými procesy v každém běhu**, včetně 25 násilných ukončení: 12 hranic přepisu, šest hranic nového cíle, šest hranic mazání a jeden běh pro poškozený journal. Číslo assertions zahrnuje i ověření prostředí a opakované kontroly; nejde o 347 nezávislých scénářů. Lokálně Windows 11 Pro 10.0.26200, NTFS na C:, MSVC 14.51, MSBuild 18.10, x64 Debug/Release, warning level 4 a warnings as errors.

**Protokol ještě není připraven k převzetí do pluginu.** Sonda potvrdila užitečnost nezávislé obnovy, odmítnutí kolize a kontroly skutečného stavu po pádu, ale také vyvrátila předpoklad, že současné read-only handly lze přímo použít pro commit. Navíc hlavní datový proud sám nezamyká ADS. Následující pozitivní výsledky platí pro řízené fixtures s klidnými proudy a rodiči, nikoli pro libovolně souběžně měněný strom.

## Co přesně sonda provádí

Zdroj [protocol_probe.cpp](../src/plugins/dirsync/tests/protocol_probe.cpp), samostatný [projekt](../src/plugins/dirsync/tests/protocol_probe.vcxproj). Spouštěč přijímá jen novou absolutní složku s prefixem `C:\Samandarin\builds\dirsync\protocol-`, ověří pevný místní NTFS a existující nereparsované předky, vytvoří nový list přes CreateDirectoryW a při jeho existenci skončí. Vlastní dlouhé cesty používají prefix `\\?\`. Sonda nic plošně neuklízí; fixtures zůstávají k inspekci. Interní `--worker` je pouze testovací rozhraní s událostí rodičovského procesu, nikoli bezpečný obecný filesystem backend.

Každý případ má oddělený zdroj, cíl a obnovu. Obsahuje hlavní proud, různé staré/nové ADS včetně Unicode jména, rozdílné chráněné DACL zdroje a cíle, známý mtime a základní atributy. Záloha a staging vznikají přes CREATE_NEW. Kontrolují se SHA-256 a přesná množina proudů. Starý cíl se přes držený DELETE handle přesune do karantény a staging na jeho jméno bez nahrazení. Mazání se modeluje přesunem do karantény; její definitivní odstranění se zde neprovádí. Nový cíl nemá zálohu neexistujícího objektu.

Testovací JSONL journal ukládá verzi, sekvenci, stav, staré/staging file ID, délku payloadu a SHA-256; každý úplný záznam flushuje. Jde o zmenšený formát sondy, **nikoli hotový manifest navržený pro produkt**: nemá úplná run/op UUID, root fingerprinty, čas, lokalizaci, zabezpečení či katalog stream hashů a jeho parser není obecný JSON parser. Recovery nikdy nepoužívá cesty z logu k zápisu. Dvakrát pouze čte známé fixtures, ověřuje záznamy a porovnává skutečné identity a obsah. Před/po se kontroluje inventář jmen, identit, velikostí přes hashe proudů, mtime, ChangeTime a atributů. Poslední přístup se neporovnává.

Proces rodiče čeká na pojmenovanou událost přesné hranice a teprve potom volá TerminateProcess. Nepoužívá destruktory workeru k záchraně dat. Každý potomek má omezené čekání; po timeoutu se ukončí a test selže. Nejde o simulaci ztráty napájení.

## Operační handly: zjištěné meze

| Pokus na této konfiguraci | Skutečný výsledek | Důsledek |
| --- | --- | --- |
| SetFileInformationByHandle / FileRenameInfo, relativní jméno a neprázdný RootDirectory | ERROR_INVALID_PARAMETER (87). | Popis API nelze považovat za místní důkaz použitelnosti; tímto voláním se protokol neprokázal. |
| Absolutní jméno z drženého parent handle, striktní parent FILE_LIST_DIRECTORY a share READ | Při průzkumném běhu ERROR_SHARING_VIOLATION (32). | Absolutní cesta není náhrada řešící současně kompatibilitu pinů. Tento průzkumný pokus není samostatná assertion finální matice. |
| NtSetInformationFile / FileRenameInformation (10), relativní jméno, striktní parent share READ | ERROR_SHARING_VIOLATION (32). | Ani nativní relativní rename nepoužije stávající read-only režim rodiče. |
| Nativní relativní rename, parent share READ \| WRITE, bez DELETE sharing | Uspěl; kolize vrací 183. Přejmenování samotného rodiče zůstalo odmítnuto. | Pouze experimentální druhý kandidát. Sonda explicitně používá tuto větev, ne tichý fallback v pluginu. |
| Zápis atributů rodiče během tohoto širšího sdílení | Nový FILE_WRITE_ATTRIBUTES handle i změna Hidden uspěly. | Rodič není neměnný. Nereparsované otevírání, práce vzhledem k handlu, bezpečné vytvoření stagingu a závody s reparse/ACL vyžadují další důkaz. |
| GENERIC_READ \| DELETE na starém cíli, share READ | Konkurenční GENERIC_WRITE i DELETE otevření odmítnuto chybou 32. | Prokázaná ochrana proti těmto běžným otevřením; ne proti všem proudům, oprávněním či privilegovaným zásahům. |
| Existující PAGE_READWRITE mapping, otevřený view i po zavření původního file handle | Pokus o nový stabilní GENERIC_READ/share READ handle odmítnut chybou 32. | Tento konkrétní mapping byl blokován; jiné mapovací režimy nejsou pokryté. |
| Držený hlavní proud GENERIC_READ/share READ a zápis existujícího ADS / vytvoření nového ADS | Obě operace uspěly. | Záloha/staging vyžadují samostatnou ochranu úplné sady proudů; dvojí hash při klidném fixture to nenahrazuje. |
| Read-only starý cíl | DELETE otevření a rename uspěly, záloha zachovala Read-only. | Read-only není autorizační bariéra. Budoucí UX musí vědomě určit ochranu takového přepisu; sonda sama nedává produktové schválení. |

Dokumentaci API poskytují autoři Microsoft: [FILE_RENAME_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info), [NtSetInformationFile](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile), [FILE_RENAME_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information). Nativní volání se v sondě načítá z ntdll; ABI a dostupnost na dalších podporovaných Windows/architekturách nejsou touto místní x64 sondou potvrzené. Žádný vzorový kód nebyl převzat.

## Přerušení a obnova

| Přesná hranice kill při přepisu | Poslední úplný journal | Skutečný stav po ukončení |
| --- | --- | --- |
| VALIDATED nebo polovina zálohy | VALIDATED | Starý cíl na původním místě; neúplná záloha není připravená obnova. |
| BACKUP_READY nebo polovina stagingu | BACKUP_READY | Starý cíl beze změny, kompletní nezávislá záloha; staging může být neúplný. |
| STAGED | STAGED | Starý cíl zachován, staging a obnova připraveny. |
| COMMIT_INTENT před rename | COMMIT_INTENT | Starý cíl zachován. |
| První rename proběhl, ještě bez jeho záznamu | COMMIT_INTENT | Původní cesta chybí, starý ID v karanténě a ověřená obnova dostupná. |
| OLD_RELOCATED | OLD_RELOCATED | Stejná mezera; bez automatického rollbacku. |
| Publikování proběhlo, ještě bez jeho záznamu | OLD_RELOCATED | Na cíli staging ID a nový obsah, starý ID v karanténě. |
| PUBLISHED / VERIFIED / COMPLETE | Odpovídající stav | Nový cíl a stará obnova zachovány; při opakované inspekci nic nepřepisováno. |

Pro nový cíl a mazání se samostatně killovalo po BACKUP_READY, STAGED, COMMIT_INTENT, po publikování před záznamem, po PUBLISHED a COMPLETE. U nového cíle před publikováním cesta chyběla; po něm byl nový ID. U mazání před přesunem zůstal původní cíl a po něm chyběl, se starým ID v karanténě a samostatnou zálohou. Záznam STAGED u mazání neznamená existenci staging souboru.

Doplňující kill po prvním rename se použil k poškození journalu: připojený neúplný konec i přepsaný první bajt byly rozpoznány, recovery dvakrát pouze četla, nic nevracela přes cílové jméno. Stav COMMIT_INTENT se tedy nesmí interpretovat jako „rename ještě neproběhl“. Ani COMPLETE neopravňuje k opakovanému provedení.

Kooperativní Stop se modeloval v polovině kopírování zálohy/stagingu i po jejich připravení: skončil ERROR_CANCELLED (1223), starý cíl se nepřesunul. Injektovaný Stop po prvním rename se odložil až za dokončení právě rozpracované publikace: journal COMPLETE, nový obsah na cíli a výsledek běhu Stop. Nejde o GUI test; integrace Stop s budoucím executorem není implementována.

## Chybové scénáře a zachované vlastnosti

| Scénář | Výsledek |
| --- | --- |
| Injektovaný ERROR_DISK_FULL (112) po polovině zálohy nebo stagingu | Starý cíl včetně ADS zachován, nic nepublikováno. **Není to skutečně plný svazek.** |
| Injektovaná chyba flush (29) zálohy, stagingu nebo journalu před COMMIT_INTENT | Bez destruktivního kroku. Příslušný stav se neprohlásí za dokončený. Nejde o skutečné selhání zařízení při FlushFileBuffers. |
| Injektovaná nedostupnost trvalého journalu po prvním / druhém rename | Vrácena chyba, bez COMPLETE; recovery rozliší skutečný stav podle ID, zachová zálohu a karanténu. |
| Skutečně poškozený hlavní proud či ADS v záloze/stagingu před Verify | ERROR_CRC (23); původní cíl nepřesunut. |
| Skutečně poškozený nový hlavní proud po PUBLISHED | ERROR_CRC; žádný automatický rollback ani úklid zálohy. |
| Cizí cíl vytvořený v mezeře po prvním rename / při novém cíli | Rename selhal 183; cizí obsah FOREIGN zůstal, při přepisu zůstala i stará obnova. |
| Obsazené jméno karantény | První rename selhal 183; původní cíl i cizí karanténa zachovány. |
| Skutečný deny FILE_GENERIC_WRITE v DACL obnovovací složky | ERROR_ACCESS_DENIED (5) při vytvoření journalu, starý cíl nezměněn. DACL se po pokusu v harnessu obnovila. |
| Skutečný existující čtenář cíle bez share DELETE | ERROR_SHARING_VIOLATION (32) před zálohou; původní cíl zachován. |
| Hardlink starého cíle existující už před přepisem | Jiná cesta zachovala původní hlavní data, ADS a testované atributy. Záloha má jiný ID; po následné změně starého aliasu zůstala původní. |
| Ruční obnova zálohy do nového jména | Hlavní data i ADS odpovídala starému cíli, nezávislý ID. Publikovaný cíl zůstal nový; obsazené obnovovací jméno selhalo 80 bez změny cizího obsahu. |
| Přenos metadat | Přepsaný soubor zachoval testovanou původní cílovou DACL, nový odpovídal DACL nového sourozence v cíli; zdrojová DACL se nepřenesla. mtime i kombinace Hidden/System/Read-only prošly. |
| Základní atributy po rename | Průzkumný test bez Archive odhalil jeho doplnění při rename. Opravená sonda nastavuje konečný mtime/atributy přes přesný publikovaný handle po rename a před VERIFIED. Archive zůstává technické produktové pravidlo k dohodě, sonda ho nesynchronizuje samostatně. |
| Prázdná / neprázdná složka | Prázdná odstraněna přes FileDispositionInfo po zavření handlu. Neprázdná vrací 145; dítě zachováno. Totéž při přidání dítěte až po otevření původně prázdné složky. |
| Vytvoření adresáře | Nový adresář má zjistitelný ID; obsazené jméno vrací 183. Bez rekurzivního mazání. |
| Dlouhá Unicode cesta | Celá posloupnost přepisu a obnovovací inspekce prošla ve vícesložkové cestě delší než 260 UTF-16 jednotek, s češtinou, CJK a emoji, včetně Unicode ADS. |

## Reprodukce a lokální evidence

Z kořene repozitáře; každý běh musí dostat **nové** jméno fixtures. Příklad pro Debug (Release obdobně):

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  src/plugins/dirsync/tests/protocol_probe.vcxproj /m:2 /v:minimal `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:OPENSAL_BUILD_DIR=C:\Samandarin\builds\dirsync\ /p:TreatWarningsAsErrors=true
if ($LASTEXITCODE -ne 0) { throw 'build failed' }
& C:\Samandarin\builds\dirsync\tests\Debug_x64\protocol_probe.exe `
  C:\Samandarin\builds\dirsync\protocol-new-unique-run
if ($LASTEXITCODE -ne 0) { throw 'probe failed' }
```

Finální Debug fixtures `C:\Samandarin\builds\dirsync\protocol-20261015-debug-final5`, Release `C:\Samandarin\builds\dirsync\protocol-20261015-release-final3`. Výpisy `C:\Samandarin\builds\dirsync\protocol-debug.log` a `protocol-release.log`, prostředí `protocol-environment.json`. Předchozí r1–r8 a final varianty jsou průzkumné běhy v oddělených složkách, nikoli finální výsledky. Běhy r1–r4 skončily na matici rename nebo mappingu; další neúplné běhy odhalily read-only a Archive předpoklady. Evidence z build adresáře se necommituje.

## Historické meze sondy a odložená ověřování

1. **Úplná stabilita rodičů a proudů při souběhu.** Prokázat bezpečné otevírání/create-new vzhledem k parent handlu a ochranu ADS včetně přidání/odebrání proudu, změn ACL a reparse. Nativní rename s širším sharingem je pouze kandidát; nesmí nahradit bezpečnostní kontroly. Staging se v sondě znovu otevírá a kontroluje ID; toto není hotový důkaz kontinuálního držení všech produkčních objektů.
2. **Reálný nedostatek prostoru a I/O porucha.** Použít izolovaný malý NTFS svazek a řízené prostředí pro skutečné chyby zápisu/flush/journalu. Systémový disk se nezaplňoval. Reálné výpadky napájení, durability rename/directory entries a journalu na jiném svazku nebyly testovány.
3. **Rozsah přístupových práv a metadat.** Testované chráněné DACL dávaly vlastníkovi plná práva. Chybí složitější inherited/deny ACL, nedostatek WRITE_DAC/WRITE_ATTRIBUTES, změna ACL během operace, EFS, sparse/komprese, všechna hardlinková propojení napříč plánem, case-sensitive adresáře a mount/SUBST aliasy. Bez důkazu zůstávají blokované nebo mimo podporu podle D5.
4. **Produktový formát a zotavení.** Plný zabezpečený manifest, nedůvěryhodné poškozené logy, obnovení po restartu aplikace, zobrazení nejistých stavů a ruční obnova/úklid mají teprve vzniknout. Neobnovovat automaticky přes jiný ID. Uživatelsky vyjasnit neatomickou mezeru a chování Read-only; není zde dodatečné schválení destruktivních akcí.

Původně doporučené další reparse/ACL/ADS souběhové sondy se po rozhodnutí uživatele odkládají. Následuje úzké mapování jádra a SDK na D4/D5 a běžné testy podle aktuálního návrhu; native commit kandidát se do pluginu nepřebírá. Žádný zápisový executor, SMB test ani změna SDK nebyly přidány.
