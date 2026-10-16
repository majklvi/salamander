# Directory Synchronization: status bar a strukturované dialogy

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


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

## Cesty vedle sebe a značky v seznamu (16. října 2026)

Oba kořeny jsou na jednom řádku ve stejně širokých editorech; při zvětšování se přírůstek šířky dělí mezi ně a pravý popisek se posouvá spolu s pravou polovinou. Pod nimi je maska a jeden řádek šesti voleb skenu. Zobrazovací filtry mají společný řádek s Compare/Stop. Seznam dostal 246 místo 165 dialogových jednotek na výšku (+49 %), přes polovinu výchozí výšky dialogu 470 DU. Při zvětšení okna dostává celý přírůstek výšky. Zůstává native status bar a kompaktní samostatná tlačítka operací.

Action a State doplňují geometrické značky vykreslené přes GDI: šipka doprava/doleva, rovnost/nerovnost, Skip, křížek mazání a vykřičník pro rozhodnutí/blokování. Text zůstává lokalizovaný a dostupný UI Automation. Zelená/modrá/okrová/šedá/červená paleta má samostatné hodnoty pro světlý a tmavý režim; zvýrazněné řádky zachovávají nativní kontrast. Značky nejsou závislé na dostupnosti Unicode symbolu ve fontu. Použit stávající opt-in `Salamander.DarkModeLib.CustomListView`, aby společná knihovna předala custom draw tohoto seznamu pluginu; knihovna, hostitel ani SDK se nemění. Barva se výslovně vrací na běžný text u ostatních sloupců, aby se nepřenášela na velikost/čas/důvod.

Computer Use: soukromý Release x64 hostitel 0.16, produkční SPL/SLG bez testovací úpravy stylu, experimentální Windows Dark Mode. Fixture `C:\Samandarin\builds\dirsync-ui\layout-20261016`, pět porovnávaných cest / sedm souborů: shoda, stejně starý rozdílný pár, levostranné soubory včetně Unicode jména a pravostranný soubor. Ověřeny obě barevné šipky, =, ≠, Skip, výzva k rozhodnutí, čitelnost zvýraznění a po Asymmetric také červený návrh Delete on right. Žádná zápisová operace nebyla spuštěna. Maximalizace/obnovení zachovaly rozložení obou cest, seznamu, spodních prvků a status baru. Finální binárka po běžném Unload a novém načtení potvrdila opravu barev sousedních sloupců i kontrast zvýraznění.

Devět nezávislých kontrol disku prošlo: sedm původních hashů, přesný inventář a prázdná obnova. Hostitel běžně ukončen; soukromá konfigurace následně obnovena byte-for-byte, tři uživatelské instance zachovány. Evidence mimo Git: before.json, results.json, config-before.reg a config-after.reg ve fixture. Finální SPL SHA-256 `2AA8F78913848D95E7F3946E641E8F251B48FB48A69BDBBFC9A94FC2E2C38B01`.

Produkční Debug/Release x64 build s warnings-as-errors prošel. **1 678 resource/layout kontrol** prošlo v Debug i Release při 96/120/144/192/240 DPI, navíc hlídají stejně široké kořeny vedle sebe, jeden řádek voleb skenu a převahu seznamu. Lifecycle regrese **2 343 kontrol** prošla v obou konfiguracích. Nové barevné značky jsou runtime ověřené v tmavém režimu x64; světlý režim a runtime změny DPI monitoru nejsou tímto průchodem ověřené. Bez SMB, mazání složek a změn zápisových adaptérů.

**Později uzavřeno distribučním ověřením:** popis pluginu překračoval limit 260bajtového bufferu hostitele; zkrácení na 136 bajtů opravilo registraci po restartu. Viz [výsledky a regrese](directory-sync-plugin-distribution.md). Následující pozorování je původní evidence před opravou. Otevřený distribuční bod: po jednom běžném restartu této starší soukromé RegFile konfigurace správce ukázal 0 pluginů, ačkoli config.reg stále obsahoval registraci dirsync. Po obnovení config-before.reg se stejná binárka načetla a běžný Unload/reload proběhl správně. Příčina není určena a nebyl to test čisté instalace; případ nesmí být prezentován jako úspěšné ověření registrace po restartu. Při distribučním kroku zopakovat s čistou konfigurací 0.16 a oddělit načtení registrace hostitelem od nastavení Keep originals/cesty pluginu.
## 2026-10-16 — kompaktní volby a konfigurace vzhledu

Aktuální stav nahrazuje dřívější trvale viditelné technické popisky. Maska (58 dialogových jednotek) a šest voleb skenu sdílejí řádek; seznam začíná na y=70 a má 274 jednotek výšky místo 246. Pevná tolerance 3 s je pouze v návodu. Vysvětlení Keep originals, rozsahu příkazů a omezení mazání jsou v nativních tooltipech; F1 na masce, obnově nebo operačním tlačítku poskytuje stejný kontext bez myši. Potvrzení dále obsahují rozsah i ON/OFF obnovy pro přepis/mazání.

Action má výchozí šířku 48 px při 96 DPI a vycentrovaný geometrický symbol. Text zůstává v datovém modelu ListView pro přístupnost, tooltip a stavový řádek při zaměření položky; Show action labels obnoví text a šířku 145 px. Nativně vykreslené pozadí výběru se zachovává, symboly používají textovou barvu výběru. State nadále ukazuje symbol i text.

Appearance je dostupný v náhledu a přes Plugins Manager → Configure. Používá existující SDK AttachColorArrowButton, systémový výběr barvy s DarkModePrepareChooseColor, MS Shell Dlg 8 a stávající darkmodelib. Sedm významů má samostatnou světlou a tmavou paletu; výběr Palette upravuje paletu, nepřepíná hostitele. Restore defaults obnoví pouze vybranou paletu, OK uloží, Cancel/Escape neuloží. Nové hodnoty ActionLabels a LightColorsV1/DarkColorsV1 používají původní Load/Save SDK callbacky. Chybějící, poškozená nebo nesprávně dlouhá paleta má bezpečný návrat k výchozí. SDK ani hostitel se nemění.

Ověření:

