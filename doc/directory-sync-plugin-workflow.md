# Directory Synchronization: celý pracovní postup v GUI

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


16. října 2026. Souvislý akceptační průchod nad malou izolovanou sadou na místním NTFS; navazuje na [kopírování](directory-sync-plugin-copy-new.md), [přepisy](directory-sync-plugin-overwrite.md) a [mazání souborů](directory-sync-plugin-delete.md). Bez mazání složek, SMB testů a změn SDK/hostitele.

## Prostředí a vstupy

Computer Use, soukromý Release x64 hostitel Samandarin 0.16, vlastní RegFile konfigurace, Windows Dark Mode (experimental). Produkční plugin bez testovacích hooků z dosavadního headu `9cec2e96`; SPL SHA-256 `96EE8EB4CB84E2501F3CB102B4529BE99ABDBEA8CE2CB4FBA7D8FC3FE3DF9FF7`, english.slg `AB74D3E513A78E80A4A73F4FD7FE46730F6AB1537085747551F35A782774EA41`.

Fixture `C:\Samandarin\builds\dirsync-e2e\gui-20261016`: kořeny `left`/`right`, oddělená zpočátku prázdná složka `recovery-漢字`. Hlavní data jsou UTF-8 bez BOM; testovací ADS mají Unicode jméno `:漢字`.

| Položka | Vlevo | Vpravo | Zamýšlený výsledek |
| --- | --- | --- | --- |
| keep.txt | KEEP, 4 B | shodný obsah a čas | beze změny |
| new-漢字.txt | NEW FILE, 8 B; ADS NEW ADS | chybí | nová kopie doprava |
| overwrite.txt | UPDATED CONTENT, 15 B; ADS UPDATED ADS; o minutu novější | OLD CONTENT, 11 B; ADS OLD ADS | přepis s uchováním originálu |
| skip.txt | LEAVE UNCOPIED, 14 B | chybí | ručně Skip, nekopírovat |
| delete.txt | chybí | DELETE ME, 9 B; ADS DELETE ADS | odstranit soubor s uchováním originálu |
| empty-dir | chybí | prázdná složka | ponechat |

## Provedený sled a výsledky

1. Z panelů otevřen plugin, zapnuto By content, Asymmetric a Empty folders. Compare: **6 řádků, žádný konflikt/blokování**, očekávané návrhy a shodný keep.txt. Na vybraný skip.txt použit Proposal Skip → Apply to selected rows; ostatní řádky zůstaly beze změny.
2. Copy new files… zařadilo **1 soubor / 8 B**, vynechalo **3 jiné návrhy** (přepis, odstranění souboru a složky). Vybraný byl stále skip.txt: příkaz správně použil způsobilou viditelnou sadu, nikoli zvýraznění. Výsledek **Copied and verified 1, errors/cancelled 0, not started 0, omitted 3**. Nezávisle ověřena hlavní data i ADS nové kopie, nepřítomnost pravého skip.txt, nezměněné oba budoucí destruktivní cíle a prázdná obnova.
3. Nastavena oddělená Unicode cesta obnovy, Keep originals ON; nové Compare označilo novou kopii Equal. Overwrite files… zařadilo pouze **1 přepis / 15 B nových a 11 B původních hlavních dat**, vynechalo **3 jiné návrhy**. Výsledek **Replaced and verified 1, errors/cancelled 0, not started 0, omitted 3**, obnova **1 z 1**. Cíl obsahoval nová hlavní data i ADS a zachoval původní cílovou DACL. Samostatná obnova obsahovala přesně OLD CONTENT a OLD ADS, manifest i overwrite.jsonl končily complete. Soubor k mazání zůstal původní, skip.txt se nekopíroval.
4. Nové Compare označilo přepis Equal. Delete files… zařadilo pouze **1 pravostranný soubor / 9 B**, vynechalo **2 jiné návrhy** (složku a stále chybějící kopii skip.txt). Potvrzení správně uvedlo ON, oddělenou obnovu a zachování složek. Po konkrétním souhlasu uživatele provedeno mazání: **Deleted 1, errors/cancelled 0, not started 0, omitted 2**, obnova **1 z 1**, samostatný nový běh s manifestem a delete.jsonl.
5. Závěrečné Compare s By content: **5 řádků, 3 páry Equal/Skip s důvodem Binary content, žádné konflikty/blokování**. delete.txt zmizel. Zbývají dvě zamýšlené odlišnosti: levý skip.txt a pravá prázdná složka. Nejde o tvrzení, že jsou celé stromy totožné.
6. Po Close se panel automaticky aktualizoval bez F2: nová Unicode kopie, přepis s velikostí 15 B, odstraněný delete.txt a ponechaná složka. Hostitel běžně ukončen, soukromá konfigurace obnovena byte-for-byte se shodným SHA-256; tři původní uživatelské instance zůstaly spuštěné.

