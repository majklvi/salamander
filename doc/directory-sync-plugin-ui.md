# Directory Synchronization: status bar a strukturované dialogy

16. října 2026. Zpřehlednění stávajícího rozhraní; žádná změna pravidel skenu, kopírování, přepisů, mazání ani obnovy. Bez mazání složek, SMB testů a změn SDK/hostitele.

## Rozhraní

- Spodní statický text nahradil skutečný nativní `msctls_statusbar32`. Dvě stabilní části: vlevo stav/fáze operace, vpravo počty řádků nebo přenesené/ověřené bajty a dokončené položky. Stav rozlišuje Ready, dokončené porovnání, běh, Stop, chybu, zneplatněný plán a dokončenou operaci s požadavkem nového Compare. Texty mají tooltip. Status bar se při změně velikosti drží spodního okraje a používá existující darkmode podporu včetně darkmodelib.
- Plan summary a všechna čtyři potvrzení i čtyři výsledkové souhrny používají společný lokalizovaný, zvětšitelný nativní dialog s tabulkou **Field / Value**. Počty souborů, hlavní bajty, chyby/přerušení, nezahájené a vynechané návrhy jsou samostatné řádky. Obnova ON/OFF, cesta a oba příslušné protokoly mají vlastní pole. ADS jsou výslovně nad rámec uváděných hlavních bajtů.
- Samostatné pole Selected value bylo odstraněno. Ctrl+C v tabulce kopíruje úplnou hodnotu vybraného řádku; Ctrl+Shift+C kopíruje celý souhrn včetně názvů polí a poznámek. Stejné dvě volby poskytuje malé kontextové menu přes existující SDK Salamanderu. Chybové/staging podrobnosti mají vlastní řádek; vysvětlení, omezení a varování jsou v oddělené posuvné části Notes. Texty i cesty zůstávají UTF-16 bez převodu přes ACP.
- Potvrzovací tlačítko jmenuje skutečnou akci: Copy files / Save originals / Overwrite files / Delete files. **Výchozí volba potvrzení je Cancel**; Escape, zavření okna i neúspěšné vytvoření dialogu nepotvrdí operaci. Výsledky mají jediné OK vpravo dole. Tab a mnemoniky zpřístupňují tabulku a poznámky. Použitý font zůstává MS Shell Dlg.

Potvrzení zápisových operací jsou záměrnou součástí release, nikoli diagnostická okna. Zachovávají kontrolu rozsahu a ON/OFF před zápisem. Výsledkové souhrny také zůstávají modalní, ale data už nejsou jedním dlouhým odstavcem. Krátká jednotlivá hlášení (například chybějící vstup, žádná způsobilá položka nebo selhání workeru), About a menu Help nadále používají existující hlášení. Nemění se model samostatného potvrzení každého druhu operace.

## Kompaktní rozložení (aktuální)

Šířka společného dialogu klesla z 570 na **300 dialogových jednotek** (přibližně polovina). Okraje mají 7 DU, běžné mezery 5 DU, tlačítka výšku 14 DU. Vlastní rozměry byly porovnány s dialogy jádra v `src/lang/lang.rc`: Copy/Move 227/255 DU, Size results 254 DU. Výška tabulky vychází z počtu skutečných řádků (nejvýše deset před posuvem); výška Notes se měří podle fontu a zalomeného textu, s omezeným maximem. Krátký výsledek tak nemá vyhrazenou velkou prázdnou tabulku. Dialog nadále umožňuje zvětšení a maximalizaci.

## Aktuální ověření

- Produkční plugin a jazykový modul: Debug/Release x64 s warnings as errors. **1 609 resource/layout kontrol** pro oba dialogy při 96/120/144/192/240 DPI prošlo v obou konfiguracích. Jde o metriky přeložených resources, nikoli runtime přechod mezi monitory.
- Stávající lifecycle sada po překladu aktuálního probe: **2 343 kontrol** v Debug i Release x64. Adaptér a SDK se nemění.
- Computer Use, Release hostitel 0.16, Windows Dark Mode (experimental), izolovaná NTFS sada `C:\Samandarin\builds\dirsync-ui\compact-20261016`: souhrn šesti řádků 452 × 285 px, potvrzení nové kopie se třemi řádky 452 × 260 px, výsledek se čtyřmi řádky 452 × 238 px. Bez pole Selected value a bez prázdných řádků navíc. Kontextové menu má dvě položky a odpovídá vzhledu hostitele. Maximalizace i návrat do kompaktní velikosti zachovaly tabulku, poznámky a OK.
- Výslovně potvrzená kopie nového `new-漢字.txt`: Copied and verified 1, errors 0, not started 0, omitted 1. Šest následných kontrol disku/konfigurace prošlo: nová data, nezměněný původní cíl i zdroj, prázdná obnova, přesně obnovená soukromá konfigurace, tři uživatelské instance ponechané v běhu. V tomto běhu neproběhl přepis ani odstranění.
- Původní pokus o GUI ověření schránky byl neprůkazný kvůli cílení zakázaného rodiče v Computer Use. Následující ověření toto omezení obešlo testovacím resource modulem a dokončilo kontrolu všech čtyř způsobů kopírování. Produkční kód kopírování nevyžadoval opravu.

