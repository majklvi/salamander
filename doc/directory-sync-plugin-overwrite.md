# Directory Synchronization: přepisy podle D4/D5

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


**Aktuální stav:** implementováno [asymetrické mazání souborů s volitelnou obnovou](directory-sync-plugin-delete.md), 3 380 kontrol adaptéru a 2 343 lifecycle kontrol Debug/Release x64. Mazání složek, SMB testy a změny SDK/hostitele nejsou součástí. Níže zůstává historie předchozích etap.

15. října 2026, `feature/directory-sync-plugin`, výchozí commit `a8c7fca0`. Na pokyn uživatele implementovány **přepisy existujících běžných souborů** nad stávajícím adaptérem. Navazuje na [uchování originálů a zapamatovanou volbu](directory-sync-plugin-recovery.md) a [mapování jádra](directory-sync-plugin-core-adapter.md). Mazání uživatelských položek, SMB testy a změny SDK/hostitele zůstávají mimo rozsah.

## Chování

Nové lokalizované tlačítko **Overwrite files…** pracuje s viditelnými návrhy kopie, u kterých na obou stranách existují podporované soubory. Směr určuje zdroj a přepisovaný cíl; nejde pouze o zvýrazněné řádky. Skip, chybějící cíl, mazání, adresáře, konflikty, blokované a nerozhodnuté položky se nevykonají. Copy new files dál provádí pouze kopie do chybějících cílů a Save originals zůstává samostatným nedestruktivním příkazem.

Po validaci náhledu následuje samostatné potvrzení s počtem přepisů, objemem nových i původních hlavních dat a počtem vynechaných návrhů. ADS jsou další objem. Potvrzení výslovně uvádí **Keeping originals: ON/OFF** a při vypnutí absenci obnovovací kopie původních dat i ADS. Volba a cesta se přebírají ze zapamatovaného nastavení a při spuštění se pro worker zmrazí. Zapnutí/vypnutí během běhu nelze měnit. Cancel nepřenese žádná data a nevytvoří manifest ani staging.

- **ON:** do nové složky běhu se nejprve nezávisle uloží, ověří a zalogují všechny zařazené originály. Až celý obnovovací běh úspěšně skončí a je připraven protokol přepisů, lze začít nahrazovat cíle. Selhání i druhé zálohy tedy zabrání také přepisu prvního souboru. Již dokončená záloha zůstane. Je potřeba místo pro všechny původní soubory, ne pouze pro jeden.
- **OFF:** obnova se vůbec nevytváří a její cesta se nevyžaduje ani neověřuje. Zůstávají staging, Verify, cílová DACL a běžné kontroly. V této první etapě je výsledek bez obnovy pouze v souhrnu UI/paměti, bez trvalého protokolu na disku; nevzniká kvůli němu skrytá složka záloh.

Read-only původní cíl se před kopírováním odmítne chybou 5. Plugin tento atribut neodstraňuje, protože jeho změna by ovlivnila další hardlinková jména. Hidden/System cíle přepsat lze, dovolují-li to běžná oprávnění. První chyba nebo přerušení zastaví další přepisy; nejsou dialogy Retry/Skip jednotlivých chyb. Je potřeba znovu Compare.

## D5 a posloupnost operace

1. Revalidate původního snapshotu, ověření způsobilosti a read-only předpokladu. Při ON dokončení SaveOriginals včetně Verify a manifestu mimo oba stromy; nepoužívají se staré zálohy z předchozího běhu.
2. Zachycení původní **cílové DACL včetně ochrany dědičnosti**, bez přenosu zdrojové DACL/owner/SACL. Ověření původní identity, velikosti, mtime, change time, atributů, počtu hardlinků a podporovaného typu; kontrola rodičů.
3. Nový vlastní CREATE_NEW staging ve stejné cílové složce, CopyFileExW se Stop. Staging dostane původní cílovou DACL, pokud ji už nemá, a zdrojový mtime a Read-only/Hidden/System. Nezachraňujeme chybějící právo WRITE_DAC změnou politiky nebo zvýšením oprávnění.
4. Přesné Verify hlavního proudu a celé sady nových ADS: jména, délky, obsah. Kontrola zdroje, stagingu a jeho požadovaných metadat/DACL.
5. Bezprostředně před publikací znovu běžně ověřit očekávaný cíl i jeho DACL a rodiče. Potom **MoveFileExW s MOVEFILE_REPLACE_EXISTING** na stejném svazku. Žádný COPY_ALLOWED, delete/recreate fallback ani přepis dat starého objektu na místě.
6. Kontrola identity publikovaného souboru, konečného času, atributů a DACL; výsledek. Při ON zápis výsledku a flush protokolu. Poté další položka.

