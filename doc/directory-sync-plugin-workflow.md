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