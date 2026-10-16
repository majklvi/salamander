# Directory Synchronization — aktuální návod

Stav 16. října 2026, vývojová verze 0.1 pro současný **Samandarin 0.16**, bez rozšíření SDK. Tento dokument je výchozí přehled aktuálního chování. Ostatní dokumenty zachovávají návrh a datovanou evidenci jednotlivých etap; jejich tehdejší omezení a počty testů nepředstavují dnešní stav.

## Instalace a spuštění

Použijte plugin ze stejného sestavení a pro stejnou architekturu jako hostitel (x64 hostitel vyžaduje x64 SPL). Vývojový výstup obsahuje:

```text
plugins/dirsync/dirsync.spl
plugins/dirsync/lang/english.slg
```

Zavřete instance používající starou verzi pluginu, zkopírujte celou složku `dirsync` do `plugins` vedle programu a ponechte uvedenou strukturu. V Samandarinu otevřete **Plugins → Plugins Manager → Add…**, vyberte `dirsync.spl` a ověřte položku Directory Synchronization. Pokud ji distribuce už zaregistrovala, znovu ji nepřidávejte. Plugin spusťte z nabídky **Plugins → Directory Synchronization → Directory synchronization…**. Obě porovnávané složky musí být fyzické Windows cesty; archivy a virtuální pluginové systémy nejsou vstupem.

