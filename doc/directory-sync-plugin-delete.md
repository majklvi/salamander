# Directory Synchronization: asymetrické mazání souborů

15. října 2026, navazuje na [přepisy D4/D5](directory-sync-plugin-overwrite.md). Cílem je běžná použitelnost současného jádra nad existujícím SDK 0.16. Bez mazání složek, SMB testů a změn SDK/hostitele.

## Krok 1: adaptér a obnova

`DeleteFiles` ve stávajícím adaptéru přijímá zmrazené viditelné indexy. Vyžaduje platný asymetrický snapshot a akci DeleteRight nad podporovaným běžným souborem pouze vpravo. Skip, kopie, konflikty, blokované položky, adresáře a položky existující vlevo se neprovedou. Indexy se deduplikují; žádné mazání podle nefiltrovaného stromu, rekurzivní odstranění ani Koš. První commit `2fbe222f` obsahoval adaptér bez příkazu GUI; druhý krok níže jej připojuje.

1. Revalidate snapshotu a odmítnutí read-only cíle beze změny atributů či dalších hardlinkových jmen.
2. Se zapnutým Keep originals nejprve nezávisle uložit **všechny** zařazené originály mimo oba stromy přes stávající staging, Verify hlavních dat i celé sady ADS, podporovaný mtime/atributy a zalogování. Chyba druhé zálohy zabrání i prvnímu mazání. Recovery kopie dědí oprávnění složky obnovy; nepřenáší původní owner/SACL/DACL pro automatické obnovení.
3. Při ON vytvořit ve stejném novém běhu `delete.jsonl` přes CREATE_NEW; všechny planned záznamy flushnout před prvním mazáním. Při OFF se cesta obnovy nepoužije a stejně jako u přepisů zatím není trvalý protokol; zůstává výsledek v paměti/UI. Staré zálohy se neodstraňují.
4. Před každým DeleteFileW zkontrolovat Stop, bezpečné existující rodiče, stále chybějící protějšek vlevo a původní pravou identitu, velikost, mtime/change time, atributy a počet hardlinků. Žádné clearing read-only, změny DACL či zvyšování oprávnění.
5. DeleteFileW na konkrétním jméně, zaznamenat skutečný výsledek, při ON append+flush. První chyba/Stop zastaví zbytek. Hotové odstranění se nevrací automaticky zpět; obnova zůstává. Stop po odstranění zabrání dalším položkám.

Úroveň běžných operací jádra: kontrola a DeleteFileW netvoří atomické podmíněné smazání konkrétního file ID; není úplný snapshot proti libovolným souběžným změnám ADS/ACL ani crash/power-loss transakce. Úspěch znamená přijetí odstranění jména Windows; při dalších otevřených handlerech se fyzické uvolnění může odložit. Další hardlinková jména ponechají původní data, ADS, atributy a DACL. Prázdné rodiče se neuklízejí.

## Protokol při ON

`manifest.jsonl` popisuje samostatnou dokončenou obnovovací fázi `save_originals_only`; její complete není dokončení mazání. `delete.jsonl` je UTF-8 bez BOM, LF, každý záznam WriteFile+FlushFileBuffers:

| event | Pole |
| --- | --- |
| run | schema=1, operation=delete_right, keepOriginals=true, left, right, recoveryManifest |
| planned | item od 1, target, saved |
| result | item, status deleted/failed/cancelled/not_started, Windows error, removed |
| end | status complete/failed/cancelled |

Chyba planned před mazáním ponechá všechny originály. Chyba výsledkového logu po přijatém DeleteFileW zachová známý stav Deleted/removed=true v reportu, označí běh jako neúplný a další soubor nezačne. Chybějící/částečný end není úspěch. Není replay, automatická obnova ani purge. Ruční obnova nejprve do nového jména.

## Ověření a další krok

**3 380 společných kontrol adaptéru prošlo Debug i Release x64 s warnings as errors.** Nativní testy skutečného adaptéru na místním NTFS pokrývají ON/OFF, prázdné hlavní proudy i ADS, Unicode ADS, vnořené soubory bez protějškových rodičů, Hidden/System, hardlinkovou nezávislost, zachování adresářů, viditelné indexy/deduplikaci, odmítnutí symetrického snapshotu, změny po skenu i těsně před DeleteFileW, nově vzniklý levý protějšek, reparse rodiče, read-only, sharing a skutečně odepřené DELETE i FILE_DELETE_CHILD. Dále Stop před/po odstranění, Stop při skutečném přenosu/čtení Verify obnovy, poškozená obnova, řízený nedostatek místa při druhé obnově a chyby planned/result logu. Dlouhé Unicode cesty překračují 260 UTF-16 i UTF-8 jednotek. GUI a lifecycle ověření mazání budou součástí druhého kroku.