- Debug a Release resource test: 1699 kontrol pro tři skutečně kompilované dialogy, měření fontu při 96/120/144/192/240 DPI, bez oříznutých popisků a překryvů, jedinečné mnemoniky. Nejde o přepínání skutečných monitorů ani simulaci WM_DPICHANGED.
- Release lifecycle: 2343 kontrol; rozšířený test konfigurace má 15 podmínek včetně round trip popisků a obou palet, Unicode obnovovací cesty přes 260 znaků a chybných hodnot. Testuje skutečné pluginové callbacky s izolovanou implementací registry SDK.
- Computer Use v soukromém Samandarinu 0.16 x64: tmavý náhled pouze se symbolem, UI Automation stále četla Copy to right. Alt+C spustilo Compare nad izolovanými left/right (1 návrh, žádný zápis). Alt+L zapnulo popisky, Tab přecházel na paletu, Alt+C otevřelo nativní výběr barvy v tmavém režimu; Equal byl změněn na RGB(255,0,255). Enter uložil.
- Normální ukončení a restart: Plugins Manager ukázal zachovanou registraci (1 instalovaný, 0 načtených); Configure načetlo plugin a ukázalo zapnuté popisky i fialovou shodu. Konfigurace na disku obsahovala ActionLabels=1 a přesnou barvu v DarkColorsV1. Alt+R obnovilo návrh palety; Escape jej zahodilo a nové otevření stále ukázalo fialovou. Alt+P/Home vybralo nezměněnou světlou paletu.
- Druhé ověření ve skutečném světlém schématu soukromého hostitele: náhled s texty i konfigurace měly světlý vzhled, F1 na masce ukázalo kontextovou nápovědu. Vypnutí popisků a Enter okamžitě zúžilo Action a zachovalo existující návrh. Výběr řádku aktualizoval stavový řádek na Copy to right. Po uzavření byl na disku ActionLabels=0. Během kontroly byl opraven kontrast symbolu na aktivním výběru.

Pro světlé schéma se upravovala jen konfigurace soukromé testovací instalace. Jedna příprava chybně zapsala REGEDIT4 jako UTF-16; hostitel ji správně odmítl a skončil. Po návratu k původnímu UTF-8 formátu test pokračoval. Nejde o závadu pluginu. Minimální hostitel také stále upozorňuje na chybějící salmon.exe. Běžící uživatelské instance ani jejich konfigurace se neměnily. Žádná GUI souborová operace se nespustila, testovací right i recovery zůstaly prázdné. Bez mazání složek, SMB a změn SDK.
## 2026-10-16 — Appearance pouze ve správci pluginů

Tento záznam nahrazuje výše popsaný přepínač palety a vstup z náhledu. Appearance je nyní dostupný pouze přes předvolby/Configure ve správci pluginů. Tlačítko, handler i resize větev v náhledu byly odstraněny. Dialog má 154 × 201 DU místo 274 × 150 DU (o 44 % užší); sedm významů je současně pod sebou, každý s vlastním barevným tlačítkem a symbolem. Žádný seznam ani přepínač Light/Dark nezůstal. Režim se převezme z hlavní konfigurace Salamanderu. Uložení a Restore defaults se týkají pouze aktuálního režimu; nastavení druhého režimu zůstává zachované a kompatibilní s předchozí verzí.

Release x64 build prošel, Debug/Release resource testy mají 2000 kontrol při 96/120/144/192/240 font DPI, bez překryvů nebo ořezaných popisků. Release lifecycle prošlo 2343 kontrolami. Computer Use v nové izolované kopii 0.16 x64 (compact-20261016-host) ověřilo otevření přes Configure, současné zobrazení všech sedmi řádků, jejich přístupné názvy a klávesové Alt+B pro přímé otevření barvy posledního řádku. Při světlém schématu hostitele byl automaticky použit světlý vzhled/barvy. Volba barvy i celé okno byly zrušeny bez změn. Bez souborových operací, SMB nebo změn SDK. Nová testovací kopie byla zvolena, protože staré testovací SPL/SLG byly zamčené jinou instancí; běžící uživatelské instance se nezavíraly ani nepřepisovaly.
## 2026-10-16 — obnova v Preferences, barvy ve dvou sloupcích

Po upřesnění zadání se přesouvají pouze existující nastavení, žádný úklid záloh se nepřidává. Okno Preferences (306 × 201 DU) má sekce Appearance a Other settings oddělené nativními vodorovnými čarami. Sedm barev se vejde do dvou sloupců (4 + 3), takže ani s novou sekcí nenarostla výška proti předchozímu jednokolonovému dialogu. Keep originals a plná Unicode Recovery folder jsou pouze v Preferences. OK ukládá vše společně, Escape/Cancel rozepsané změny zahodí; vypnutá obnova znepřístupní editor, ale nemaže uloženou cestu ani starší zálohy. Restore defaults stále resetuje pouze barvy aktuálního vzhledu.

Náhled už nemá checkbox obnovy ani její editor; seznam získal 44 DU výšky (318 místo 274). Save visible originals zůstává operačním příkazem nad aktuálními viditelnými návrhy. Save/Overwrite/Delete berou obnovovací cestu z uložených nastavení, nikoli z odstraněného editoru náhledu. Potvrzení přepisů a mazání dále jasně uvádí ON/OFF a při ON i cestu.

Release x64 build prošel. Debug i Release resource testy mají 2122 kontrol při 96/120/144/192/240 font DPI, včetně dvou sloupců, oddělení nastavení a nepřítomnosti obnovovacích ovladačů v náhledu. Release lifecycle prošlo 2343 kontrolami včetně existujícího round trip nastavení a dlouhých Unicode cest.

Computer Use v soukromém preferences-20261016-host (Samandarin 0.16 x64, hlavní tmavé schéma) ověřilo obě čáry, dvě kolony, sedm barevných ovladačů a samostatnou sekci obnovy. Alt+K vypnulo obnovu a znepřístupnilo editor; Escape a nové otevření zachovalo původní ON. Nová cesta obnova-žluťoučký-漢字 byla uložena Enterem. Náhled nastavení neobsahoval, Compare vytvořilo jeden návrh přepisu; jeho potvrzení ukázalo ON a po maximalizaci celou přesnou novou Unicode cestu. Potvrzení bylo zrušeno, žádná souborová operace se neprovedla. Soubory source/target i obnovovací složka zůstaly beze změny. Bez úklidu záloh, mazání složek, SMB a změn SDK.
## 2026-10-16 — úplné tooltipy a potvrzení zahození výsledků

V Preferences se dlouhé statické tooltipy předávají jako úplné Unicode řetězce s životností svázanou s oknem tooltipu, místo odkazu na string resource. Maximální šířka pro zalamování se přepočítává podle DPI. V GUI bylo ověřeno celé znění Keep originals včetně závěru „existing backups are kept.“ a Recovery folder včetně „plugin preferences.“; samostatné popup snímky i text přístupnosti obsahovaly konec obou textů. Popup může přesahovat úzké Preferences; nejde o text omezený hranicí tohoto dialogu.