Evidence mimo Git: `results.json`, `config-before.reg` v aktuální fixture. SPL SHA-256 `AE85E5D6A48C079DC19BB8587CA8A58A57311F776A8CB5DA84BC71A7FE3B7C29`, SLG `F9FCB15A1C2CF963FB1558351BC95E27BBA12B0A16EA387448C31CE499FE7DDD`.

## Dokončené ověření schránky (16. října 2026)

Computer Use, skutečný Release plugin v hostiteli Samandarin 0.16 x64, Windows Dark Mode (experimental). SPL zůstal **beze změny**. V dočasné kopii anglického SLG byl pouze u `IDD_INFORMATION` přidán extended style `WS_EX_APPWINDOW`; modalita, rodič, dialogová procedura, tabulka, resources textů i skutečné SDK clipboard/menu služby zůstaly stejné. Příznak umožnil vybrat potvrzovací dialog z `list_windows()` místo aktivace jeho zakázaného rodiče. Testovací SLG se nedistribuuje; po testu byl obnoven původní modul a konfigurace, ověřeno SHA-256.

Cesta zadaná do Recovery folder měla **346 jednotek UTF-16 / 896 bajtů UTF-8**: ASCII prefix, `obnova-漢字`, tři samostatné komponenty po 90 znacích `階`, konec `žluťoučký-😀`. Žádná komponenta nepřekračuje běžný limit jména. Cesta sloužila jen jako text; neověřuje se zde její otevření ani zápis. Potvrzení přepisu zobrazilo šest polí, ve kterých tato cesta byla zkrácená na šířku buňky.

| Ovládání | Výsledek vložení do nového dokumentu Poznámkového bloku |
| --- | --- |
| Ctrl+C na řádku Recovery folder | Přesná shoda celé cesty, včetně posledního emoji; žádné zkrácení podle buňky nebo accessibility textu. |
| Ctrl+Shift+C | Přesná shoda názvu dialogu, všech šesti názvů/hodnot, tabulátorů a úplných poznámek včetně prázdných řádků. |
| Pravé tlačítko → Copy value | Přesná shoda celé dlouhé cesty. |
| Shift+F10 → Copy all | Menu ukotvené k vybranému řádku; přesná shoda se souhrnem z Ctrl+Shift+C. |

Shoda se kontrolovala s očekávaným textem přes `document_text` po skutečném Ctrl+V, nikoli přímým čtením schránky nebo pouhou kontrolou návratové hodnoty SDK. Souhrn má 928 jednotek UTF-16 při normalizaci CRLF na LF. Poslední vložení bylo uloženo jako `pasted-summary.txt`: **940 jednotek UTF-16, 1 490 bajtů UTF-8, šest tabulátorů, 12 CRLF**, SHA-256 `C9FE579FD558B4923C712B6467F2D13815A41A6B2D4593B5594D766A62089F95`. Uložená testovací záložka byla zavřena, původní dokumenty editoru ponechány.

Escape zavřelo potvrzení bez provedení přepisu. Původní zdroj `NEW` i cíl `OLD` zůstaly stejné, neproběhlo mazání ani vytvoření obnovy. Tři původní instance Salamanderu zůstaly spuštěné. Evidence mimo Git: `C:\Samandarin\builds\dirsync-ui\clipboard-20261016\results.json`, `pasted-summary.txt`, `shipping.slg`, `probe.slg`, `config-before.reg`. Produkční SPL/SLG mají stejné hashe jako v předchozím odstavci; testovací SLG má hash `935199E17827B4131D8645E1435502C52A15257D5DA00E693DCBBF50D6163A31`.

### Opakování s Computer Use

