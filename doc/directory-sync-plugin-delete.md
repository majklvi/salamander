# Directory Synchronization: asymetrické mazání souborů

15. října 2026, navazuje na [přepisy D4/D5](directory-sync-plugin-overwrite.md). Cílem je běžná použitelnost současného jádra nad existujícím SDK 0.16. Bez mazání složek, SMB testů a změn SDK/hostitele.

## Krok 1: adaptér a obnova

`DeleteFiles` ve stávajícím adaptéru přijímá zmrazené viditelné indexy. Vyžaduje platný asymetrický snapshot a akci DeleteRight nad podporovaným běžným souborem pouze vpravo. Skip, kopie, konflikty, blokované položky, adresáře a položky existující vlevo se neprovedou. Indexy se deduplikují; žádné mazání podle nefiltrovaného stromu, rekurzivní odstranění ani Koš. V tomto kroku ještě není nový příkaz připojený do GUI.

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

Příští krok: samostatně potvrzovaný lokalizovaný příkaz pro viditelné soubory, zmrazení zapamatované volby ON/OFF a cesty, průběh/Stop/Close, pravdivý souhrn s obnovou/logy, zneplatnění náhledu a oznámení panelům. Testovací hooky zůstávají mimo distribuovaný SPL.
