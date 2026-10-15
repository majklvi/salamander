# Directory Synchronization: mapování jádra a nejmenšího adaptéru

15. října 2026, větev `feature/directory-sync-plugin`, výchozí stav `10a3a1fd`. Jde o čtení zdrojů a návrh, **nikoli implementaci executoru ani nové runtime ověření**. Platí schválená [úroveň současného jádra](directory-sync-plugin-execution-protocol.md) a D4/D5 z [hlavního návrhu](directory-sync-plugin-design.md). SDK, hostitel i read-only plugin zůstávají beze změn; SMB nebylo testováno.

## Závěr

Použít malý privátní adaptér pluginu nad wide Win32 API. SDK 0.16 neposkytuje službu pro vykonání našeho potvrzeného seznamu kopií/přepisů/mazání. Nekopírovat celý worker, nepřipojovat interní třídy hostitele a nebudovat native/transakční backend. Převzít běžné chování jádra: vytvoření nového souboru, průběh, Stop, explicitní chyby a konečné nastavení metadat. Nezávislá obnova je doplněk D4; cílová DACL a nezměnění ostatních hardlinků jsou závazek D5.

Doporučené minimum: `CopyFileExW` pro přenos do nového vlastního souboru a obnovy, `MoveFileExW` pro publikování na stejném svazku, `DeleteFileW` pro soubor až po obnově a `CreateDirectoryW`/`RemoveDirectoryW` pro potřebné/prázdné adresáře. K tomu malá obsluha podporovaných metadat, běžné ověření a jednoduchý protokol. Je to **kandidát k běžným implementačním testům**, ne tvrzení o již ověřeném kombinovaném chování těchto API v pluginu. Uvedené runtime testy mají ověřit hlavně D5; pokud přenosová funkce přenese nepovolené bezpečnostní vlastnosti, nelze to ignorovat ani tiše oslabit D5.

## Konkrétní mapování

Odkazy níže vedou na zdrojové soubory této větve. Čísla řádků jsou orientační pro uvedený výchozí commit; funkce slouží jako stabilnější identifikátor.