Změna Subfolders, By content, Ignore date, Asymmetric, Panel selection nebo Empty folders při neprázdných výsledcích nyní předchází dialog Discard comparison results?. Původní stav auto-checkboxu je před dotazem obnoven. Jen Change option aplikuje požadovanou změnu a zneplatní plán. Cancel, výchozí Enter, Escape, křížek nebo neúspěšné vytvoření dialogu nechají volbu i výsledky zachované. Dotaz se řídí všemi řádky, nikoli pouze viditelnými. Do not ask again se ukládá pouze s potvrzenou změnou. Preferences → Other settings → Warn before discarding comparison results jej znovu zapne; výchozí stav je ON. Filtry nemění plán a dotaz nevyvolávají. Textové editace masky a kořenů zůstávají mimo tuto změnu.

Skutečný Computer Use průchod v izolovaném Samandarinu 0.16 x64: C:\Samandarin\builds\dirsync-distribution\discard-warning-20261016-host. Dvojice equal.txt a same-漢字.txt měla shodu a návrh kopie doprava. Nebyla provedena žádná operace nad daty.

| Případ | Pozorovaný výsledek |
| --- | --- |
| Ruční Skip na Unicode řádku; změna Subfolders; Enter | Cancel zachoval Subfolders ON, oba řádky a ruční Skip. |
| Filtry Equal a Different vypnuty | Rows 2 / visible 0 / hidden 2, bez dotazu a bez ztráty plánu. |
| By content při všech skrytých výsledcích; Do not ask again; Escape | Dotaz se zobrazil; výsledky zůstaly skryté, By content OFF. |
| Následná změna Asymmetric | Dotaz se znovu zobrazil: předchozí Escape potlačení neuložil. |
| Asymmetric; Do not ask again; Change option | Asymmetric ON, Rows 0; potlačení uloženo. Po novém Compare změna Ignore date zrušila plán bez dotazu. |
| Normální restart hostitele; Configure v manageru | Plugin zůstal registrovaný; varování v Preferences OFF. V souborové konfiguraci nezávisle ověřeno ConfirmDiscardResults=0. |
| Alt+W a Enter v Preferences | Varování obnoveno a uloženo, ostatní nastavení zachována. |
| Označení souboru v panelu, nový náhled a Compare | Panel selection ON, uzamčené kořeny, jeden Unicode výsledek. |
| Změna Panel selection po obnově varování; Escape | Dotaz opět funkční; původní cesty zůstaly uzamčené, výsledek zachován. |
| Empty folders; zavření potvrzení křížkem | Empty folders OFF a původní jeden výsledek zachován. |

Preferences mají 306 × 219 DU, varování 280 × 99 DU, MS Shell Dlg 8. Kompilované dialogy v Debug i Release prošly 2 506 kontrolami geometrie, úplnosti popisků, mnemonik a Tab při 96/120/144/192/240 DPI, včetně výchozího Cancel a nové volby v Preferences. Nejde o runtime přepínání mezi monitory. Release lifecycle regrese prošla 2 343 kontrolami; mock skutečných SDK callbacků zahrnuje ON/OFF/obnovení i neplatnou hodnotu ConfirmDiscardResults vedle dosavadního nastavení obnovy/vzhledu. Testovací host byl řádně ukončen; běžící uživatelské instance nebyly měněny.

Kontextové menu samotných souborových výsledků bylo zařazeno do dalšího plánu v aktuálním návodu: průzkum referenčního chování, zápis pozorování a následná implementace se současným SDK. V této etapě se neimplementuje. Bez mazání složek, SMB testů a změn SDK.
## 2026-10-16 — nativní kompaktní potvrzení a výběr všech cest

Podle zdrojových dialogů Salamanderu (například IDD_USERMENUCOMPAREARGS / IDD_PLUGINKEYS v src/lang/lang.rc) bylo potvrzení zmenšeno z 280 × 99 na **220 × 70 DU**: o 44 % menší plocha, okraje 7 DU, malé mezery, tlačítka **OK/Cancel 50 × 14 DU**, standardní MS Shell Dlg 8 a centrování pomocí existujícího SDK. Výchozí Enter nadále znamená Cancel; OK přijme změnu. Také potvrzovací tabulky pro souborové operace používají OK/Cancel, nepřepisují již OK na názvy operací. Trvalé pravidlo tohoto rozložení je v AGENTS.md → Native Salamander GUI layout.

Vlevo a vpravo v náhledu a u Recovery folder jsou nyní nativní **…** tlačítka 14 × 14 DU, oddělená od editace čtyřmi DU jako v hostiteli. Šířka editací ustoupila tlačítku; seznam souborů se nezmenšil. Při resize se tlačítka posouvají s koncem příslušné editace. Disabled stav sdílejí s editací: běžící operace / Panel selection a Keep originals OFF.

Dialog výběru je nativní strom SHBrowseForFolderW, stejného typu jako v jádře, s výchozí složkou, centrováním přes SDK a tématem pluginu. Záměrně se nevolá úzké SDK GetTargetDirectory: jeho implementace v 0.16 prochází ANSI a pevnými krátkými buffery. Výsledek získává SHGetNameFromIDList, paměť vlastní standardní shell/COM alokátor. Případný krátký název se rozvine přes GetLongPathNameW s dynamickým bufferem a dlouhou Win32 cestou. SDK ani hostitel se nemění. Nejde o nový vlastní styl výběru složky. Pokud shell složku nezpřístupní, zůstává ruční zadání plné cesty; neúspěch původní hodnotu nepřepíše.

Skutečné GUI ověření: izolovaný Samandarin 0.16 x64 v C:\Samandarin\builds\dirsync-distribution\native-ui-20261016-host, schéma Windows Dark Mode. Bez provedení kopií, přepisů nebo mazání dat.

| Případ | Výsledek |
| --- | --- |
| Compare nad dvěma soubory, Subfolders, výchozí Enter | Kompaktní dialog OK/Cancel čitelný; původní dva výsledky zachované. Ve finálním sestavení ověřeno centrování a OK → volba změněna, Rows 0. |
| Browse left → Cancel | Otevřela se správná výchozí levá složka; původní cesta a Rows 2 zachované. |
| Browse right → OK bez změny výběru | Správná pravá složka; Rows 2 zachované. |
| 305 UTF-16 jednotek / 793 UTF-8 bajtů, tři dlouhé Unicode komponenty | První průchod odhalil návrat zkráceného 8.3 názvu ze shellu. Opraveno rozvinutím názvu; ve finálním dialogu obnovy potvrzen přesný původní Unicode řetězec bez zkrácení. |
| Keep originals OFF/ON | Editace obnovy i její browse tlačítko se společně zakázaly/povolily. |
| Alt+O → Tab → Space v Preferences | Zaměření browse tlačítka a otevření nativního výběru klávesnicí. OK výběru zachovalo úplnou dlouhou Unicode cestu; OK Preferences ji uložilo. |
| Maximalizace náhledu a návrat | Obě cesty se rozšířily, tlačítka zůstala bez překryvu vedle správných editací. Seznam dostal dodatečné místo. |