## Nezávislé ověření a závěr

**37 následných kontrol disku prošlo, 0 selhání.** Shodné SHA-256 hlavních dat, mtime a atributy všech tří výsledných párů; přesná sada hlavního proudu a jednoho Unicode ADS i jeho obsah u nové kopie a přepisu. Původní levé soubory a pravý keep.txt zachovaly hashe i mtime. Cílová DACL přepisu zachována, odstraněné jméno chybí, skip.txt vpravo nevznikl, složka zůstala. Přesné inventáře obou stromů vylučují pozůstalý staging nebo jiné neočekávané položky.

V obnově jsou přesně dva originální soubory z oddělených běhů, s původními hashi hlavních dat a správnými ADS. Pozdější mazání nezměnilo dřívější zálohu přepisu. Oba manifesty i oba prováděcí protokoly jsou platné JSONL a končí complete. Dílčí výsledky a evidence zůstávají mimo Git: `before.json`, `intermediate.json`, `results.json`, záloha konfigurace i malé testovací soubory ve výše uvedené fixture.

Průchod neodhalil chybu produkčního kódu; mění se pouze dokumentace. Praktické pravidlo: **Compare sestavuje nový plán a ruční Skip se do něj nepřenáší.** Před každou další operací je třeba nový náhled zkontrolovat a potřebné ruční volby znovu použít. V tomto průchodu další samostatné příkazy přepisu/mazání nekopírovaly skip.txt ani po obnovení jeho návrhu Copy to right. Trvalé vyloučení lze vyjádřit maskou; tuto variantu tento průchod netestoval.

Jde o jeden běžný úspěšný pracovní postup s obnovou ON, nikoli novou chybovou/souběhovou kampaň. OFF a přerušení dokládají dřívější samostatná ověření. Závěrečné GUI porovnání Binary content se týká hlavních dat; ADS byly navíc ověřeny nezávisle na disku. Neověřuje se SMB, mazání složek, instalátor ani jiné architektury za běhu. Samostatné příkazy a ruční obnova zůstávají rozsahem první verze.

## Rozsah operačních tlačítek — 16. října 2026

Aktuální rozhraní rozlišuje tři věci: `Scan panel selection` omezuje vstupy skenu podle původního označení v panelech, `Apply to highlighted rows` mění návrhy pouze zvýrazněných řádků náhledu a operační tlačítka používají **všechny viditelné způsobilé návrhy bez ohledu na zvýraznění**. Názvy Copy/Overwrite/Save/Delete obsahují „visible“; stálá instrukce nad náhledem, první řádek Scope v každém potvrzení a úvod poznámek uvádějí tentýž rozsah. Kopie, přepisy a mazání se spouštějí samostatně. Skryté řádky a Skip se neprovádějí. Počítadlo Other visible proposals omitted znamená jiné viditelné nenulové návrhy, nikoli skryté řádky či Skip. Plan summary zahrnuje viditelné návrhy; součty kopií zahrnují nové i existující cíle. Compare návrhy sestavuje znovu, proto je třeba případný ruční Skip zopakovat; nově to připomíná souhrn, výsledek i nápověda.

Computer Use ověření proběhlo v soukromém Release x64 Samandarinu 0.16 v experimentálním Windows Dark Mode, s produkčním SPL i neupraveným produkčním SLG. Fixture `C:\Samandarin\builds\dirsync-ui\scope-20261016` obsahovala dvě nové kopie doprava (5 + 5 B, jedna s Unicode jménem), jeden budoucí přepis (9 B proti 3 B), další levostranný soubor 4 B a pravostranný soubor 6 B pro kopii doleva; oddělená obnova `recovery-漢字` byla prázdná.