| Potřeba | Existující postup / kontrakt | Použitelnost v pluginu a doporučení |
| --- | --- | --- |
| Nová kopie | `DoCopyFile`, [worker.cpp](../src/worker.cpp), ř. 5615, 5745–5778: zdroj GENERIC_READ se share READ \| WRITE; nový cíl CREATE_NEW. Vlastní synchronní/asynchronní přenos a progress. | Funkce je interní a závislá na `COperation`, `COperations`, dialogových datech a společném bufferu; není SDK služba. Použít stejnou běžnou úroveň sdílení/chyb, nikoli port workeru. Pro minimální přenos preferovat CopyFileExW s callbackem a FAIL_IF_EXISTS do vlastního stagingu. |
| ADS | `DoCopyADS`, [worker.cpp](../src/worker.cpp), ř. 3525–4115: výčet a samostatné kopie proudů, chyby a Stop; při nedokončení odstraňuje rozpracované vlastní proudy. | Nepřebírat staré ACP převody a buffery 2×MAX_PATH. CopyFileExW je kandidát pro společný přenos dat/ADS; pro Verify a hlášení potřebujeme malý wide výčet FindFirstStreamW/FindNextStreamW rozlišující konec od chyby. Nezamykat celou sadu ADS. |
| Přepis | `DoCopyFile`, [worker.cpp](../src/worker.cpp), ř. 6498–6635: větev s odstraněním/recreate i přepis na místě přes CREATE_ALWAYS, odstranění starých ADS, kontrola některých autorizovaných identit. | Nepřebírat přepis na místě: u hardlinku by měnil další jména, což D5 nepovoluje. Návrh: nezávislá obnova → nový staging s požadovanými cílovými právy → kontrola předpokladů → MoveFileExW se REPLACE_EXISTING. Žádný fallback smazat cíl a znovu kopírovat. |
| Oprávnění | `DoCopySecurity`, [worker.cpp](../src/worker.cpp), ř. 2664; volání při `script->CopySecurity`, ř. 6243. Kopíruje i owner/group a pracuje s ochranou dědičnosti DACL. | Celou funkci nepřebírat: D5 nechce zdrojové vlastnictví/audit/oprávnění. Nový cíl má dědit z cílové složky; staging leží u cíle. Při přepisu zachytit a nastavit původní cílovou DACL včetně ochrany dědičnosti, bez owner/group/SACL. Zjistitelná chyba zachování = neprovést přepis. |
| Čas a atributy | [worker.cpp](../src/worker.cpp), ř. 6080–6205: ADS před SetFileTime, následně atributy a kontrola; `DoMoveFile`, ř. 6926, řeší Archive po rename. | Malé wide helpery pro mtime a Read-only/Hidden/System, kontrola po publikaci. Nenastavovat zdrojový creation/access time ani samostatně nesynchronizovat atributy. Archive doporučeno jako běžný příznak změněné kopie; není samostatným porovnávacím kritériem D5. |
| Mazání souboru | `DoDeleteFile`, [worker.cpp](../src/worker.cpp), ř. 7447–7575: kontroly/confirm pro skryté/systémové, práce s Read-only, DeleteFile nebo Koš, chyby. | D4 používá vlastní obnovu, nikoli Koš. Po ověřené záloze a aktuální kontrole DeleteFileW. Nepřebírat shell mazání ani automatické clearing Read-only na souboru s dalšími hardlinkami. První adaptér smí Read-only cíl odmítnout s důvodem; nesmí upravit atributy ostatních jmen. |
| Prázdné adresáře | `DoCreateDir`, ř. 7806; `DoDeleteDir`, ř. 8157 v [worker.cpp](../src/worker.cpp): samostatné operace, chyby a skutečná prázdnost pro příslušné větve. | CreateDirectoryW pouze potřebných rodičů, při existenci ověřit typ. RemoveDirectoryW pouze skutečně prázdného zařazeného adresáře; žádná rekurzivní destrukce. Pro D4 uložit reprezentaci prázdné složky a podporovaných atributů. Adresář s ADS se do odstranění nezařadí, dokud jeho podporovaná data neumí obnova zachovat. |
| I/O s chybovými dialogy | [spl_file.h](../src/plugins/shared/spl_file.h), `CSalamanderSafeFileAbstract`, ř. 26, 167, 312; [safefile.cpp](../src/safefile.cpp), ř. 44–56: limit MAX_PATH, bajtové jméno. SafeFileRead/Write mohou při retry znovu otevřít handle. | Nevhodné jako obecný transport pro již podporované Unicode/dlouhé cesty; nejde jen o velikost bufferu volajícího. Vzor Retry/Skip/Cancel ano, přímé univerzální použití ne. Žádná úprava SDK. |
| Operace SDK a progress | [spl_com.h](../src/plugins/shared/spl_com.h), ř. 843–897: interface žije jen uvnitř callbacku, progress pumpuje zprávy; MoveFiles přesouvá všechny soubory složky. [spl_menu.h](../src/plugins/shared/spl_menu.h), ř. 84–97: parametr může být NULL. | Nelze předat pointer jinému threadu ani jej použít po návratu callbacku. Modální preview uvnitř callbacku samo tento lifetime neporušuje, ale interface nesmí obsluhovat jeho worker. MoveFiles není copy/delete checklist API. Zachovat vlastní již existující UI/worker model a posílat progress do UI. |
| Oznámení změn | [spl_gen.h](../src/plugins/shared/spl_gen.h), ř. 2110–2121: PostChangeOnPathNotification, bajtová cesta, volatelné z libovolného threadu; [spl_menu.h](../src/plugins/shared/spl_menu.h), ř. 88–95. | Po skutečných změnách oznámit dotčené kořeny přes existující SDK; preferovat UI část pro jednotný životní cyklus. Zkontrolovat kódování na hranici SDK, žádné ACP ztráty. Notifikace není potvrzením úspěchu operace. |

Staré ANSI/bufferové a EFS/síťové workaroundy jádra nejsou předpisem k portu. Úroveň spolehlivosti jádra neznamená zavést do pluginu již známé limity MAX_PATH nebo vypustit D4/D5.

## Nejmenší privátní adaptér

Navržené rozdělení je orientační; žádné nové veřejné SDK rozhraní ani framework.