Release x64 sestavení prošlo s warnings-as-errors. Debug i Release resource/layout testy mají **2 458 kontrol**, při 96/120/144/192/240 font DPI: rozložení, standardní potvrzovací popisky, Tab, příslušnost browse tlačítek a ostatní dosavadní kontroly. Pokles proti minulé etapě je odstranění kontrol již nepoužívaných dynamických názvů potvrzovacích tlačítek. Release lifecycle sada má **2 353 kontrol**; nová nativní regrese ověřuje úplnou dlouhou Unicode cestu, její shellový 8.3 alias a odmítnutí neexistujícího výběru. GUI nebylo přesouváno mezi různými DPI monitory. Testovací host byl normálně zavřen; uživatelské instance zůstaly beze změny.

Průzkum nabídky výsledků a návrh implementace jsou v directory-sync-plugin-context-menu.md. Nabídka v pluginu zatím implementovaná není. Bez mazání složek, SMB testů a změn SDK.
CI doplnění: první běh nové regrese selhal při porovnání úplné cesty s kořenem fixture převzatým z TEMP, který může obsahovat krátké aliasy rodičů. Test nyní nejprve převede vlastní kořen na úplné jméno; produkční kód se nemění. Lokálně prošlo všech 2 353 lifecycle kontrol, vzdálený běh se opakuje na opraveném commitu.

## 2026-10-16 — ikonové filtry, řádek Compare a vytvoření složky

Compare (62 × 14 DU), krátká maska (45 DU) a šest voleb skenu jsou na jednom řádku; vodorovná nativní čára odděluje porovnávání od zobrazení výsledků. Výška seznamu zůstává 318 DU. Filtry jsou pět nativních ikonových přepínačů 20 × 18 DU namísto dlouhých textových checkboxů; Show je lokalizovaný popisek. Tvary =, ≠, obrys jednostranného souboru a obě šipky používají aktuální uživatelské barvy; názvy zůstávají přístupné asistivním technologiím. Tooltipy uvádějí význam a Alt+Q/D/N/G/H. Stisknutý stav znamená zapnutý filtr. Vlastní je pouze geometrická kresba symbolu, rám a interakce jsou nativní tlačítka Windows se současnou darkmodelib integrací.

První GUI pokus s BS_PUSHLIKE checkboxy odhalil, že knihovní checkbox painter ignoruje ikonu a vrací text. Finální přepínače používají nativní BS_ICON pushbutton s malým lokálním subclass stavem pro BM_GETCHECK/BM_SETCHECK a zachování stisknutého vzhledu. Společná knihovna ani hostitel se neměnily. Ověřeno po Compare, během revalidace a po změně filtrů v hostitelském tmavém schématu. Filtry fungovaly myší i Alt zkratkou a zůstaly ikonové; smazání výsledků při změně skenovacích voleb se nadále řídí předchozím potvrzením.

Výběr složek používá BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE, bez BIF_NONEWFOLDERBUTTON a v STA COM. Tlačítko Make New Folder je dokumentovaná vlastnost [BROWSEINFOW u Microsoftu](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/ns-shlobj_core-browseinfow), nikoli soukromý zásah do systému. V GUI Preferences → Recovery folder → … bylo pod izolovaným rodičem vytvořeno **New folder**, potvrzením vrácena přesná cesta do pole a uložena přes OK. Složka existuje a je prázdná. Pokus o přejmenování přes Computer Use nezměnil výchozí jméno; Unicode přejmenování se proto za ověřené nevydává. Vytvoření nové složky probíhá okamžitě v shellu, Cancel nevrací tento úkon zpět. Stejná možnost je i ve výběrech obou kořenů. Dosavadní Unicode expanze dlouhých jmen zůstává, má samostatnou regresi; vytvoření nové složky přímo v >260znakovém rodiči se netestovalo.

