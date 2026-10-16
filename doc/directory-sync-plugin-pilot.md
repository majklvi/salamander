# Větší místní NTFS pilot — 16. října 2026

## Prostředí a data

Computer Use, soukromý Samandarin 0.16 Release x64 s vlastní RegFile konfigurací, Windows Dark Mode (experimental). Bez změn SDK/hostitele, SMB a mazání složek. Základ tvoří **kopie 5 809 sledovaných souborů skutečného stromu `src`**, 81 117 229 B, z pracovního stromu `c1dbd38d`. Nejde o osobní dokumenty uživatele. Binární i textová data, různé přípony a adresářová struktura jsou převzaté ze skutečného projektu; rozdíly jsou řízené pro předem ověřitelný výsledek.

Fixture: `C:\Samandarin\builds\dirsync-large-pilot-20261016`. `left` a `right` jsou nezávislé fyzické kopie, nikoli hardlinky. `recovery-漢字` leží mimo oba stromy. Přidány tři 128MiB soubory (shoda, nová kopie, přepis), Unicode jména a víceúrovňová dlouhá Unicode cesta se souborem `soubor-😀.txt`; nové Unicode soubory mají ADS `:漢字`. Velký původní cíl přepisu má také ADS `:old-only`, který v novém zdroji není.

| Výchozí kategorie | Počet |
| --- | ---: |
| Shodné páry | 5 660 |
| Nové levostranné soubory | 103 |
| Existující pravé cíle s novějším levým zdrojem | 51 |
| Další malé levostranné položky pro ruční návrhy | 10 |
| Pravostranné kontrolní soubory | 25 |
| Celkem výsledných souborových řádků | 5 849 |

Vlevo je 5 824 souborů / 483 770 573 B, vpravo 5 736 souborů / 337 885 210 B hlavních dat. Prázdná pravá složka `_pilot\empty-retain` se musí zachovat. Subfolders, By content a Asymmetric ON, Ignore date, Panel selection a Empty folders OFF; maska `*.*`. Žádný reparse bod ani síťový kořen není součástí sady.

## Omezení přístupnosti velkého seznamu

První Compare dokončilo **5 849 viditelných řádků, 0 blokovaných**. Následné ovládání vykazovalo výrazně opožděné vstupy a úplný požadavek na přístupnostní strom vrátil null. Process.Responding střídal true/false, rostl CPU čas a pracovní paměť. Diagnostický snímek hlavního vlákna `uia-stack.txt` vede přes UIAutomationCore, SendMessageW a COMCTL32. Po dokončení nákladného průchodu se otevřely opožděně vyvolané Preferences, které znovu reagovaly. Náhled byl běžně uzavřen a znovu otevřen; proces nebyl násilně ukončen.

Tento výsledek odpovídá [dřívějšímu reproduktoru standardních Win32 seznamů](directory-sync-plugin-ui-automation.md). Neprokazuje deadlock kopírovacího workeru ani přesnou interní příčinu UIA. Během této fáze nebyly provedeny zápisy do porovnávaných stromů. Není to důkaz svižného běžného ručního ovládání 5 849 zobrazených řádků bez automatizace; tento případ zůstává k nezávislému uživatelskému ověření. Přístupnost nebyla vypnuta a nevznikl vlastní provider.

Pro dokončení pilotu byl filtr **Equal vypnut ještě před novým Compare**. Celá sada se opět skenovala a porovnávala, ale seznam obsahoval pouze **189 rozdílů / 5 660 skrytých shod**. Tato varianta byla prostřednictvím Computer Use ovladatelná. Skrývá se pouze zobrazení, nikoli vstupní data skenu.

## Provedený průchod