Nový cíl má přesně nové ADS; staré se do něj neslučují. Při ON zůstávají v nezávislé obnově. Jiná hardlinka původního cíle si zachová svá původní data, ADS, mtime a podporované atributy i DACL; pouze počet jmen starého objektu se přirozeně změní. Obnovovací kopie sama dědí práva obnovovací složky a neslibuje automatické obnovení původní DACL. Přenos původní cílové DACL na nový cíl je samostatná povinnost přepisu.

Před publikací se smí uklidit pouze vlastní staging, po kontrole identity; původní cíl se kvůli úklidu nemaže. Po publikaci zůstane náhrada i při chybě konečných metadat, DACL nebo protokolu, se stavem Failed či neúplného běhu a příznakem `published`. Při chybě logu po již ověřené náhradě se zachová známý úspěch položky, ale celý běh není complete a další položka nezačne. **Žádný automatický rollback.**

Stop před publikací ji zabrání. Stop po publikaci se zaznamená podle skutečného dokončení aktuálního souboru a zabrání dalšímu přepisu; celý běh není atomický. Progress počítá skutečné přenesené/ověřené bajty a dokončené přenosy: při ON jsou zálohy i náhrady, tedy až 2× počet cílových souborů. Souhrn zvlášť uvádí dokončené náhrady a uložené/zalogované originály. Po běhu UI invaliduje snapshot, vyžádá Compare a přes stávající SDK oznámí dotčené kořeny a případnou obnovovací cestu. Worker SDK nevolá.

## Jednoduchý protokol při ON

`manifest.jsonl` dál jednoznačně popisuje dokončenou samostatnou fázi `save_originals_only`. Ve stejné nové složce běhu se před prvním přepisem přes CREATE_NEW vytvoří **`overwrite.jsonl`**, UTF-8 bez BOM, každý záznam WriteFile + FlushFileBuffers. Formát 1:

| Událost | Pole |
| --- | --- |
| `run` | `schema: 1`, `operation: "overwrite"`, `keepOriginals: true`, `left`, `right`, `recoveryManifest`. |
| `planned` | `item` od 1, absolutní `source`, `target`, `saved`. Všechny plány se zapíší před prvním přepisem. |
| `result` | `item`, `status`: `replaced`, `failed`, `cancelled`, `not_started`; Windows `error`, `published`, `remainingStage`. |
| `end` | `status`: `complete`, `failed`, `cancelled`. Chybějící/částečný konec není úspěšný dokončený běh. |

Obnovovací manifest může být complete, i když žádný přepis nezačal nebo pozdější přepis selhal. Nezaměňovat dokončení obnovy s dokončením synchronizace. Při selhání přípravy/otevření protokolu přepis nezačne. Při append/flush chybě se běh zastaví a log může být neúplný; nedoplňuje se vymyšlený úspěch. Ruční obnova z `saved` nejprve do nového jména; žádný replay ani automatický přepis při čtení manifestu.

## Ověření na místním NTFS

Celkem **2 663 společných copy/recovery/overwrite assertions Debug i Release x64**, **1 577 lifecycle kontrol Debug i Release x64** po níže uvedeném dokončení ověření, **539 engine assertions Release x64**, **1 213 resource/layout kontrol Debug x64** při 96/120/144/192/240 DPI. Součástí jsou dosavadní regrese nových kopií, obnovy a zapamatování nastavení. Metriky layoutu nejsou runtime přepínání DPI monitoru.