1. Zavřít pouze soukromý testovací hostitel. Zálohovat jeho konfiguraci i původní `english.slg`.
2. Spustit [prepare-clipboard-probe.ps1](../src/plugins/dirsync/tests/prepare-clipboard-probe.ps1) s `-SourceLanguage <původní english.slg> -ProbeLanguage <nová cesta mimo instalaci>`. Skript odmítá přepis zdroje i existujícího výstupu; ověřuje extended dialog a přesnou očekávanou změnu resource. Neupravuje SPL, repozitářové resources ani SDK.
3. Pouze v kopii hostitele nahradit SLG vytvořeným probe, spustit hostitel a přes GUI zadat izolované kořeny a dlouhou testovací hodnotu. Porovnat, otevřít potvrzení, vybrat jeho **vlastní** vrácené okno. Neprovést žádnou zápisovou operaci.
4. V tabulce vybrat hodnotu. Každou zkratku a menu ověřit skutečným vložením do nového dokumentu; porovnat plné řetězce. Pro celý souhrn kontrolovat také tabulátory a odstavce.
5. Zrušit potvrzení, zavřít testovací hostitel a obnovit původní SLG i konfiguraci. Ověřit shodu hashů a nezměněné cíle.

Jde o ověření skutečných clipboard/menu handlerů s jedním testovacím příznakem dostupnosti okna. Netvrdí se, že Computer Use umí přímo cílit dialog s nezměněným produkčním extended style. Žádná změna SDK/hostitele ani nový test souborových operací není potřeba.

## Předchozí ověření původního širšího presenteru

Následující evidence popisuje předchozí variantu; zmínky o Selected value a tehdejší počty kontrol nejsou popisem aktuálního rozložení.

- Produkční plugin i jazykový modul sestaveny Debug a Release x64, s warnings as errors.
- **1 675 kontrol resource/layout prošlo v Debug i Release**: oba skutečné přeložené dialogy, geometrie, MS Shell Dlg, mnemoniky, Tab, nativní třída status baru a read-only posuvná pole. Metriky při 96/120/144/192/240 DPI zahrnují i dynamické texty všech potvrzovacích tlačítek. Není to simulace runtime přechodu mezi monitory.
- **2 343 stávajících lifecycle kontrol prošlo v Debug i Release x64**, včetně Stop/Close/force Release při skenu, porovnání, přenosech, Verify a mazání. Zápisový adaptér se neměnil.
- Computer Use, privátní Release hostitel 0.16, Windows Dark Mode (experimental), izolovaná NTFS fixture `C:\Samandarin\builds\dirsync-ui\gui-20261016`: skutečný dvoudílný status bar potvrzen i přes accessibility. Tabulkový plán zobrazil správné počty a bajty. Enter v potvrzení nové kopie provedl Cancel, nikoli zápis. Následná výslovná nová kopie Unicode souboru/ADS skončila Copied 1, errors 0, pending 0, omitted 2.
- Potvrzení přepisu zobrazilo samostatně počty, nové/původní bajty, ON a cestu obnovy. Cesta o **275 UTF-16 jednotkách / 711 UTF-8 bajtech** byla v Selected value úplná; přesná shoda s textem zadaným do náhledu ověřena přes accessibility. Šlo o zobrazení cesty, nikoli její použití k zápisu. Maximalizace dialogu zachovala tabulku, hodnotu, poznámky i tlačítka. Přepis zrušen Escape.
- Potvrzení mazání ověřeno pro ON i OFF, včetně výrazného textu o absenci obnovy při OFF. Obě zrušena bez provedení. V tomto ověření rozhraní nevznikl přepis, odstranění ani obnova.
- Finální binárka po doplnění názvů tlačítek a zarovnání OK ověřena znovu: stav Comparison complete, potvrzení Copy files pro dvě nové kopie (14 B, jeden jiný návrh vynechán), výsledek Copied 2 / errors 0 / pending 0 / omitted 1 a stav Run finished již při otevřeném souhrnu. Maximalizace hlavního náhledu zachovala spodní status bar bez překrytí tlačítek. Šlo o nový pravý final.txt a nový levý delete.txt; existující cíle nebyly přepisovány.

Deset následných kontrol fixture prošlo: správná nová data/ADS, obě finální kopie, nezměněný pravý overwrite.txt i delete.txt, ponechaná prázdná složka, prázdná obnova, přesně obnovená soukromá konfigurace a zachované tři původní uživatelské instance. Evidence mimo Git: `results.json` a `config-before.reg` ve fixture. Finální SPL SHA-256 `BF0E663EE9CD89F3B126B1E487264B48A7515B43A78C1B274C5BF5DC2072B660`, SLG `DE79568410C49846F6BA3F244AA48999633539448A742F66D50057776591050A`.

GUI běhy přímo ověřují úspěšné výsledky nových kopií a potvrzení přepisu/mazání; nové výsledkové dialogy ostatních operací využívají stejný presenter, ale jejich dokončené GUI běhy se zde netvrdí. Chyby/přerušení adaptéru a dřívější GUI operace dokládají navazující dokumenty. Žádná nová kampaň souběhových či pádových scénářů, žádné změny SDK.
