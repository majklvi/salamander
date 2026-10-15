# Directory Synchronization: status bar a strukturované dialogy

16. října 2026. Zpřehlednění stávajícího rozhraní; žádná změna pravidel skenu, kopírování, přepisů, mazání ani obnovy. Bez mazání složek, SMB testů a změn SDK/hostitele.

## Rozhraní

- Spodní statický text nahradil skutečný nativní `msctls_statusbar32`. Dvě stabilní části: vlevo stav/fáze operace, vpravo počty řádků nebo přenesené/ověřené bajty a dokončené položky. Stav rozlišuje Ready, dokončené porovnání, běh, Stop, chybu, zneplatněný plán a dokončenou operaci s požadavkem nového Compare. Texty mají tooltip. Status bar se při změně velikosti drží spodního okraje a používá existující darkmode podporu včetně darkmodelib.
- Plan summary a všechna čtyři potvrzení i čtyři výsledkové souhrny používají společný lokalizovaný, zvětšitelný nativní dialog s tabulkou **Field / Value**. Počty souborů, hlavní bajty, chyby/přerušení, nezahájené a vynechané návrhy jsou samostatné řádky. Obnova ON/OFF, cesta a oba příslušné protokoly mají vlastní pole. ADS jsou výslovně nad rámec uváděných hlavních bajtů.
- Vybraný řádek se celý zobrazí v samostatném read-only poli se zalamováním, výběrem textu a posuvem. Dlouhá cesta tak není odkázaná na zkrácenou buňku tabulky. Chybové/staging podrobnosti mají vlastní řádek; vysvětlení, omezení a varování jsou v oddělené posuvné části Notes. Texty i cesty zůstávají UTF-16 bez převodu přes ACP.
- Potvrzovací tlačítko jmenuje skutečnou akci: Copy files / Save originals / Overwrite files / Delete files. **Výchozí volba potvrzení je Cancel**; Escape, zavření okna i neúspěšné vytvoření dialogu nepotvrdí operaci. Výsledky mají jediné OK vpravo dole. Tab a mnemoniky zpřístupňují tabulku i obě textová pole. Použitý font zůstává MS Shell Dlg.

Potvrzení zápisových operací jsou záměrnou součástí release, nikoli diagnostická okna. Zachovávají kontrolu rozsahu a ON/OFF před zápisem. Výsledkové souhrny také zůstávají modalní, ale data už nejsou jedním dlouhým odstavcem. Krátká jednotlivá hlášení (například chybějící vstup, žádná způsobilá položka nebo selhání workeru), About a menu Help nadále používají existující hlášení. Nemění se model samostatného potvrzení každého druhu operace.

## Ověření

- Produkční plugin i jazykový modul sestaveny Debug a Release x64, s warnings as errors.
- **1 675 kontrol resource/layout prošlo v Debug i Release**: oba skutečné přeložené dialogy, geometrie, MS Shell Dlg, mnemoniky, Tab, nativní třída status baru a read-only posuvná pole. Metriky při 96/120/144/192/240 DPI zahrnují i dynamické texty všech potvrzovacích tlačítek. Není to simulace runtime přechodu mezi monitory.
- **2 343 stávajících lifecycle kontrol prošlo v Debug i Release x64**, včetně Stop/Close/force Release při skenu, porovnání, přenosech, Verify a mazání. Zápisový adaptér se neměnil.
- Computer Use, privátní Release hostitel 0.16, Windows Dark Mode (experimental), izolovaná NTFS fixture `C:\Samandarin\builds\dirsync-ui\gui-20261016`: skutečný dvoudílný status bar potvrzen i přes accessibility. Tabulkový plán zobrazil správné počty a bajty. Enter v potvrzení nové kopie provedl Cancel, nikoli zápis. Následná výslovná nová kopie Unicode souboru/ADS skončila Copied 1, errors 0, pending 0, omitted 2.
- Potvrzení přepisu zobrazilo samostatně počty, nové/původní bajty, ON a cestu obnovy. Cesta o **275 UTF-16 jednotkách / 711 UTF-8 bajtech** byla v Selected value úplná; přesná shoda s textem zadaným do náhledu ověřena přes accessibility. Šlo o zobrazení cesty, nikoli její použití k zápisu. Maximalizace dialogu zachovala tabulku, hodnotu, poznámky i tlačítka. Přepis zrušen Escape.
- Potvrzení mazání ověřeno pro ON i OFF, včetně výrazného textu o absenci obnovy při OFF. Obě zrušena bez provedení. V tomto ověření rozhraní nevznikl přepis, odstranění ani obnova.
- Finální binárka po doplnění názvů tlačítek a zarovnání OK ověřena znovu: stav Comparison complete, potvrzení Copy files pro dvě nové kopie (14 B, jeden jiný návrh vynechán), výsledek Copied 2 / errors 0 / pending 0 / omitted 1 a stav Run finished již při otevřeném souhrnu. Maximalizace hlavního náhledu zachovala spodní status bar bez překrytí tlačítek. Šlo o nový pravý final.txt a nový levý delete.txt; existující cíle nebyly přepisovány.

Deset následných kontrol fixture prošlo: správná nová data/ADS, obě finální kopie, nezměněný pravý overwrite.txt i delete.txt, ponechaná prázdná složka, prázdná obnova, přesně obnovená soukromá konfigurace a zachované tři původní uživatelské instance. Evidence mimo Git: `results.json` a `config-before.reg` ve fixture. Finální SPL SHA-256 `BF0E663EE9CD89F3B126B1E487264B48A7515B43A78C1B274C5BF5DC2072B660`, SLG `DE79568410C49846F6BA3F244AA48999633539448A742F66D50057776591050A`.

GUI běhy přímo ověřují úspěšné výsledky nových kopií a potvrzení přepisu/mazání; nové výsledkové dialogy ostatních operací využívají stejný presenter, ale jejich dokončené GUI běhy se zde netvrdí. Chyby/přerušení adaptéru a dřívější GUI operace dokládají navazující dokumenty. Žádná nová kampaň souběhových či pádových scénářů, žádné změny SDK.