| Ověření | Výsledek |
| --- | --- |
| Apply při zvýraznění pouze skip.txt | Jen tento návrh přešel na Skip; ostatní zůstaly původní. |
| Vypnutí Right to left | Návrh kopie doleva zmizel, stav 5 řádků / 4 viditelné / 1 skrytý / 0 blokovaných. |
| Plan summary | 3 kopie doprava / 19 B včetně přepisu; 0 doleva, 1 skrytý. |
| Potvrzení přepisu a uložení originálů, obě zrušena | Každé zařadilo 1 existující cíl, 3 B originálu, vynechalo 2 jiné viditelné návrhy. Scope a upozornění na zvýraznění/Skip/skryté řádky čitelné. Žádný přepis ani záloha neprovedeny. |
| Zvýrazněno pouze a-visible-漢字.txt, spuštěno Copy visible new files | Potvrzení 2 soubory / 10 B, 1 jiný návrh vynechán; výsledek 2 ověřené kopie, 0 chyb, 0 nezahájených, 1 vynechaný. Nezvýrazněný b-visible.txt se správně také zkopíroval. |
| Nové Compare | Obě nové kopie Equal; skip.txt opět Copy to right, původní cíl přepisu dál odlišný a jeden návrh skrytý. |

**12 nezávislých kontrol disku prošlo:** všech šest původních souborů má původní SHA-256, obě nové kopie odpovídají zdrojům, skrytý ani Skip cíl nevznikl, obnova zůstala prázdná a přesný počet osmi souborů v obou stromech vylučuje další výstupy. Soukromý hostitel běžně ukončen, jeho konfigurace obnovena byte-for-byte, tři původní uživatelské instance zachovány. Evidence mimo Git: before.json, results.json a config-before.reg ve fixture.

Debug i Release x64 produkční build s warnings-as-errors prošel; v obou konfiguracích prošlo **1 670** stávajících kontrol kompilovaných resources a rozložení při 96/120/144/192/240 DPI. Nové texty se vejdou do stávajícího kompaktního rozložení. Nemění se výběr operací ani adaptéry. Jde o ověření rozsahu přes novou kopii a zrušená potvrzení; neprohlašuje nový běh přepisu/mazání, runtime přechody DPI, instalaci, SMB ani jiné architektury za běhu. Mazání složek a změny SDK/hostitele nejsou součástí.
## 2026-10-16 — přesun rozsahu do nabídek

Aktuální rozložení už nemá dolní tlačítka ani Proposal/Apply. Návrhy se upravují přes výběrové kontextové menu/zkratky, operační příkazy přes horní Synchronize… nebo oddělenou podnabídku Visible plan. Všechny dříve ověřené zásady rozsahu a potvrzení zůstávají: operace mají celý viditelný způsobilý plán, nezávisle na zvýraznění, a běží odděleně. GUI bez výběru ověřilo potvrzení jednoho přepisu / 6 B, následně Cancel, a Plan summary 1 / 6. Nevznikl nový smíšený executor. Podrobnosti v aktuálním návodu a poslední GUI/menu evidenci.

## 2026-10-16 — volby příštího porovnání

Aktuální checkboxy Subfolders/By content/Ignore date/Asymmetric/Panel selection/Empty folders nemění ani nemažou zobrazený plán. Použijí se až příštím Compare; do té doby i kontextové a operační příkazy používají snapshot a režim původního porovnání. Stav hlásí čekající volby. Původní potvrzení ztráty výsledků a jeho preference byly odstraněny. Samotná textová změna masky nebo kořenové cesty zůstává přímým zneplatněním. Filtry jsou nezávislé kombinovatelné přepínače, nikoli volba jediné kategorie; GUI i nativní test kombinací potvrdily zachování ostatních stavů. Skip je šedý s tlumeným odstínem State; tabulka sahá až k bokům a status baru.

## 2026-10-16 — aktuální průchod včetně ruční obnovy

Computer Use, soukromý Samandarin 0.16 Release x64, Windows Dark Mode (experimental), produkční plugin z `8acdbd98`. SPL SHA-256 `45F5161B1E14AA55D469403CC968CCCAD909F29FADE831D65E694C1F985C4FDD` odpovídá rozbalenému balíčku `dark-responsive-dpi-20261016`. Nebyly použity testovací hooky ani změny hostitele/SDK. Fixture na místním NTFS: `C:\Samandarin\builds\dirsync-pilot-20261016`, kořeny `left`/`right`, samostatná obnova `recovery-漢字` a kontrolní cíl `restored`.

| Položka | Vstup | Očekávání |
| --- | --- | --- |
| equal.txt | obě strany 5 B, shodný čas i data | Equal |
| new-漢字.txt | pouze vlevo, 8 B + Unicode ADS | nová kopie doprava |
| overwrite.txt | vlevo 12 B, o minutu novější; vpravo 8 B, obě strany s ADS | přepis doprava, uchovat původní pravý soubor |
| skip.txt | pouze vlevo, 9 B + ADS | ručně Skip před první kopií |
| delete.txt | pouze vpravo, 11 B + ADS | samostatně odstranit s obnovou ON |
| empty-dir | prázdná pravá složka | zachovat |