1. Z deseti malých doplňkových položek vybrány `_pilot\00-skip-00.txt` a `00-skip-01.txt`, druhá přes Shift+Down. Ctrl+M: **2 změněné návrhy, 0 vynechaných**. Zbývajících osm položek zůstalo běžnými novými kopiemi. Vypnutí směru doprava při skrytých shodách ukázalo 27 řádků / 5 822 skrytých; opětovné zapnutí obnovilo 189 / 5 660. Ruční Skip zůstal zachovaný.
2. **Copy visible new files** nabídlo **111 souborů / 145 886 652 B**, vynechalo 76 jiných návrhů (51 přepisů a 25 pravostranných souborů). Výsledek: **111 copied and verified, 0 chyb/přerušení, 0 nezahájených**. Dva ruční Skip cíle nevznikly. Všech 111 nových hlavních proudů následně odpovídalo výchozím hashům zdrojů. Součástí je 128MiB soubor a dlouhá Unicode cesta s ADS.
3. Nové Compare: **78 rozdílů / 5 771 skrytých shod**. Ruční Skip se správně změnil zpět na výchozí návrh nové kopie; nový plán vyžaduje opětovnou kontrolu. **Overwrite visible files** zařadilo právě **51 souborů / 135 271 342 B nových a 135 272 342 B původních hlavních dat**, 27 jiných návrhů vynechalo. Keep originals ON, samostatná Unicode obnova. Výsledek: **51 replaced and verified, obnova 51/51, 0 chyb/přerušení, 0 nezahájených**.
4. Závěrečné Compare: **27 rozdílů / 5 822 skrytých shod, 0 blokovaných**. Zůstaly dva záměrně nepřenesené soubory a 25 kontrolních pravostranných souborů. **Mazání souborů tento pilot nespouštěl**; tyto soubory slouží jako kontrola, že samostatné příkazy kopií/přepisů neprovedou jinou operaci. Prázdná složka zůstala zachovaná. Provedené mazání a ruční obnova jsou doloženy předchozím malým [GUI průchodem](directory-sync-plugin-workflow.md#2026-10-16--aktuální-průchod-včetně-ruční-obnovy).

## Kontrola disku a nalezená oprava

**11 842 následných kontrol prošlo, 0 selhání.** Evidence `results.json` a `result-summary.json` zahrnuje:

- všechny původní levé soubory: SHA-256 hlavního proudu, mtime a atributy;
- všechny očekávané pravé výsledky, včetně nezměněných shod a kontrolních pravostranných souborů: hlavní hash, mtime a atributy; všech 51 přepisů navíc zachovalo cílovou DACL;
- obsah i přesnou sadu ADS u doplňkových Unicode/dlouhých cest a velkých nových/přepisovaných souborů; starý ADS nepřežil v přepsaném cíli a zůstal v obnově;
- všech 51 originálů obnovy proti výchozím pravým hashům a mtime, úspěšné výsledky saved a dokončení manifestu i overwrite.jsonl;
- nepřítomnost obou Skip cílů, zachování prázdné složky a přesný inventář **5 847 pravých souborů** bez zbytkového stagingu.

Přípravný a ověřovací skript i původní inventář jsou uložené vedle fixture/mimo Git: `C:\Samandarin\builds\Prepare-DirsyncLargePilot.ps1`, `Verify-DirsyncLargePilot.ps1`, `inventory.json`, `baseline.json`. První pomocná kontrola musela sjednotit `/` z Git inventáře s `\` v inventáři Windows; pro výpis ADS dlouhé cesty byl doplněn prefix `\\?\`. Nešlo o chyby kopírování pluginu.

Průchod odhalil drobný problém zobrazení: **infotip akce mohl zůstat topmost nad potvrzením nebo výsledkem operace**. `Busy` nyní před prací infotip schová a deaktivuje; po návratu do nečinného stavu jej znovu povolí. Nemění se datový adaptér, rozsah návrhů ani formáty obnovy. Opravené produkční SPL má SHA-256 `402E038ACB9A847004121C3E0539F49612FDB9B6C2D83038C3E58BD563902D21`. Původní dávky byly provedeny na předchozím SPL `45F5161B1E14AA55D469403CC968CCCAD909F29FADE831D65E694C1F985C4FDD`; po opravě nebyly celé dávky znovu spouštěny. Malý následný GUI průchod na opravené binárce otevřel potvrzení přepisu po zaměření akce bez starého infotipu, Cancel zachoval plán a soubor.

Release x64 sestavení s warnings-as-errors prošlo; prošlo **2 356 lifecycle** a **1 974 resource/layout** regresních kontrol. Poslední jsou testy fontových metrik 96–240 DPI, nikoli nový běh na různých systémových DPI. Soukromý hostitel byl běžně zavřen a jeho původní konfigurace obnovena; tři uživatelské instance zůstaly spuštěné.

## Závěr pro běžné testování

Větší místní datový průchod kopií a přepisů s obnovou prošel bez chyby dat. Nejde o dlouhodobý provoz, terabajtovou zátěž ani úplné ověření všech typů uživatelských dat. Pro pilot je praktické skrýt shody a před každou samostatnou operací zkontrolovat čerstvý plán. Známé prodlevy velkého UIA seznamu zůstávají výslovným omezením této automatizované relace a vhodným bodem nezávislého ručního ověření. Bez mazání složek, SMB testů a změn SDK.