| Část | Odpovědnost |
| --- | --- |
| Kontrola položky | Převzít vlastněné wide cesty a pouze potvrzenou viditelnou akci. Použít současný Revalidate před během, před jednotlivou akcí běžnou kontrolu identity/metadat/typu/přítomnosti; odmítnout pozorovaný rozpor a nepodporované položky. Revalidate není opakovaný plný sken pro každý soubor. |
| `CopyNew` | Jediná společná přenosová primitiva pro staging i obnovu: nový kolizně bezpečný název, CopyFileExW + callback, Stop přes návrat callbacku, skutečný Win32 error. Bez restartovatelných kopií a bez volby dešifrovat EFS. Neúplnou kopii nikdy neoznačit jako hotovou obnovu. |
| `SaveOriginal` | Před přepisem/mazáním použít CopyNew do `<pair>/<run>/<side>/<operation>/`, uložit původní relativní cestu a podporovaná metadata, ověřit hlavní obsah i podporované ADS čtením a přesnou sadou jmen/délek. Název není jediný důkaz vlastnictví při pozdějším úklidu. Obnova nezávislá na cílovém ID/hardlinku; bez automatického purge. |
| Metadata a Verify | Malé helpery pro výčet/porovnání proudů, mtime, základní atributy a cílovou DACL. CopyFileExW není automatický důkaz D5: po přenosu ověřit očekávaná práva/zděděné vlastnosti a nepřenést zdrojovou bezpečnostní politiku. Pokud by bylo nutné problematické API obcházet složitou bezpečnostní vrstvou, zvolit pro přenos jednoduché wide ReadFile/WriteFile se stejným rozsahem, nikoli rozšířit podporu bezpečnostních vlastností. |
| `Publish` / `DeleteSaved` | Nový cíl: MoveFileExW bez nahrazení. Přepis: stejný svazek, připravená cílová DACL, MoveFileExW s REPLACE_EXISTING; bez COPY_ALLOWED a bez DELAY_UNTIL_REBOOT. Mazání: DeleteFileW až s platným výsledkem SaveOriginal. Read-only/ACL/sharing chyba se explicitně vrátí. |
| Výsledek běhu | Malý záznam plánované/skutečné akce, stavu, chyby, původní a obnovovací cesty. UI si vyžádá Retry/Skip/Stop podle chyby; retry znovu kontroluje předpoklady, neprovádí slepě commit. Worker sám nevolá modal SDK dialogy. Po skončení UI shrne výsledky, invaliduje starý náhled a odešle notifikace změn. |

Pro uložení mtime/atributů a čtení DACL používat wide/handle varianty dostupné na cílovém Windows; pokud konkrétní práva pro zachování cílové DACL chybí, operaci odmítnout. Není třeba získávat privilegia pro owner/SACL. Složitá serializace celého security descriptoru do recovery journalu není podmínkou D4, která neslibuje automatické obnovení původních oprávnění.

## Běžná posloupnost a její meze

**Nový soubor:** zkontrolovat plán a kořeny → CopyNew do stagingu u cíle → ověřit požadované vlastnosti včetně cílového dědění → znovu ověřit nepřítomnost → publikovat bez nahrazení → konečný mtime/atributy a výsledek. Zdroj zůstane zachovaný. Pokud v mezeře vznikne cizí položka, publish selže a nepřepíše ji. Staging neznamená rezervaci cílového názvu.

**Přepis:** zjistit aktuální cíl a jeho DACL → SaveOriginal a Verify → CopyNew zdroje do stagingu → připravit původní cílovou DACL → Verify → znovu běžně zkontrolovat očekávaný cíl → MoveFileExW s nahrazením → konečná metadata a kontrola výsledku. Není to podmíněné nahrazení pouze určitého file ID a neslibuje se odstranění všech souběhových závodů. Nejde ani o transakci napříč svazkem obnovy a cílem. Při chybě po publikaci zůstane obnova a operace je chybová/nejistá; žádný automatický rollback. Standardní přejmenování má nahradit jméno, nikoli přepsat data starého hardlinkovaného objektu; konkrétní výsledek musí potvrdit běžný hardlink test níže.

**Mazání:** SaveOriginal a Verify → aktuální kontrola stejného objektu → DeleteFileW → výsledek. Neúplná záloha, chybějící právo nebo zjištěná změna znamenají bez mazání. Prázdné složky samostatně až po dětech, nikdy podle filtrované prázdnosti. Selhání odstranění není důvodem smazat zálohu.

**Stop:** callback CopyFileExW vrací PROGRESS_CANCEL; dokumentace rozlišuje jeho odstranění částečné kopie od PROGRESS_STOP, který ji ponechá. Pro naše vlastní staging/backup preferovat první variantu, ověřit a případně ohlásit zbytek. Stop před publish/delete zabrání destruktivnímu kroku; přijde-li až během krátkého dokončení, zaznamenat skutečný výsledek aktuální položky a nezačít další. Nepřidávat složitý systém obnovitelného přenosu.

**Read-only cíl:** běžná chyba či odmítnutá položka jsou v první úzké etapě přijatelné. Automatické odstranění tohoto atributu může změnit další hardlinkové jméno, a proto není součástí minimálního adaptéru. Nejde o změnu D5 pro přenos Read-only ze zdroje; ten se zachová jako konečný atribut nové kopie. Zvláštní UX pro potvrzený přepis Read-only lze doplnit později.

## API podklady a co není dosud ověřeno

[CopyFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-copyfileexw) dokumentuje přenos ADS i dalších vlastností a průběh/cancel. Je to důvod preferovat hotovou přenosovou funkci, nikoli důkaz přesného D5 v naší kombinaci; zděděná DACL a bezpečnostní resource attributes vyžadují běžný test a správné cílové nastavení.

[MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw) popisuje nahrazení existujícího souboru, požadavky ACL a odlišné chování přes svazky. Proto staging u cíle a žádný COPY_ALLOWED. Nepřisuzujeme API porovnání očekávaného file ID, transakční rollback ani prokázanou power-loss odolnost. [SetNamedSecurityInfoW](https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-setnamedsecurityinfow) je podklad pro práci s cílovou DACL a její dědičností; nejde o přenos celých zdrojových oprávnění.

Původní sonda ověřovala jiný native rename protokol. Jejích 347 assertions / 53 procesů se **nevydává za test navrženého MoveFileExW/CopyFileExW adaptéru**. Jde zatím o zdrojově/API podložený návrh. SMB, složité ACL závody, zamykání ADS a ztráta napájení se nyní netestují.

## Navržené běžné testy

Všechny scénáře jsou plán pro budoucí implementaci na malých izolovaných místních fixtures; nic z této tabulky nebylo tímto mapováním spuštěno. Nejde o požadavek nové laboratorní infrastruktury.

| Skupina | Akceptační případ |
| --- | --- |
| T1 Nový cíl | Nenulový i prázdný soubor, správná data, mtime a cílové dědění. Existující cíl/nová kolize nesmí být přepsán bez schválené akce. |
| T2 Přepis a D4 | Starý i nový obsah s různými ADS. Obnova má přesně staré proudy, nový cíl přesně nové, bez sloučení. Záloha má jiný ID a přežije změnu starého hardlinku. |
| T3 Hardlinky | Jiná hardlinka původního cíle před přepisem zachová hlavní data, ADS a podporované atributy; publikovaný cíl nezůstane touto hardlinkou. |
| T4 D5 metadata | Zdrojová chráněná DACL se nepřenese; nový cíl dědí v cíli, přepis zachová původní cílovou chráněnou i běžně děděnou DACL. mtime a Hidden/System/Read-only na nové kopii; chyba požadovaného nastavení není úspěch. |
| T5 Mazání | Platná nezávislá záloha před DeleteFileW, skutečně odstraněná pouze zařazená položka. Chyba/Stop při záloze nebo změna cíle nechá původní cíl. |
| T6 Adresáře a rozsah | Potřebný rodič a existující kolize typu; skutečně prázdná složka versus skryté/vyloučené/nové dítě. Nezařazené či skryté akce se nevykonají; adresář s neuchovanými ADS se nemaže. |
| T7 Stop | Stop během obnovy, stagingu a před publish; původní cíl zachován. Stop kolem dokončení hlásí skutečný dílčí výsledek, další soubor se nezačne. Vlastní zbytky se uklidí nebo výslovně uvedou. |
| T8 Běžné chyby | Obnova nepřístupná, obsazený cíl, Read-only cíl, chybějící WRITE_DAC/WRITE_ATTRIBUTES, nedostatek místa a chyba přenosu/Verify. Nejprve řízená injekce pro větve, skutečné reprodukovatelné chyby tam, kde jsou dostupné bez plnění systémového disku. Žádná chyba nepřejde do úspěchu/destrukce bez obnovy. |
| T9 Zastaralý plán | Změna hlavních dat či identity po Compare, zmizelý cíl a dříve chybějící nově vzniklý cíl. Použít existující kontrolní logiku; nevyžadovat úplné závodové testování. |
| T10 Cesty | ASCII, český/emoji/CJK soubor i předci, více Unicode komponent, UTF-8 >260 B a UTF-16 >260 jednotek pro staging, obnovu a protokol. SDK omezení nesmějí zkrátit nebo ztrátově přesměrovat operaci. |
| T11 Výsledek a obnova | Ruční kopie ze zálohy do nového jména vrátí původní podporovaná data. Výsledek rozliší úspěch, Skip, Stop a chybu včetně selhání po publish. Žádný automatický replay nebo přepis cizí kolize. |
| T12 Integrace | Stávající lifecycle/Stop/Close a tmavé schéma, lokalizované chyby, skutečný progress bez volání zapůjčeného SDK interface z workeru, správný refresh a nový náhled po běhu. CI buildy/testy přiměřené změně. |

## Doporučená nejbližší etapa

Po samostatném pokynu začít **jen kopií do nového cíle**: CopyNew, vlastní staging, běžné Verify/metadatové kontroly, publish bez nahrazení, Stop a výsledek. Ověřit hlavně T1, přenosovou část T4 a T7–T10; cíle již existující zůstanou blokované. Tím se prakticky rozhodne vhodnost CopyFileExW pro D5 bez stavby pokročilých sond. Přepis a mazání zařadit teprve poté se společným SaveOriginal a T2/T3/T5, nikoli jako tichý vedlejší efekt prvního kroku.

Tento dokument nic z těchto zápisů neimplementuje ani nezapíná. Aktuální úkol končí mapováním a návrhem; SDK, hostitel, testovací a produkční kód zůstávají beze změn.