CI prvního commitu `2fbe222f`: [PR Build](https://github.com/majklvi/salamander/actions/runs/38812674869) Win32/x64/ARM64 Debug s warnings as errors prošel, [PR Tests](https://github.com/majklvi/salamander/actions/runs/38812674910) 41 native/source-contract skupin + 49 Python testů, 0 failures/skips; [Source Growth](https://github.com/majklvi/salamander/actions/runs/38812674819) a squash guard prošly. Skutečná CodeQL analýza byla stávajícím gate přeskočena kvůli absenci změn hlavních C++ souborů aplikace, nejde o dokončenou bezpečnostní analýzu.

## Krok 2: použitelný příkaz v GUI

Lokalizované **Delete files…** má samostatný řádek s výslovným omezením na pravostranné soubory v asymetrickém režimu. Samostatně revaliduje a potvrzuje viditelné způsobilé návrhy, počet souborů, hlavní bajty, další objem ADS a počet vynechaných návrhů. Potvrzení uvádí ON/OFF, nevyužití Koše, zachování složek a read-only odmítnutí. Při ON uvádí cestu a požadavek uložení všech originálů před mazáním; při OFF výslovně varuje před absencí obnovy dat/ADS. Volba a cesta se zmrazí pro worker a během operace jsou ovládací prvky vypnuté.

Průběh počítá dokončené obnovovací přenosy a mazání, odděleně přenesené/ověřené bajty. Stop a Close používají společný cancel/join. Souhrn uvádí Deleted, chyby/přerušení, Not started, vynechané návrhy, ověřené/zalogované obnovy a cesty obou protokolů. Log chyba po smazání nevrací položce vymyšlený neúspěch odstranění: její skutečný Deleted je zachován a celý běh je incomplete. Po běhu se snapshot zneplatní, vyžádá Compare a přes existující SDK se oznámí pravý kořen i případná obnova; worker SDK nevolá.

Použití: Compare s Asymmetric → zkontrolovat viditelné Delete on right návrhy a případně Skip/filtry → nastavit Keep originals a při ON složku mimo oba stromy → Delete files… → zkontrolovat potvrzení a souhrn → nové Compare. Zařazuje se viditelná sada, nikoli jen zvýrazněné řádky. Prázdné složky mohou zůstat návrhem v náhledu; příkaz je započte jako vynechané a nikdy je neodstraní. Automatická synchronizace několika druhů operací jedním tlačítkem není součástí této první verze.

### Ověření druhého kroku

- **2 343 lifecycle kontrol Debug i Release x64**, warnings as errors. Nových 18 scénářů: Stop/Close/force Release před DeleteFileW s ON/OFF a s 0/1 předchozím odstraněním, plus stejné kontroly během skutečného Copy/Verify obnovy. Přerušení obnovy zabrání všem mazáním; pozdní Stop ponechá skutečně odstraněnou položku a zachová další originály. Join pod místním limitem 3 s, opakované otevření zbývajících souborů výhradně, žádný staging, okamžité FreeLibrary a odmapovaná testovací DLL.
- **1 319 compiled resource/layout kontrol** při 96/120/144/192/240 DPI, včetně dosažitelnosti nového příkazu přes Tab. Nejde o runtime přepínání monitoru.
- Produkční SPL/SLG sestaveny Debug Win32/x64/ARM64 a Release x64. Runtime nativní testy pouze x64; produkční SPL neobsahuje hooky/exporty testovací sondy.
- Computer Use, privátní Release 0.16, Windows Dark Mode (experimental): nové tlačítko i jeho popis jsou čitelné. Fixture `C:\Samandarin\builds\dirsync-delete\gui-20261015` má shodný keep.txt, pravý delete-a.txt (5 B + Unicode ADS) a prázdnou složku. Asymmetric+Empty folders vytvořily tři řádky; potvrzení správně zařadilo **1 soubor / 5 B**, vynechalo **1 adresář**, uvedlo ON a správnou Unicode cestu obnovy. Skutečné potvrzení GUI mazání zatím čeká na samostatný souhlas podle pravidel Computer Use.

### Meze

Žádné mazání složek, SMB testy nebo změny SDK/hostitele. Není hard kill, kritický shutdown, fyzicky plný VHD pro mazání, jiné filesystémy ani reentrantní unload během živého modalního callbacku. Nativní testy používají skutečný kód v oddělené DLL s řízenými hranicemi; nenahrazují důkaz těchto odložených situací. Read-only se zatím odmítá. Při OFF zůstává absence trvalého logu, při ON ruční obnova bez automatického vracení původních oprávnění.