| Běžný případ | Výsledek |
| --- | --- |
| Oba směry, ON/OFF, rozdílné hlavní obsahy a sady ADS | Přesně nová data v cíli, bez starých-only ADS; při ON přesně původní cíle v obnově. Při OFF nepoužita ani úmyslně neplatná cesta obnovy. |
| Chráněná i skutečně nechráněná/děděná cílová DACL, odlišná zdrojová politika | Cílová DACL zachována; nezměněna ochrana dědičnosti. Explicitně ověřeno i při směru zprava doleva. |
| Zdrojový Read-only/Hidden/System a mtime; původní Hidden/System cíl | Náhrada má podporované zdrojové vlastnosti; hardlinka originálu ponechá staré atributy. |
| Původní cíl s další hardlinkou | Další jméno má stará hlavní data, ADS, atributy i původní DACL; uložená obnova je nezávislá. |
| Read-only původní cíl | Odmítnut beze změny atributů, bez zbytečné zálohy či přepisu. |
| Změna po Compare, výhradní zámek cíle, chybějící obnova | Zastaralý plán nebo skutečná Windows chyba 5/32; původní cíl nepřepsán. |
| Stop při záloze, její nedostatek místa po jedné hotové kopii | Žádný přepis; hotová obnova zůstane, neúplná se nepoužije jako oprávnění k náhradě. |
| Stop během skutečného přenosu/čtení Verify nového stagingu | Bez publikace, originál i staré ADS zachovány, vlastní staging uklizen. |
| Pozdní Stop po publikaci prvního cíle | Dokončená náhrada zůstane, další náhrada nezačne. |
| Poškozený staging, změna cílových dat nebo DACL před publish | Žádná náhrada. Pozorovaný rozpor se neignoruje. |
| Skutečně odepřené WRITE_DAC na stagingu s jinou cílovou DACL | Požadované nastavení práv selže chybou 5 před publikací. |
| Řízený disk-full při reálném částečném přenosu a při publish | Chyba 112, originál zachován. Při ON zůstává dříve ověřená obnova. Nejde o nový fyzicky plný VHD. |
| Odepřené závěrečné FILE_WRITE_ATTRIBUTES po publish | Failed s published=true, náhrada zůstane; další soubor se nepřepíše, obnova při ON je dostupná. |
| Řízená chyba zápisu planned / result po publikaci | Před plánem žádný přepis; po publikaci zachován skutečný výsledek, neúplný protokol a zastavená dávka. |
| Viditelné indexy, duplicitní index a mimo rozsah; nové cíle/návrhy mazání | Jen způsobilé předané existující cíle jednou; nový soubor ani návrh mazání se nevykoná. |
| Dlouhé Unicode předky a basename, emoji/české ADS, UTF-16 i UTF-8 přes 260 | Náhrada i obnova bez zkrácení; ověřen i prázdný původní hlavní proud s ADS. |
| Ruční obnova původních dat do nového jména | Původní hlavní obsah a staré ADS vráceny. |
| Skutečná obsluha Stop/Close v DLL při Copy/Verify přepisu, ON/OFF | Všech 8 nativních případů cancel/join prošlo, handly uvolněny, původní cíle zachovány. |

Produkční SPL a SLG sestaveny pro Debug Win32/x64/ARM64 a Release x64; runtime testy pouze x64. SDK i hostitel beze změn. Nové testy jsou součástí existujících projektů, které CI již automaticky objevuje. V této etapě se nic nepushovalo a nové vzdálené CI se nespouštělo.

## Krátká kontrola GUI

Computer Use, soukromý Release Samandarin-0.16, Windows Dark Mode (experimental) v konfiguraci hostitele. Nové tlačítko i obě potvrzení jsou čitelné. Fixture `C:\Samandarin\builds\dirsync-overwrite\gui-20261015`: vlevo `NEW SOURCE`, vpravo `OLD TARGET`, obojí 10 B, vlevo novější čas. Návrh Copy to right; potvrzení ON ukázalo 1 soubor, 10 B nových a 10 B původních dat, správnou Unicode obnovovací cestu a požadavek zálohy před přepisem. Po Alt+K potvrzení OFF výslovně uvedlo absenci obnovy původních dat i ADS. Obě potvrzení byla zrušena přes Cancel; nezávislá kontrola potvrdila nezměněné vstupy a prázdnou obnovovací složku.