Při sestavení ze zdrojů použijte projekt `src/plugins/dirsync/vcxproj/dirsync.vcxproj`; jazykový modul je jeho závislost. Podrobný build postup je v [implementačním záznamu](directory-sync-plugin-implementation.md#sestavení-a-testy). [Distribuční ověření](directory-sync-plugin-distribution.md) dokončilo rozbalení ZIP do čisté konfigurace 0.16 x64, automatickou registraci a dva skutečné restarty s ON/OFF a Unicode cestou obnovy. Jde o ruční portable instalaci pluginu, nikoli sestavení celého Inno instalátoru.

## Vzhled a nápověda

Maska má krátké pole vedle všech šesti voleb skenu; delší masky lze stále zadat a vodorovně posouvat. Pevná tolerance času 3 sekundy se nenastavuje. Vysvětlení obnovy a rozsahu příkazů jsou v tooltipech a nápovědě F1; potvrzení přepisu a mazání vždy uvádí rozsah i režim obnovy. Zvýraznění řádků není omezením operačních příkazů.

Konfiguraci vzhledu **Appearance** otevřete výhradně přes předvolby pluginu ve **Plugins Manager → Configure…** (Preferences podle jazyka hostitele). Náhled synchronizace nemá vlastní tlačítko konfigurace. **Show action labels** zapne text vedle symbolů a rozšíří sloupec Action; výchozí je vypnuto. Všech sedm významů je vidět pod sebou: shoda, rozdíl, směr doprava/doleva, Skip, mazání a rozhodnutí/blokování. Každý řádek má barevné tlačítko Salamanderu a ukázku symbolu. Světlý/tmavý režim se automaticky řídí hlavním nastavením Salamanderu; v pluginu se nepřepíná. **Restore defaults** obnoví barvy aktuálního režimu. **OK** uloží popisky a upravované barvy; **Cancel/Escape** rozepsané změny zahodí. Nastavení druhého režimu zůstane zachované. Volby se pamatují přes běžnou konfiguraci hostitele stejně jako uchování originálů. Tvar symbolu rozlišuje význam nezávisle na barvě a zvýrazněné položky používají barvy výběru.

Dialog lze ovládat Tab/Shift+Tab. Alt+L přepíná popisky; Alt+E/D/R/F/S/T/B otevře volbu barvy příslušného řádku. Alt+U obnoví výchozí barvy. Enter uloží a Escape zruší.

## Běžný postup

1. Otevřete požadované kořeny v levém a pravém panelu a spusťte plugin. Zkontrolujte obě cesty vedle sebe. **Panel selection** používá sjednocený původní výběr z panelů a uzamkne kořeny; nejde o zvýraznění v náhledu.
2. Nastavte masku a volby skenu. Výchozí `*.*` zahrnuje všechny názvy. Více masek oddělujte mezerou nebo středníkem, názvy s mezerou uvozovkami; za `|` patří výluky (např. `*.txt | *.tmp`). Subfolders zahrnuje podstromy. **Compare** jen čte a sestaví návrhy. Tolerance časů je 3 sekundy včetně; By content porovnává hlavní datový proud, nikoli ADS, a nenahrazuje pravidlo přednosti novějšího času. Při Ignore date nebo stejně starých odlišných datech zvolte směr ručně.
3. Prohlédněte sloupce Action, State a Reason. Ve sloupci Action jsou výchozí pouze vycentrované symboly; textové popisy lze zapnout v Appearance. Text významu zůstává dostupný přes přístupnost, tooltip akce a stavový řádek při zaměření položky klávesnicí. Sloupec State má symbol i popis. Ve výchozí paletě zelená šipka znamená kopii doprava, modrá doleva; zelené `=` shodu, okrové `≠` neshodu/jednostrannost, šedé pozastavení Skip, červené `×` mazání a `!` nutnost rozhodnutí či blokování. Zvýrazněné řádky používají barvu výběru. Barva sama neurčuje operaci.
4. Chcete-li změnit návrh, zvýrazněte řádky, vyberte **Proposal** a použijte **Apply to highlighted rows**. Pro vynechání nastavte Skip. Filtry Equal/Different/Single-sided a směrové filtry určují viditelnost. Blokované a konfliktní řádky zůstávají viditelné.
5. Zkontrolujte **Plan summary**. Operační příkazy zpracují **všechny viditelné způsobilé návrhy**, nikoli jen zvýrazněné řádky. Skryté a Skip se neprovedou. Počty kopií v plánu zahrnují nové i existující cíle; konkrétní potvrzení ukáže jen soubory způsobilé pro daný příkaz. Jiné viditelné návrhy jsou uvedeny jako omitted.
6. Zvolte příslušné samostatné tlačítko a zkontrolujte potvrzení. **Copy visible new files…** vytváří jen chybějící cíle. **Overwrite visible files…** přepisuje jen existující cílové soubory. **Delete visible files…** odstraňuje jen pravostranné soubory chybějící vlevo při Asymmetric. Složky se nemažou. Žádné tlačítko neprovádí všechny druhy operací najednou.
7. Přečtěte výsledek a spusťte nové **Compare** před další operací. Nové porovnání sestaví návrhy znovu, proto je třeba ruční Skip znovu použít. Stavový řádek ukazuje průběh a počty; **Stop** přeruší další práci, **Close** počká na ukončení workeru. Hotové operace zůstávají provedené. Tab přechází mezi prvky; v tabulkových souhrnech Ctrl+C kopíruje vybranou hodnotu a Ctrl+Shift+C celý souhrn, obojí je také v kontextovém menu.

Asymmetric považuje levou stranu za zdroj: navrhuje nové a způsobilé novější levé soubory doprava a pravostranné položky chybějící vlevo k odstranění. Novější pravý soubor se automaticky nepřepisuje. Bez historie nelze odvodit, zda chybějící soubor byl úmyslně odstraněn nebo nově vznikl na druhé straně.

## Uchování originálů a ruční obnova

**Keep originals** je výchozí zapnutá volba. Před přepisy a mazáním se nejprve uloží a ověří všechny zařazené originály. Zadejte existující **Recovery folder** mimo oba stromy, která není ani jejich předkem. Obnova potřebuje místo na původní data včetně ADS; staging navíc potřebuje místo pro právě přenášený soubor. Při chybě zálohování se destruktivní část nespustí. Volbu lze vypnout; potvrzení pak uvádí OFF, původní data ani ADS se nově neuchovají a Koš se nepoužívá. Starší zálohy se automaticky nemažou. Volba a Unicode cesta obnovy se pamatují společně pro plugin, nikoli pro jednotlivé dvojice stromů; konfiguraci fyzicky ukládá hostitel.

**Save visible originals…** samostatně uloží původní cíle viditelných návrhů přepisu. Neprovede přepis ani mazání a nezastupuje povinnou čerstvou obnovu při pozdějším běhu s ON.

Ruční obnova:

1. V obnovovací složce vyhledejte příslušný `pair-…/run-{GUID}` podle času běhu a cest v `manifest.jsonl` (UTF-8, jeden JSON záznam na řádek). Souhrn operace uvádí cestu k protokolu. Pro mazání/přepis je vedle manifestu také `delete.jsonl` nebo `overwrite.jsonl`.
2. V manifestu spárujte `planned` a `result` přes `item`: `original` je původní cesta a `saved` cesta kopie. Použijte položku s výsledkem `saved`; samotné `published: true` nestačí. Dokončená obnova má závěrečné `end` se stavem `complete`. Chybějící konec nebo neúplný řádek znamená neúplný běh a vyžaduje kontrolu jednotlivých kopií. Prováděcí protokol upřesní, co se po záloze skutečně přepsalo nebo odstranilo.
3. Kopii nejprve běžným kopírováním v Samandarinu přeneste do **nového názvu nebo prázdné kontrolní složky**, prohlédněte obsah a potřebné ADS/metadata. Teprve potom rozhodněte o návratu na původní místo. Přepsání existujícího souboru při ruční obnově je samostatné uživatelské rozhodnutí.

Obnovovací kopie jsou nezávislá data, nikoli hardlinky. Dědí oprávnění obnovovací složky; původní DACL, vlastník a SACL se pro automatické obnovení neukládají. Přepis v synchronizaci zachovává cílovou DACL; běžná nová kopie dědí oprávnění cílové složky. Přenášejí a ověřují se hlavní data, podporovaná úplná sada ADS, mtime a Read-only/Hidden/System; nejde o klon všech NTFS vlastností. Read-only původní cíle přepisu/mazání plugin odmítá.

## Známá omezení a stav ověření

- Ověřený provoz je místní NTFS x64 se současným SDK 0.16. SMB není otestované; síťovou spolehlivost tento stav neslibuje. Win32 a ARM64 se sestavují v CI, runtime ověření je x64.
- Bez mazání složek, samostatné synchronizace prázdných složek, historie, profilů dvojic, automatické smíšené dávky, automatické obnovy nebo úklidu starých záloh. Nabídka Empty folders může zobrazit návrhy složek, které nynější operační příkazy vynechají.
- Reparse/junction/symlink, EFS a nepodporované cloud/offline položky se blokují. Kořeny se nesmějí překrývat. Unicode a dlouhé cesty jsou podporované v pluginu; průchod takovou cestou v panelech hostitele je samostatná schopnost.
- První chyba nebo Stop zastaví zbytek. Hotové kopie/přepisy/odstranění se automaticky nevracejí. Vlastní nedokončený staging se uklidí, pokud je to možné; případný zbytek je nutné prohlédnout podle ohlášené cesty. Mohou zůstat vytvořené prázdné rodičovské složky a neúplné protokoly.
- Při OFF není v této verzi trvalý prováděcí protokol. Chyba zápisu protokolu při ON zastaví další operace, ale již provedenou změnu nevrátí. Nejde o transakční zálohování ani záruku při pádu napájení či libovolných souběžných změnách.
- ZIP a čistá konfigurace jsou ověřené pro x64; podepsaná katalogová distribuce a celý Inno instalátor nejsou součástí tohoto ověření. Předchozí mizení registrace opravuje kratší registrační popis: hostitel 0.16 přijímá nejvýše 259 bajtů textu. Pokud starý preview plugin již zmizel z nabídky, po aktualizaci souborů jej znovu přidejte přes Plugins Manager; starší obnovy nemažte. Kontroly rozložení při více DPI nenahrazují runtime přechod mezi monitory. Podrobná evidence rozlišuje skutečné GUI testy od řízených chybových testů.

## Mapa dokumentace

| Potřeba | Dokument |
| --- | --- |
| Aktuální instalace, ovládání, obnova a omezení | Tento návod |
| Schválená rozhodnutí a pozdější změny rozsahu | [Návrh](directory-sync-plugin-design.md), [mapování jádra/SDK](directory-sync-plugin-core-adapter.md) |
| Rozhraní, DPI, klávesnice, schránka | [GUI evidence](directory-sync-plugin-ui.md) |
| Souvislý pracovní postup a rozsah tlačítek | [GUI workflow](directory-sync-plugin-workflow.md) |
| Implementace, build a historické etapy | [Implementační záznam](directory-sync-plugin-implementation.md) |
| Technický formát a ověření zápisů | [Nové kopie](directory-sync-plugin-copy-new.md), [obnova](directory-sync-plugin-recovery.md), [přepisy](directory-sync-plugin-overwrite.md), [mazání souborů](directory-sync-plugin-delete.md) |
| Přerušení a nedostatek místa | [Stop/Close](directory-sync-plugin-copy-cancellation.md), [uvolnění](directory-sync-plugin-copy-unload.md), [chyby místa](directory-sync-plugin-copy-space.md), [skutečný VHD test](directory-sync-plugin-vhd-results.md) |

Starší protokolové a souběhové experimenty jsou výzkumná evidence, nikoli požadavek první verze. Rozhodnutí držet úroveň běžných operací jádra má přednost před jejich původními doporučeními. Použitelný rozsah první preview verze uzavírá [distribuční ověření](directory-sync-plugin-distribution.md). Mazání složek, SMB testy a změny SDK zůstávají mimo tento rozsah.