Evidence menu a schránky je v [záznamu kontextového menu](directory-sync-plugin-context-menu.md#2026-10-16--implementace-a-ověření-první-nabídky). Prošly produkční Release x64 warnings-as-errors, Debug/Release resource/font testy po 2 477 kontrolách a 2 354 lifecycle kontrol včetně 64 návrhových/path/hit-test kontrol. Font DPI 96/120/144/192/240 jsou geometrická měření, ne skutečné runtime změny monitoru. Bez mazání složek, SMB testů a změn SDK.
## 2026-10-16 — horní operace, dvojice Stop a seznam až dolů

Odstraněn text Show; všech pět ikonových filtrů má svůj lokalizovaný tooltip a přístupný název. Filtry leží mezi středem a pravou operační skupinou. Compare má sousední Stop (Alt+T), Synchronize… (Alt+Y) svůj Stop (Alt+P); Close je také nahoře vpravo. Masky a všech šest skenovacích voleb zůstávají na jednom řádku pod Compare. Seznam má 378 DU místo 318 DU a končí 6 DU nad nativním stavovým řádkem. Při zvětšování rostou seznam i kořeny, filtry se posunou o polovinu přírůstku šířky a Synchronize/Stop/Close zůstanou vpravo nahoře. Nastavení používá přesný popisek **Restore default colors**, zarovnaný k pravému okraji a posunutý pod mřížku barev; stále resetuje jen paletu aktuálního režimu.

Stop se aktivuje výhradně pro odpovídající práci: sken/obsah/revalidace návrhů nebo souhrnu u Compare; kopie/uchování/přepis/mazání včetně jejich revalidace u Synchronize. Mimo běh jsou oba neaktivní. Zaměření při zahájení jde na správný Stop, po konci zpět na seznam/Compare. Nativní regrese volá skutečné dialogové handlery a Busy: neaktivní protějšek nesmí nastavit cancel, aktivní jej nastaví. Dosavadní Stop/Close/uvolnění nad skutečnými NTFS fixture operacemi používají operační Stop a prošly.

Computer Use v Samandarinu 0.16 x64 s hostitelským tmavým schématem ověřil nové rozložení, seznam do spodní části, oba zakázané Stop při nečinnosti, tooltip Equal, Alt+Y a dostupnost pouze přepisu u dvou párových souborů. Bez jediného zvýrazněného řádku potvrzení správně zahrnulo jeden soubor / 6 hlavních bajtů a režim obnovy ON; Escape jej zrušil. Pravé kliknutí do prázdna zpřístupnilo oddělené Visible plan s čarami, zatímco změny návrhů byly neaktivní. Plan summary klávesou Y uvedlo 1 / 6 doprava. Maximalizace ponechala operace nahoře vpravo, filtry mezi středem a pravou stranou a rozšířila seznam až nad status bar. Souborové executory se v tomto GUI průchodu nespouštěly.

První kontrola nabídky zachytila chybějící nové jazykové řetězce; opraveno a jejich přítomnost v sestaveném resource nyní ověřuje regrese. Finální Release x64 warnings-as-errors prošel; Debug/Release resource/font sada má **2 251 kontrol** pro 96/120/144/192/240 DPI. Počet poklesl odstraněním zrušených ovládacích prvků; zůstávají měření textů, geometrie a mnemonik všech dialogů. Nejde o změnu monitoru za běhu. Release lifecycle: **2 355 kontrol**, včetně 77 návrhových/path/plan/hit-test kontrol a nové sady obou Stop. Regrese enginu: **539**, souborových operací: **766**. Bez mazání složek, SMB testů a změn SDK.
Finální Preferences přes Plugins Manager: nový úplný popisek Restore default colors je bez oříznutí, vpravo a níže. Alt+U obnovilo původní tmavou paletu (magenta vlastní Equal → výchozí zelená), Keep originals i cesta obnovy zůstaly stejné; Cancel změnu neuložil.

## 2026-10-16 — spojená tlačítka se zastavovací částí

Compare a Synchronize mají bez mezery připojenou menší pravou část 14 × 14 DU pouze s neutrálním křížkem. Jde o dvojici nativních tlačítek, nikoli zneužití standardní rozbalovací šipky split buttonu: obě části potřebují nezávislou aktivaci a keyboard focus. Nativní rámy, hover, focus a zakázaný stav zůstávají pod kontrolou Windows / stávající darkmode integrace. Křížek se generuje stejnou DPI cestou jako filtry, ale používá barvu textu ovládání hostitele, nezávislou na nastavitelné barvě mazání. Ikony se uvolňují při zániku náhledu a obnovují při změně tématu.

Přístupné názvy jsou Stop comparison / Stop synchronization; tooltipy doplňují účel a Alt+T / Alt+P. Start a Stop mají vlastní Tab zastávku pouze pokud jsou dostupné. Dosavadní Busy/StopControl/handler i zaměření zůstávají: běžící operaci lze přerušit pouze jejím křížkem. Po dokončení jsou oba zakázané. Compare i nadále pouze porovnává a levá část Synchronize otevře nabídku samostatných druhů běhu.

Release x64 warnings-as-errors prošel. Resource/font regrese Debug i Release: **2 268 kontrol**, 96/120/144/192/240 DPI, včetně dotýkajících se hran, stejné výšky, malé šířky, ikonového stylu, přístupných názvů a Tab. Release lifecycle: **2 355 kontrol**, včetně neaktivního/aktivního Stop při skenu, validaci, kopii, obnově, přepisu a mazání souborů. Computer Use v soukromém 0.16 x64 hostiteli s tmavým schématem ověřil kompaktní křížky bez textových nápisů, jejich odlišné přístupné názvy a oba neaktivní při nečinnosti. Kontrola font DPI není přechodem mezi monitory; nové GUI přenosy ani mazání se v této etapě neprováděly. Bez mazání složek, SMB testů a změn SDK.

## 2026-10-16 — nezávislé filtry, zachované výsledky a rozložení bez okrajů

Maska je úplně vlevo (45 DU); napravo od ní je šest voleb ve dvou kompaktních řádcích po třech. Compare, Synchronize a Close jsou v pravém sloupci nad sebou, obě operační tlačítka mají připojený křížek Stop. Seznam sahá od levého k pravému okraji klientské plochy a dole až ke skutečné horní hraně nativního status baru. Nezůstává 8 DU boční ani 6 DU spodní vnější mezera. Při změně velikosti se bere skutečná výška status baru; nejde jen o odhad z resource DU.

Každý ikonový filtr má vlastní bool stav; žádný příkaz nevypíná jiný. Kategorie se sjednocují a směry dodatečně omezují kopie. Oprava WM_PAINT znovu uplatní stisknutý vzhled z vlastního stavu, protože změna nativního default tlačítka / focusu může vnitřně zrušit pressed bit i bez BM_SETSTATE. Nativní regrese prochází všech 32 kombinací pěti přepínačů a změny focusu, dále kombinaci kategorií/směrů a viditelnost blokovaných položek.

Řádky s akcí Skip mají běžné sloupce v nastavené barvě Skip (výchozí šedá), zatímco sémantický sloupec State zachovává původní odstín smíchaný 1:1 s pozadím. Symbol Skip zůstává šedý. Ostatní operace nemění barvy. Vybraný řádek si ponechává nativní barvy výběru kvůli čitelnosti; po zrušení výběru je vidět tlumené provedení. Test barev zahrnuje světlé i tmavé pozadí. Při obnově seznamu se také skryje předchozí nativní infotip, aby po změně návrhu nezůstával viditelný starý popis akce.

Přepnutí kterékoliv ze šesti skenovacích voleb nyní zachová výsledek, snapshot a ruční návrhy. Nezmění zpětně Options použitá posledním porovnáním. Volby se převezmou až ve Start/Compare, včetně zdrojových kořenů původního Panel selection. Stavový řádek oznamuje čekající volby; návrhové i operační příkazy dál používají starý platný plán. Dialog Discard comparison results, jeho preference a ukládání ConfirmDiscardResults byly odstraněny. Stará uložená hodnota se ignoruje. Textová změna masky/cesty nadále zneplatňuje plán přímo.

Computer Use v soukromém `layout-options-20261016-host`, Samandarin 0.16 x64, místní dva párové NTFS soubory: Compare dal 2 výsledky. Alt+Q/D/Q/D postupně dalo **1 / 0 / 1 / 2 viditelné řádky** bez vzájemného resetování; po kliknutí do seznamu zůstaly zapnuté přepínače stisknuté. Ctrl+M změnilo jediný Unicode řádek na Skip. Alt+O zapnulo By content bez dialogu a beze ztráty dvou výsledků či Skip; status ukázal čekající volby. Po zrušení výběru byl text šedý a Different okrová ztlumená. Další Compare teprve uplatnilo By content (u shody Reason Binary content), vrátilo výchozí návrh Copy right a odstranilo indikaci čekajících voleb. Maximalizace zachovala pravý sloupec a seznam nalepený na oba boky i status bar. Žádná souborová operace nebyla spuštěna.

Release x64 warnings-as-errors prošel. Resource/font sada Debug i Release: **1 924 kontrol** při 96/120/144/192/240 DPI (úbytek odstraněného dialogu a preference); nejde o runtime přechod monitoru. Release lifecycle: **2 356 kontrol**, včetně nové sady skutečných nativních ovladačů pro kombinace filtrů, zachování snapshotu po šesti volbách, neměnné použité options, tlumené barvy a nulové okraje při dvou rozměrech okna. Dosavadní Stop/Close/uvolnění regrese prošly. Bez mazání složek, SMB testů a změn SDK.

## 2026-10-16 — kompaktní mřížka voleb porovnání

Šest checkboxů má společné začátky sloupců 91/164/248 DU a stejné šířky v rámci každého sloupce. Rozteč řádků klesla z 18 na 13 DU; při nativní výšce 12 DU zůstává jen 1 DU mezera, stejně jako u kompaktních voleb jádra Salamanderu. Chování voleb se nemění.

Release x64 warnings-as-errors a resource/font regrese Debug i Release prošly: **1 925 kontrol** v každé konfiguraci, včetně zarovnání všech tří sloupců, rozteče řádků a měření popisků/překryvů při 96/120/144/192/240 DPI. Jde o kontrolu sestavených resources a fontových metrik, nikoli nové ruční GUI ověření nebo změnu monitoru za běhu.

## 2026-10-16 — nativní stav filtrů, překreslení a dva řádky

Dosavadní přepínače nahrazovaly stav checkboxu trvale stisknutým stavem obyčejných tlačítek a měnily jej i ve WM_PAINT. To kolidovalo s nativní obsluhou focusu a překreslením. Pět filtrů nyní používá BS_CHECKBOX | BS_PUSHLIKE | BS_ICON; samostatný model se přepíná pouze v BN_CLICKED a nativní BST_CHECKED řídí vzhled i přístupnost. BST_PUSHED zůstává pouze přechodným stavem fyzického stisku. Odstraněn FilterButton subclass a veškeré vynucování BM_SETSTATE.

V tmavém schématu helper jádra vnucoval i push-like tlačítkům kreslení běžného checkboxu. Plugin pouze u svých pěti filtrů odstraní tento subclass přes existující darkmodelib API a v WM_CTLCOLORBTN ponechá nativní themed painter; obecná obsluha ostatních ovládání zůstává. Bez změny jádra, sdíleného helperu a SDK. Při resize se ovládání přesunou bez průběžného překreslování, poté se zneplatní i staré oblasti a všechny děti. Náhled používá WS_CLIPCHILDREN.

Filtry mají horní řádek = / ≠ / jednostranné a dolní → / ←. Compare a Synchronize zůstávají pod sebou se samostatnými Stop částmi, Close je vedle nich napravo. Začátek seznamu se posunul z 82 na 70 DU a seznam získal dalších 12 DU výšky.

Release x64 warnings-as-errors prošel. Resource/font regrese Debug i Release: **1 926 kontrol** při 96/120/144/192/240 DPI. Lifecycle sada: **2 356 kontrol**; všech 32 kombinací se nyní dosahuje přes BM_CLICK doručené skutečnému dialogovému handleru, ověřuje se model, nativní checked stav i počet viditelných řádků, focus/paint a tři cykly vše OFF → vše ON. Dřívější test nastavoval požadované stavy přímo a nedokazoval funkčnost klikání.

Computer Use, soukromý 0.16 x64 s tmavým schématem: porovnání dvou syntetických souborů; postupné vypnutí všech pěti filtrů, opětovné zapnutí myší a Alt+Q/D/N/G/H, návrat obou výsledků a všech aktivních ikon. Dvě maximalizace a návraty na původní velikost zachovaly výsledky, zarovnání i čisté pozadí bez zbytků tlačítek. Ve zvětšeném okně vypnutí Different myší a zapnutí mezerníkem obnovilo původní dva řádky. Přesunuté Close zavřelo náhled. Pokus o tažení mimo bounds byl helperem odmítnut; další tah velikost nezměnil, takže tato evidence potvrzuje resize maximalizací/obnovením, nikoli plynulý resize tahem. Světlé schéma nebylo v této etapě znovu ručně ověřeno. Bez nových GUI souborových operací, mazání složek, SMB testů a změn SDK.

## 2026-10-16 — Settings/Close v mřížce, světlé filtry a resize tahem

Settings… je nad Close ve stejném pravém sloupci (546 DU), s horními hranami 26/46 DU shodnými s Compare/Synchronize. Obě tlačítka mají shodnou šířku 56 DU a drží pravý okraj při resize. Settings otevírá stávající Preferences; po návratu obnoví theming, ikony a šířku Action a překreslí seznam bez nového porovnání nebo změny filtrů/ručních návrhů. Během běhu je tlačítko i jeho handler neaktivní. Předchozí požadavek na dostupnost pouze přes Plugins Manager je tímto novým zadáním nahrazen; tato cesta nadále funguje.

Computer Use v odděleném Samandarinu 0.16 x64 s hlavním světlým schématem, dva místní syntetické soubory equal.txt / same-漢字.txt:

- Compare ukázalo dvě položky; postupné vypnutí všech pěti filtrů myší a Alt+D/N/G/H vedlo na nula viditelných. Opětovné zapnutí Alt+Q a myší vrátilo oba původní řádky a všech pět aktivních ikon. Zapnutý native checked vzhled, vypnutý vzhled i focus byly čitelné.
- Skutečné sky.drag za pravý resize rám změnilo šířku snímku dialogu 917 → 1 223 → 917 → 1 463 → 1 051 px. Zvětšování i zmenšování proběhlo bez maximalizace. Test zahrnul všechny filtry ON i OFF, přepínání mezi tahy a zachování výsledků/návrhů. Po každém úspěšném tahu bylo pozadí čisté, žádné zbytky původních hran/ikon; oba řádky filtrů a Settings/Close zůstaly zarovnané. Některé pokusy netrefily úzký rám a velikost nezměnily; nejsou započítány jako důkaz resize. Jde o snímky po dokončených tazích, nikoli záznam každého snímku animace.
- Settings otevřelo stejné Preferences. Show action labels ON + OK ihned rozšířilo Action a zobrazilo popisky při zachování dvou výsledků i stavu filtrů. Opětovné otevření zachovalo ON, Cancel vrátilo náhled se stejnými výsledky. Přesunuté Close zavřelo náhled. Escape cílený automation helperem na zakázaného vlastníka dialog nezavřel; toto není považováno za ověření Escape aktivního Preferences.

Produkční Release x64 warnings-as-errors prošel. Debug/Release compiled-resource/font sada: **1 959 kontrol** při 96/120/144/192/240 DPI, včetně zarovnání Settings/Close, Tab a celých popisků. Release lifecycle: **2 356 kontrol**, včetně Settings zakázaného při všech běžících režimech a předchozích 32 kombinací skutečně klikaných filtrů. Další chyba filtrů při tomto světlém/drag ověření nenalezena. Nová GUI souborová operace se neprováděla. Bez mazání složek, SMB testů a změn SDK/jádra. Uživatelské instance nebyly měněny.
## 2026-10-16 — čtvercové filtry a automatické zalamování

Filtry mají nativní výšku 14 DU shodnou s Compare/Synchronize/Settings/Close. Runtime používá skutečnou výšku Compare v pixelech také jako šířku filtru; shodné DU šířky/výšky samy nemusí po mapování fontu vytvořit přesný čtverec. Mezery se mapují z 4 DU. Pokud se před Compare vejde všech pět tlačítek i mezera, jsou v jednom řádku zarovnaném s Compare. Jinak zůstávají v mřížce 3 + 2, s druhým řádkem zarovnaným se Synchronize. Rozhoduje dostupná šířka při aktuálních metrikách, nikoli pevný práh v pixelech. Tlačítka se pouze přemisťují; jejich HWND, focus, tooltipy, zkratky i checked model zůstávají stejné.

Release x64 warnings-as-errors, Debug/Release resource/font sada **1 959 kontrol** při 96/120/144/192/240 DPI a Release lifecycle **2 356 kontrol** prošly. Native-control regrese navíc dvakrát přechází mezi šířkami 900/1200 a kontroluje skutečné čtvercové pixelové rozměry, řádky, odstupy, volné místo před operační skupinou a zachované aktivní filtry. Starší kontrola 32 klikaných kombinací zůstává.

Computer Use, soukromý 0.16 x64 se světlým schématem: výchozí snímek široký 917 px má dva řádky a filtry stejné výšky jako Compare. Po Compare byly dva výsledky; Alt+Q skrylo shodu. Tažení pravého rámu na šířku 1223 px přeskládalo všech pět filtrů do jednoho řádku, vypnuté Equal zůstalo vypnuté a viditelný zůstal pouze rozdílný soubor. Alt+Q v širokém okně vrátilo oba výsledky. Zúžení tahem zpět na 917 px obnovilo dva řádky se všemi filtry zapnutými, beze zbytků ikon na původních místech. Uživatelské instance nebyly změněny, žádný GUI zápisový příkaz neběžel. Tato etapa znovu ručně neověřovala tmavý režim ani přechod DPI mezi monitory. Bez mazání složek, SMB testů a změn SDK/jádra.
## 2026-10-16 — tmavé přeuspořádání a hranice zalamování při DPI

V soukromém Samandarinu 0.16 x64 se schématem Windows Dark Mode bylo přes Computer Use ověřeno Compare nad dvěma syntetickými soubory. Alt+Q vypnulo Equal a ponechalo jeden řádek. Skutečné rozšíření tahem 917 → 1223 px změnilo dvě řady filtrů na jednu, při zachování vypnutého Equal, focusu i jediného výsledku. Kliknutí na Copy to right skrylo poslední výsledek, Space jej vrátilo. Zúžení tahem zpět na 917 px obnovilo dvě řady se stejnými stavy; kliknutí na Equal vrátilo všechny aktivní filtry a oba původní výsledky. Barvy/ikony i rozlišení zapnutí, vypnutí a focusu zůstaly čitelné, po úspěšných tazích nezůstaly zbytky původních tlačítek. Jeden pokus netrefil rám a velikost nezměnil; do důkazu resize se nezapočítává. Náhled i vlastní hostitel byly zavřeny, původní tři uživatelské instance zůstaly spuštěné.

Výpočet FilterBounds byl beze změny logiky přesunut do interního previewlayout.h, aby test použil stejný kód jako plugin. Test čte skutečný zkompilovaný lang.rc, pro každý z 96/120/144/192/240 DPI vytvoří předepsaný 8pt MS Shell Dlg font a z jeho metrik mapuje skutečné rozměry/pozice tlačítek. Projde každou pixelovou šířku od základu do rozšíření odpovídajícího 300 px při 96 DPI: ověří právě jeden přechod do jedné řady, čtvercové rozměry, zarovnání řad, žádné překryvy a rezervu před Compare, místo pro škálovanou ikonu a návrat na základní dvě řady. Debug i Release: **1 974 resource/layout kontrol**, všechny prošly. Produkční Release x64 warnings-as-errors a **2 356 Release lifecycle kontrol** prošly. Funkční chyba nenalezena; produkční změna je pouze přesun téhož výpočtu do interního headeru pro sdílení s testem, bez změny SDK/jádra.

**Meze důkazu:** různé DPI zde znamenají explicitní GDI fontové metriky a test přesného produkčního výpočtu. Nejde o vykreslení celého hostitele při pěti systémových DPI, změnu DPI otevřeného okna ani přesun mezi monitory. Předchozí záznam této vzdálené relace uvádí zakázanou změnu měřítka displeje; nastavení systému nebylo nyní měněno ani obcházeno. Aktuální efektivní DPI GUI nebylo změřeno a z velikosti screenshotů se neodvozuje. Skutečné další systémové DPI a monitorové přechody zůstávají pro vhodnou relaci / další počítač. Bez GUI zápisových příkazů, mazání složek, SMB testů a změn SDK.
## 2026-10-16 — zobrazení podle nastavení panelů hostitele

Sloupce velikostí používají `SALCFG_SIZEFORMAT` a stejné volání jako `InternalGetSize` v `src/salamdr4.cpp`: pro bajty `NumberToStr` (stejný text jako interní `NumberToStr2`), pro celé KB `PrintDiskSize` s režimem 3 a pro smíšené jednotky s režimem 0. SDK metody v `src/zip.cpp` delegují přímo na jádro. `src/salamdr1.cpp` používá oddělovač tisíců hostitele; `src/salamdr6.cpp` používá jeho regionální desetinný oddělovač, jednotky a zaokrouhlování. Nezavádíme vlastní číselné formátování. Velikosti jsou zarovnané doprava; souhrny výslovně označené jako bajty a přenosové čítače zůstávají přesnými bajty.

`SALCFG_FILENAMEFORMAT` určuje zobrazení každé části relativní cesty. U bezeztrátově reprezentovatelných názvů plugin volá existující SDK `AlterFileName`; kvůli jeho ACP kontraktu nejprve ověří převod bez best-fit náhrad. Pro Unicode mimo ACP používá lokální adaptaci pravidel `AlterFileName` z `src/salamdr2.cpp` a přímo sdílený helper jádra `src/filenamecase.h`, včetně ochrany před růstem UTF-8 mapování. Žádná změna SDK ani hostitele. Výchozí/as-is, malá/velká písmena, počáteční velká písmena, styl DOS 8.3, soubory malými/adresáře velkými a jméno s malou příponou používají stejné významy 1–7. Výsledná podoba se používá pouze při vykreslení; originální cesty, kopírování do schránky, rozhodování a manifesty zůstávají beze změny. Nastavení se čte při otevření a oznámení změny konfigurace/Windows nastavení.

Release x64 sestavení s kontrolou varování prošlo. Nativní lifecycle regrese prošla 2 357 kontrolami, včetně nového testu skutečných pomocných funkcí pro 22 případů názvů: všech sedm režimů, DOS hranice, dlouhá cesta nad 260 znaky, česká diakritika, CJK, emoji, odlišení rodičovského adresáře od souboru a původní zápis cest ve schránce. Porovnání číselného formátování je ověřeno trasováním stejných rutin jádra; nový interaktivní GUI průchod přepínáním všech voleb Show sizes / Show names a skutečná změna systémového locale se v této etapě neprováděly.

Aktualizovaný nepodepsaný x64 ZIP: `C:\Samandarin\builds\dirsync-distribution\panel-format-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `cb03169285d969dd188e0a5d28ba06440d8b581257d4e10ca892d14ab0ef936b`. Rozbalením ověřeno všech 11 hashů manifestu a přesný inventář 12 souborů včetně obou SLG. Bez SMB testů, mazání adresářů a změn SDK.

## 2026-10-16 — GUI porovnání Show sizes a Show names

Computer Use ověřil produkční plugin z commitu `836a1311` v soukromém hostiteli Samandarin 0.16 x64 proti jeho hlavním souborovým panelům. SPL SHA-256: `985191e7da55d3887c48c24be773fd22214d10180bca5d7e5f8a21f7ed3e6061`. Nastavení se měnilo skutečně přes **Options → Configuration → Panels** a po OK následovalo znovuotevření pluginu a Compare. Nebyl spuštěn žádný příkaz Synchronize ani souborová operace pluginu. Test byl ve světlém režimu; regionální nastavení en-US zůstalo beze změny.

Izolovaná sada `C:\Samandarin\builds\dirsync-panel-format-gui-20261016` má shodné levé/pravé stromy, každý se sedmi syntetickými soubory a dvěma podadresáři. Názvy zahrnují `FOO.TXT`, `hello WORLD.TxT`, `LONGERTHAN8.TXT`, `MiXeD.Name.TxT`, `ŽLUŤOUČKÝ-漢字-😀.TXT`, `Large.BIN`, `MiXeD.Dir\NeStEd.TxT` a prázdný `EMPTY.DIR`. Soubory jsou řídké; velikost největšího je 5 368 709 243 bajtů a při porovnání nebyl čten jeho obsah. Ostatní hlavní testované velikosti jsou 0, 1, 1 023, 1 025 a 1 234 567 bajtů; vnořený soubor má 123 456 bajtů.

### Velikosti

| Bajty na disku | In Bytes: panel i plugin | In Kilo Bytes: panel i plugin | In short format: panel i plugin |
| --- | --- | --- | --- |
| 0 | `0` | `0 KB` | `0 B` |
| 1 | `1` | `1 KB` | `1 B` |
| 1 023 | `1,023` | `1 KB` | `1023 B` |
| 1 025 | `1,025` | `2 KB` | `1 KB` |
| 1 234 567 | `1,234,567` | `1,206 KB` | `1.18 MB` |
| 5 368 709 243 | `5,368,709,243` | `5,242,881 KB` | `5 GB` |

Všechny tři režimy se shodují v obou velikostních sloupcích pluginu. Ověřeno zaokrouhlování nahoru v celých KB, smíšené B/KB/MB/GB, čárky mezi tisíci a desetinná tečka. Hlavní panely byly odečteny ze screenshotů; plugin také ze skutečné přístupnosti ListView, která poskytuje celý text buněk i při úzkém sloupci. Výchozí úzký sloupec v pluginu velký bajtový údaj zkrátí výpustkou; je resizovatelný. Nejde o ztrátu či změnu číselné hodnoty. Nebyla provedena změna systémového regionu ani ověření všech kombinací locale.

### Názvy

Aktuální GUI nabízí šest voleb, nikoli všech sedm historických hodnot SDK. Zdrojově to potvrzuje šestipoložková nabídka v `CCfgPagePanels::Transfer` / `src/dialogs5.cpp`; z GUI byly všechny níže uvedené volby skutečně vybrány.

| Volba Show names | SDK formát | Příklad souboru (panel = plugin) | Příklad adresáře (panel = plugin) |
| --- | --- | --- | --- |
| As they are stored on the disk | 4 | `hello WORLD.TxT` | `EMPTY.DIR` |
| In partially mixed case if they are in DOS format | 5 | `FOO.TXT` → `Foo.txt`; `LONGERTHAN8.TXT` zůstává | `Empty.dir` |
| In lower case; directories in upper case | 6 | `hello world.txt` | `MIXED.DIR` |
| In partially mixed case | 7 | `Hello World.txt`; `Mixed.name.txt` | `Mixed.dir` |
| In lower case | 2 | `hello world.txt` | `empty.dir` |
| In upper case | 3 | `HELLO WORLD.TXT` | `EMPTY.DIR` |

Česká diakritika se podle režimu mění (`ŽLUŤOUČKÝ` / `Žluťoučký` / `žluťoučký`), CJK a emoji jsou zachované. V pluginu se správně formátuje i adresářová část vnořené cesty, např. `MIXED.DIR\nested.txt` ve formátu 6. Po opětovném otevření plugin načte novou volbu hostitele bez jeho restartu. V každém běhu zůstaly výsledky shodné a bez blokování. Nejde o úplný kartézský součin 3 × 6: sedm průchodů pokrylo tři velikostní režimy a šest názvových režimů samostatně. Starší formát 1 není v nynějším GUI volitelný; jeho předchozí nativní ověření zůstává oddělené od tohoto GUI testu.

Textová evidence sedmi dokončených porovnání je v `observations.json` vedle sady; `baseline.json` zachycuje původní soubory. Po ukončení bylo všech 14 souborů beze změny přesného názvu, velikosti, mtime a atributů; nevznikl žádný další soubor ve stromech. Obsah velkých řídkých souborů se nehashoval. Prázdné adresáře zůstaly zachované. Příkazy pro kopírování do schránky se v tomto průchodu neopakovaly; jejich nativní a dřívější GUI evidence tím není rozšiřována.

Výsledek: nebyla nalezena odchylka formátování vyžadující opravu produkčního kódu. Doplněn aktuální návod a upřesněn počet skutečně dostupných názvových režimů. Bez změn SDK, mazání adresářů a SMB testů.

Provozní poznámka k testovací sadě: konfigurace RegFile musí zachovat podporované textové kódování; UTF-16 kopie byla parserem odmítnuta a nahrazena UTF-8 bez BOM. Při skrytém startu byly panely prázdně vykreslené; po běžném interaktivním restartu bylo hlavní okno funkční. Příčina v hostiteli se zde neměnila ani nedovozuje jako závada pluginu. Launcher navíc otevřel další instalovanou instanci; ta byla po výslovném schválení uživatele ukončena. Dvě původní uživatelské instance zůstaly otevřené, soukromý hostitel byl ukončen běžně.