Přepisy nebyly v tomto prvním GUI testu potvrzeny. Následující navazující ověření již pokrývá také skutečné dokončení, souhrn a refresh. Soukromý hostitel po prvním testu zavřen, jeho konfigurace obnovena ze zálohy se shodným hashem, uživatelské instalované procesy nezměněny.

Testované GUI binárky SHA-256: SPL `E8A7151DB4999A4BDD88DCF17D3702E6157F0EA4C74705DE0788DE1CFF980A89`, SLG `A7B2A2CCA4E6D3A8BF708F5375BFAFAF626A4AEDF05B99AB7E5F9D02075CE97D`.

## Dokončení GUI, nucené Release a běžné ukončení

15. října 2026, navazující ověření na pokyn uživatele. Stejné produkční Release binárky jako výše, bez testovacích hooků, soukromý hostitel 0.16 a Computer Use. Nová izolovaná NTFS fixture `C:\Samandarin\builds\dirsync-overwrite\gui-complete-20261015`; malé vstupy, obnovy, skutečné cílové SDDL a `results.json` zůstávají mimo repozitář.

| GUI případ | Výsledek |
| --- | --- |
| `a.txt`, uchování ON, 10 → 14 B, rozdílné ADS | Potvrzený přepis, souhrn 1 nahrazený a ověřený soubor, 0 chyb, 1 uložený/zalogovaný originál; oba protokoly complete. Nová hlavní data a pouze nové ADS v cíli; původní data a staré ADS v obnově. Nové Compare: Equal/Skip. |
| `b.txt`, uchování OFF, 18 → 18 B, rozdílný obsah/čas/ADS | Potvrzení i souhrn výslovně uvádějí OFF. Souhrn 1 úspěch, 0 chyb; žádná další obnovovací složka ani manifest. Nová data a ADS v cíli, původní-only ADS chybí. Cílová DACL zachována. Nové Compare: Equal/Skip. |
| `c-refresh.txt`, uchování ON, 3 → 19 B | Samostatný úspěšný přepis nad skutečně zobrazenými panely. Zachována předem ze souboru načtená chráněná cílová DACL; ověřena nová data a ADS. Compare ukázalo všechny tři řádky Equal/Skip. Po zavření náhledu pravý panel automaticky ukázal 19 B a nový čas místo 3 B; nebylo použito F2. |

Kontrola ACL porovnává skutečná ACE a ochranu dědičnosti. První pomocný PowerShell test nesprávně porovnával původní upravený objekt `Get-Acl` před jeho normalizací přes `Set-Acl` a celý SDDL včetně účetního `AI`; hlásil tedy rozdíl i při zachovaných právech. Nejde o prokázanou chybu pluginu. Navazující případ `c-refresh.txt` zaznamenal již skutečně zapsanou DACL před skenem a porovnal binární ACL i ochranu po přepisu. Při přípravě soukromé konfigurace byl rovněž opraven formát testovacího souboru: UTF-8 bez BOM a původní CRLF; nesprávně vytvořený UTF-16 REGEDIT4 hostitel odmítl ještě před testem pluginu.

### Release s aktivním přepisovým workerem

Stávající nativní lifecycle sonda nově provádí dalších **8 scénářů**: ON/OFF × skutečný callback CopyFileEx / skutečné čtení Verify × 0/1 předchozí dokončená náhrada. Při ON jsou již všechny obnovy dokončené před přerušeným přepisem. Testovací hook pro variantu „po jedné náhradě“ byl upraven tak, aby do předchozích náhrad nezapočítával přenosy záloh; jde pouze o opravu testovací sondy.