Provedený postup:

1. Kořeny zadány přímo v pluginu, maska `*.*`, Subfolders, By content, Asymmetric a Empty folders ON. Compare sestavilo šest řádků bez blokování; shoda měla důvod Binary content, přepis Left is newer. Výběr skip.txt a Ctrl+M změnil právě jeden návrh.
2. Filtr Equal OFF: 5 viditelných / 1 skrytý. Navíc směr doprava OFF: 3 / 3. Opětovné zapnutí směru: 5 / 1, zapnutí Equal: 6 / 0. Ruční Skip zůstal zachovaný. Nejde o přepínače vzájemně výlučné kategorie.
3. Synchronize → Copy visible new files zařadilo 1 soubor / 8 B, vynechalo 3 jiné návrhy. Výsledek 1 copied and verified, 0 chyb, 0 nezahájených. Zvýrazněný Skip se nekopíroval, nezvýrazněná Unicode kopie ano. Ověřena hlavní data i ADS na disku.
4. Přes Settings nastavena existující oddělená Unicode obnova, Keep originals ON. Nové Compare správně změnilo novou kopii na Equal a obnovilo výchozí návrh skip.txt na Copy to right. Samostatný přepis zařadil pouze overwrite.txt: 12 B nových / 8 B původních dat, 3 jiné návrhy vynechané. Výsledek 1 replaced and verified, 0 chyb, obnova 1/1. Cílová DACL zachována, originál obsahuje OLD DATA a OLD DATA ADS; manifest i overwrite.jsonl končí complete.
5. Po dalším Compare samostatné mazání nabídlo pouze delete.txt / 11 B, 2 jiné návrhy vynechalo. Po konkrétním souhlasu uživatele v GUI odstraněn 1 soubor, 0 chyb, obnova 1/1 v novém běhu; složka zůstala. Závěrečné Compare má 5 viditelných řádků, žádné blokování. Tři páry jsou shodné, skip.txt a prázdná složka zůstávají zamýšlenými odlišnostmi.
6. Z manifestů přes `item`/`planned`/`result: saved` určeny originály obou běhů; prováděcí protokoly potvrzují replaced/deleted a complete. Plugin zavřen přes Close. V panelu hostitele přes Shift+F7 otevřena složka `saved`, zaměřen soubor a přes F5 zkopírován do nového cíle `restored\overwrite.txt`, obdobně `restored\delete.txt`. Obě kopie proběhly bez přepisování existujících dat. Obnova na původní jména v synchronizovaném stromu nebyla součástí testu.

**31 nezávislých následných kontrol prošlo, 0 selhání.** Kontroly zahrnují původní zdroje a shodný cíl, SHA-256 hlavních dat, ADS a mtime nových výsledků, cílovou DACL, nepřítomnost Skip/odstraněného jména, zachování složky a přesný inventář pravého stromu; dále původní hashe uchovaných i ručně obnovených souborů, jejich ADS a mtime a dokončení všech čtyř JSONL protokolů ve dvou bězích. Navíc přímo ověřeny očekávané texty obnovených ADS: OLD DATA ADS a DELETE DATA ADS. Kontrolní kopie obsahují jen hlavní proud a odpovídající Unicode ADS.

Soukromý hostitel běžně ukončen, konfigurace obnovena byte-for-byte; tři uživatelské instance zůstaly spuštěné. Evidence mimo Git: `before.json`, `overwrite-evidence.json`, `results.json`, konfigurace před testem a malé testovací soubory v uvedené fixture. Průchod neodhalil chybu produkčního pluginu; upravuje se pouze návod a tato evidence, nové sestavení není potřeba.

Jde o malý syntetický úspěšný průchod s obnovou ON, nikoli o uživatelský pilot na reprezentativních datech či zátěžový test. Praktická obnova běžným F5 je nyní ověřená i v GUI hostitele. Zůstává vhodné vyzkoušet kopii skutečné pracovní sady a aktuální ZIP na dalším počítači; skutečné jiné systémové DPI a přechod monitorů zůstávají samostatně neověřené. OFF, přerušení a chyby místa dokládají předchozí testy. Bez mazání složek, SMB testů a změn SDK.