V každém scénáři `Release(FALSE)` odmítne unload bez přerušení workeru. Skutečný `Plugin::Release(TRUE)` nastaví cancel a joinuje worker pod místním limitem 3 s, report zachová dokončenou náhradu, aktuální položka je Cancelled a další NotStarted. Následuje opakovaný Release, okamžité FreeLibrary a kontrola odmapování DLL. Nezůstane staging; nepřepsané originály mají každý původní bajt hlavního proudu i ADS. Dokončená náhrada má nové bajty a nové ADS, původní-only ADS chybí. Všechny zbývající vstupy, cíle a obnovené kopie lze výhradně otevřít. Obnovovací fáze při ON je complete. Celá sada **1 577 kontrol prošla Debug i Release x64 s warnings as errors**. Počet zahrnuje přípravu a regrese; nejde o 1 577 různých scénářů.

Sonda používá skutečný produkční kód v testovací DLL a zadrží dosažený callback nejvýše 5 s, cancel jej ihned uvolní. Pomocné modeless okno není živý modalní callback hostitele; test nedokládá reentrantní odmapování DLL během takového callbacku. Testovací exporty ani hooky nejsou součástí distribuovaného SPL.

### Ukončení zahájené během přepisu v GUI

`d-large.bin`: zdroj **3 GiB běžných nesparse dat**, původní cíl 21 B plus starý ADS. Uchování ON. Po potvrzení UI ukázalo přenos **2 ze 2, copied 574 619 682 B, Verify read 68 B, complete 1**: záloha již skončila a kopírovala se nová náhrada. První Alt+F4 během aktivní práce přerušilo worker a zavřelo náhled do panelů. Odeslání klávesy a zachycení následujícího snímku trvalo 587 ms; není to izolované měření Join ani garantovaná mez. Druhé Alt+F4 běžně ukončilo hostitele PID 17212. Nebyl použit hard kill.

Nezávislá kontrola po ukončení: původní cíl, jeho starý ADS i DACL nezměněny, ověřená původní data a ADS zůstala v obnově, žádný staging. Obnovovací manifest končí complete; `overwrite.jsonl` končí cancelled a výsledek má published=false. Celý zdroj byl výhradně otevřen a přečten: všech 768 bloků po 4 MiB odpovídalo deterministickému původnímu bloku (SHA-256 uložen v evidenci). Teprve poté byl tento vlastní velký zdroj uklizen. Dříve dokončené malé náhrady zůstaly. Soukromá konfigurace obnovena s přesnou shodou hashe; tři uživatelské instalované procesy zůstaly spuštěné.

Běžné Alt+F4 nejprve patří modalnímu dialogu pluginu; hlavní proces se tedy běžně nezavírá se stále aktivním workerem. Ochrany hostitele popsané v [předchozím ověření kopií](directory-sync-plugin-copy-unload.md) se neobcházely. GUI přerušilo přenos, fázi Verify prokazují nativní testy výše. Produkční oprava nebyla potřeba; změněny pouze testy a dokumentace.

## Meze a další krok

Úroveň běžných operací současného jádra: poslední kontrola očekávaného cíle a MoveFileExW nejsou atomické podmíněné nahrazení konkrétního file ID. Není transakce mezi obnovou a cílem, úplný snapshot všech ADS/DACL proti libovolnému souběhu, automatický rollback ani příslib odolnosti proti ztrátě napájení. Pozorované změny a chyby se nesmějí ignorovat. Zachovává se DACL, nikoli původní vlastnictví/SACL. Read-only cíl zatím neumíme přepsat.

Neověřeno: SMB, další filesystémy, nové přepisy na skutečně zaplněném VHD, hard kill a kritický shutdown během přepisu, reentrantní unload uvnitř živého modalního callbacku a širší permission kombinace. Staré copy-only VHD výsledky se za důkaz přepisových případů nevydávají. Samostatné mazání a odstraňování adresářů není implementované.

Doporučené GUI a lifecycle ověření této etapy je dokončené. Další krok: commit/push a kontrola interního draft CI na žádost uživatele; další funkční etapa může být výslovně schválené asymetrické mazání souborů se stejnou zapamatovanou volbou obnovy. Zatím se neimplementuje.
