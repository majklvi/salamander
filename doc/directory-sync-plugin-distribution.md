# Directory Synchronization: balíček a čistá konfigurace 0.16

## 2026-10-17 — aktualizace 0.1.2: oboustranné názvy a oprava záhlaví

Aktuální archiv: `C:\Samandarin\builds\dirsync-distribution\bilateral-0.1.2-20261017\plugin_5.0_dirsync_0.1.2_x64.zip`, SHA-256 `ec006515ec3b71586ef5f3f0b90b24d7a15cca773ad2d93f2952351b3b90fa18`. Nahrazuje níže uvedený první balíček 0.1.2. Verze zůstává **0.1.2**; SPL a oba SLG odpovídají modulům finálního GUI ověření. ZIP byl znovu rozbalen a všech 12 hashů manifestu ověřeno.

Aktualizovaný návod a pilotní pokyny popisují oba povinné názvové sloupce a volby sloupců v Settings; menu souborových řádků již položku Columns neobsahuje. [Ověření](directory-sync-plugin-columns.md) zahrnuje ořez kreslení skupin pod záhlavím, bitmapový regresní test a posouvání sady 240 souborů v tmavém i světlém GUI. Bez SMB testů a změn SDK.

## 2026-10-17 — balíček 0.1.2 s řazením a volitelnými sloupci

Aktuální archiv: `C:\Samandarin\builds\dirsync-distribution\sorting-0.1.2-20261017\plugin_5.0_dirsync_0.1.2_x64.zip`, SHA-256 `9b08623334c9094831391eb6eb53c60f1626069406735366fea572e282ff3028`. SPL i oba SLG mají verzi **0.1.2 (x64)**; přiložený aktuální návod a pilotní pokyny popisují řazení a nabídku sloupců. ZIP byl rozbalen do nového kontrolního adresáře, ověřeno všech 12 hashů a shoda binárních modulů s izolovanou GUI instancí.

[Ověření 0.1.2](directory-sync-plugin-columns.md) zahrnuje světlé/tmavé rozhraní, oba směry řazení velikostí, menu záhlaví, klávesnicové obnovení sloupce a skutečné restarty se zapamatováním nových voleb. Registrační mechanismus se neměnil; nejde o opakování čisté instalace ani kompletního instalátoru. Bez SMB testů a změn SDK.

## 2026-10-16 — balíček 0.1.1 se skupinami adresářů

Aktuální archiv: `C:\Samandarin\builds\dirsync-distribution\grouping-0.1.1-20261016\plugin_5.0_dirsync_0.1.1_x64.zip`, SHA-256 `57fa3a1177c8815cc3d24e96d5ad89024bc60dfe1a3b56f77c874855fa935f36`. Obsahuje aktualizovaný návod a pilotní pokyny; SPL i oba SLG mají verzi **0.1.1 (x64)**. ZIP byl rozbalen do nového kontrolního adresáře, ověřeno všech 12 hashů manifestu a shoda tří binárních modulů s instancí použitou pro GUI test.

[Nové seskupení a jeho ověření](directory-sync-plugin-grouping.md) proběhlo v izolovaném hostiteli 0.16 x64 v českém tmavém a anglickém světlém režimu. Nejde o opakování níže popsané čisté instalace celé předchozí verze; registrační mechanismus ani konfigurace se v 0.1.1 nemění. Bez SMB testů a změn SDK. Následující části jsou historická evidence jednotlivých balíčků.

16. října 2026. Navazuje na [aktuální návod](directory-sync-plugin.md). Dokončeno místní distribuční ověření první preview verze pro x64; bez mazání složek, SMB testů a změn SDK/hostitele.

## Opravená příčina mizení registrace

`IDS_DESCRIPTION` měl 270 znaků/bajtů v UTF-8. Současný hostitel ukládá celý popis, ale při dalším načtení registrace používá v `CPlugins::Load` (`src/plugins2.cpp`) buffer 260 bajtů a vyžaduje úspěšné GetValue všech povinných metadat. Příliš dlouhý popis tak vyřadil celou registraci. Nešlo o ztrátu nastavení Keep originals ani Unicode cesty. To vysvětluje předchozí datované pozorování v [GUI evidenci](directory-sync-plugin-ui.md).

Popis pluginu je nyní stručných **136 bajtů**. Podrobná pravidla zůstávají v nápovědě a návodu. Nová kontrola kompilovaných resources ověřuje přítomnost a UTF-8 délku názvu, popisu a názvu příkazu: text musí mít méně než 260 bajtů, aby se vešel i terminátor. Původní popis tuto podmínku nesplňoval. Nepřidávají se pevné buffery pro souborové cesty; nemění se SDK ani hostitelský loader. Při dalších překladech je třeba zachovat limit registračních metadat.

## Balíček a reprodukce

Skript `src/plugins/dirsync/package.ps1` přijímá složku sestaveného pluginu a **novou** výstupní složku. Kontroluje x64 PE u SPL/SLG, balí jen explicitní produkční moduly, aktuální návod, oznámení a přibalené licence. Nevkládá PDB/LIB/EXP, testovací hooky, konfiguraci, uživatelská data ani obnovy. Existující výstup odmítne. `files.json` obsahuje SHA-256 všech ostatních souborů, vedle ZIP je `package-sha256.txt`. Odkazy z přibaleného návodu vedou do dokumentace zdrojového repozitáře.

```powershell
& .\src\plugins\dirsync\package.ps1 `
  -BinaryDirectory 'C:\Samandarin\builds\dirsync-ui\salamander\Release_x64\plugins\dirsync' `
  -OutputDirectory 'C:\Samandarin\builds\dirsync-distribution\release-20261016'
```

Finální artefakt mimo Git:

- `C:\Samandarin\builds\dirsync-distribution\release-20261016\plugin_5.0_dirsync_0.1_x64.zip`
- ZIP SHA-256: `3DA9392A5596B74D40AD8A34472C41A9950E65CF921493B33CC238F563846BFB`.
- 11 souborů pod `dirsync/`, z toho 10 ověřených položek manifestu a samotný manifest. Finální ZIP znovu rozbalen do `release-20261016\verify`; hashe a přesný počet souborů souhlasí.
- SPL: `2AA8F78913848D95E7F3946E641E8F251B48FB48A69BDBBFC9A94FC2E2C38B01` (produkční kód se touto opravou nemění).
- English SLG: `B17F8F4C49790AB3430A6BC27A0DC3CDF2790A428AC9B68C4371956527BEBA5B` (zkrácený popis).

GUI test použil první rozbalený balíček `package-20261016` se stejnými SPL/SLG. Finální ZIP navíc obsahuje aktualizovaný návod/oznámení a úplnější sadu přibalených licencí; jeho oba moduly jsou byte-for-byte shodné s moduly použitými v GUI. Tato shoda byla nezávisle ověřena. Finální ZIP není jiná netestovaná binárka.

Skript byl také spuštěn nad cestou s českými i CJK předky: výstupní adresář 231 jednotek UTF-16 / 357 bajtů UTF-8, cesta samotného ZIP přesahuje 260 jednotek UTF-16. Archiv vznikl a opakování do stejného výstupu bylo odmítnuto. Přenos souborů je přes .NET/PowerShell bez ACP převodu. Nejde o nové ověření navigace hostitelských panelů dlouhými cestami.

## Skutečný postup v čistém hostiteli

Soukromá kopie **Samandarin 0.16 x64**, žádné změny nainstalovaných uživatelských instancí. Hostitel a potřebné DLL/english.slg převzaty ze stejného dosud používaného hostitele 0.16; SHA-256 EXE `F0B2A56885B363DC0F75926DFEDD3036BB1954B775753348CBCA0FC4D48574DD`. Nejde o stažení nové verze nebo přestavbu hostitele.

Nová složka `C:\Samandarin\builds\dirsync-distribution\clean-20261016`; její `configstorage.ini` na začátku obsahoval pouze StorageType=RegFile a výslovnou místní cestu. `config.reg` měl **0 bajtů**, nebyla převzata žádná starší konfigurace, žádný plugins.ver ani seznam InstalledPlugins. Používá se vlastní paměťový RegFile backend hostitele. ZIP rozbalen do `plugins`; původní uživatelský registr ani starší soukromý config.reg se neupravují.

| Krok přes Computer Use | Skutečný výsledek |
| --- | --- |
| První start po rozbalení | Hostitel automaticky našel plugin. Plugins Manager: total 1, loaded 1, verze 0.1, `dirsync\dirsync.spl`, zkrácený popis. Ruční Add nebylo potřeba a nebylo provedeno. |
| První otevření pluginu | Keep originals výchozí ON, cesta obnovy prázdná. |
| Nastavení Unicode cesty `recovery-漢字`, vypnutí, Close a běžné ukončení hostitele | Cesta uchována, ovládání obnovy při OFF neaktivní; soubor konfigurace obsahuje DWORD 0 a úplnou UTF-16 REG_BINARY cestu. |
| První skutečný restart | Plugins Manager: **total 1, loaded 0** před spuštěním příkazu. Registrace se načetla z uloženého stavu; nešlo o náhradní opětovnou instalaci. Po příkazu se plugin načetl, OFF zůstal, přesná Unicode cesta byla v neaktivním editoru. |
| Zapnutí ON, Close a běžné ukončení | Konfigurace obsahuje DWORD 1, stejnou cestu. |
| Druhý skutečný restart | Nabídka i příkaz dostupné; plugin načten s ON, stejnou Unicode cestou a aktivním ovládáním obnovy. |
| Krátký Compare po druhém restartu | Izolované `left\new.txt` (3 B) a prázdný right: jeden návrh Copy to right, 1 viditelný / 0 skrytých / 0 blokovaných. Žádný zápisový příkaz se nespouštěl. |

Unicode cesta byla navíc nezávisle dekódována z uložených hex bajtů a porovnána s očekávanou cestou. Evidence zůstává mimo Git: `config-off.reg.evidence`, `config-on.reg.evidence`, výsledná konfigurace a malá fixture; ověření finálního ZIP v `release-20261016\verification.json`. Testovací hostitel běžně ukončen, tři původní uživatelské instance zachovány. Obnova i right zůstaly prázdné, new.txt má původní 3 B.

Při startu této minimální kopie bylo nutné zavřít známé upozornění na chybějící běžící Bug Reporter (salmon.exe); nejde o chybu načítání pluginu ani o test distribuce Bug Reporteru. GUI pluginu používá výchozí experimentální Windows Dark Mode. První běh měl nedokreslené panely hostitele, po restartu se vykreslily; do jádra se nezasahovalo. Tento záznam není důkazem sestavení či kontroly kompletní distribuce Samandarinu.

## Kontroly a meze

Produkční Debug/Release x64 sestaveny s warnings-as-errors. **1 684 kompilovaných resource/layout kontrol** prošlo v každé konfiguraci při 96/120/144/192/240 DPI včetně nové regrese registračních metadat. U zápisových adaptérů a SDK nejsou změny, proto se neopakovala další chybová kampaň. Vzdálené CI je kontrolováno na aktuálním commitu interního draftu.

Dokončena instalace rozbalením ZIP, automatická registrace, skutečné načtení po dvou běžných restartech a zapamatování ON/OFF i Unicode cesty v čisté místní konfiguraci 0.16 x64. Archiv je **nepodepsaný vývojový balíček pro ruční instalaci**, nikoli publikace do oficiálního katalogu. Katalogová metadata, podpis, celý Inno instalátor, Win32/ARM64 runtime, registry backend, nouzové ukončení a SMB nejsou tímto průchodem ověřené. První preview zůstává v rozsahu [aktuálního návodu](directory-sync-plugin.md), bez mazání složek, SMB testů a změn SDK.
## 2026-10-16 — aktualizace vzhledu

Nový aktuální x64 balíček: `C:\Samandarin\builds\dirsync-distribution\appearance-20261016\plugin_5.0_dirsync_0.1_x64.zip`.

SHA-256: `DA6F3F69B47959C7B7E53DEB370EB31B659E6D0BD2773263EBD85E4C8C601A2A`.

Obsahuje kompaktní řádek masky/voleb, symboly Action a konfiguraci popisků a světlé/tmavé palety. README odpovídá aktuálnímu návodu. ZIP byl znovu rozbalen: 11 souborů, všech 10 hashů manifestu a úplnost inventáře ověřeny. Jde stále o lokální nepodepsaný vývojový ZIP pro ruční instalaci.

Samandarin 0.16 x64 ve stejné izolované instalaci úspěšně načetl nové moduly a po restartu zachoval registraci i vlastní vzhled. Přístup přes Plugins Manager → Configure a uchování originálů/Unicode cesty zůstaly funkční. Podrobná evidence DPI, klávesnice, palet a Cancel je v závěrečném záznamu [UI ověření](directory-sync-plugin-ui.md). Finální barevný kontrast výběru a přísná kontrola délky uložené palety jsou součástí tohoto balíčku. Bez změn hostitele/SDK, SMB a mazání složek.
## 2026-10-16 — kompaktní Appearance ve správci pluginů

Aktuální ZIP je `C:\Samandarin\builds\dirsync-distribution\appearance-compact-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `1DFBA26F6DF5B731DB82713B4CAFCFB6D701B6C99B68A3575706AEDE0708DE55`. Nahrazuje předchozí balíček vzhledu. Znovu rozbaleno a ověřeno všech 10 hashů a přesný inventář 11 souborů. Obsahuje nový úzký dialog se sedmi řádky, režimem podle hostitele a jediným vstupem přes správce pluginů; README odpovídá aktuálnímu návodu. Moduly byly ověřeny v nové izolované kopii hostitele, nikoli přepisem souborů načtených uživatelskou instancí.
## 2026-10-16 — obnova v Preferences a dvousloupcový vzhled

Aktuální ZIP: `C:\Samandarin\builds\dirsync-distribution\preferences-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `33F8A13E290077C34B9BDA2EDA98A168EFFC2A1F79C094CFEFC9901FD8111258`. Nahrazuje předchozí kompaktni balíček. ZIP obsahuje aktuální Preferences s oddělenými sekcemi a odpovídající návod; všech 10 hashů a inventář 11 souborů byly po rozbalení ověřeny. SPL/SLG přesně odpovídají modulům z GUI testu v preferences-20261016-host. Po normálním zavření hostitele bylo nezávisle ověřeno KeepOriginals=1 a přesná uložená UTF-16 cesta obnova-žluťoučký-漢字. Testovací soubory i prázdná obnova zůstaly beze změny. Bez úklidu starých záloh, mazání složek, SMB a změn SDK.
## 2026-10-16 — tooltipy a potvrzení před změnou voleb skenu

Aktuální ZIP: `C:\Samandarin\builds\dirsync-distribution\discard-warning-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `4F101991465C907433D21C23488D01569645202958F5A2A915013044DE1022E8`. Nahrazuje předchozí balíček Preferences. Obsahuje opravené tooltipy, nový potvrzovací dialog a zapamatované varování obnovitelné v Preferences. Aktualizovaný návod popisuje ovládání a plán kontextového menu výsledků. Znovu rozbaleno, ověřeno všech 10 hashů, přesný inventář 11 souborů a byte-identické SPL/SLG vůči skutečnému GUI testu v discard-warning-20261016-host. GUI ověřilo zachování registrace a potlačeného dotazu po normálním restartu i obnovení varování přes Configure. Stále lokální nepodepsaný x64 ZIP pro ruční instalaci; bez mazání složek, SMB testů a změn SDK.
## 2026-10-16 — nativní kompaktní potvrzení a výběr adresářů

Aktuální ZIP: `C:\Samandarin\builds\dirsync-distribution\native-ui-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `6951BAE2BA7F3DAA19556D8D26C010BC581CA9E97836BDEA7EAEB17ED28677A1`. Nahrazuje předchozí balíček s varováním. Obsahuje kompaktní potvrzení OK/Cancel, tlačítka výběru u obou kořenů a složky obnovy a aktualizovaný návod. ZIP znovu rozbalen: všech 10 hashů manifestu a přesný inventář 11 souborů ověřeny; SPL/SLG jsou byte-identické s moduly skutečného GUI testu v native-ui-20261016-host.

Computer Use ověřil Cancel i potvrzení stejné složky bez ztráty výsledků, klávesnici, maximalizaci a vypnutí ovládání obnovy při Keep originals OFF. Výběr skutečné 305znakové Unicode cesty odhalil návrat krátkého aliasu ze shellu; finální oprava obnoví úplné dlouhé jméno a prošla opakovaným GUI testem i regresí. Podrobnosti a meze DPI jsou v [UI ověření](directory-sync-plugin-ui.md). Produkční Release x64 build s warnings-as-errors, 2 458 resource kontrol v Debug i Release a 2 353 Release lifecycle kontrol prošly. Balíček zůstává nepodepsaný místní x64 ZIP pro ruční instalaci. Kontextové menu výsledků je pouze zdokumentovaný návrh, nikoli součást nové implementace. Bez mazání složek, SMB testů a změn SDK.
## 2026-10-16 — kontextové menu a ikonové filtry

Aktuální ZIP: `C:\Samandarin\builds\dirsync-distribution\context-menu-20261016-final\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `1BE4DF49D3681B56EEBF859D18DAEAD18A3852781179D20D14E0FC20345FE42A`. Nahrazuje předchozí native-ui balíček. Obsahuje návrhové kontextové menu, kopírování cest, kompaktní ikonové filtry, společný řádek Compare/masky/voleb, vytváření složek v nativním pickeru a aktuální návod. Po rozbalení ověřeno všech 10 hashů manifestu a přesný inventář 11 souborů.

Finální SPL/SLG byly přímo z tohoto ZIP nasazené do izolovaného menu-impl-20261016-host a načtené v Samandarinu 0.16 x64. Krátké finální GUI ověření: dva výsledky Compare, zachované ikonové filtry, nativní menu včetně separátoru a Reverse nad jediným párovým Unicode souborem změnil právě jeden návrh doprava na doleva. Šlo jen o náhled, soubory se nekopírovaly. Kompletní předcházející GUI scénáře dlouhých cest, filtrů, asymetrie, stale plánu a vytvoření složky jsou v [evidenci menu](directory-sync-plugin-context-menu.md) a [GUI evidenci](directory-sync-plugin-ui.md). Po posledních optimalizacích výběru/dostupnosti cest prošla znovu nativní regrese. Hostitel běžně ukončen, původní tři uživatelské instance zůstaly.

Stále nepodepsaný vývojový x64 ZIP pro ruční instalaci. Nejde o nové kompletní instalační ověření čisté konfigurace či Inno distribuce; tyto meze se nemění. Bez mazání složek, SMB testů a změn SDK.
## 2026-10-16 — kompaktní operace nahoře a členěné menu

Aktuální ZIP: `C:\Samandarin\builds\dirsync-distribution\compact-actions-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `7D2B59B404447228E6559E9034F9C3A172ED35EEB2DAF108734A3F2F66DF3A9D`. Nahrazuje context-menu balíček. Obsahuje horní Synchronize/Close, dvě odpovídající Stop, seznam až ke status baru, členěné Visible plan a popisek Restore default colors. Aktuální návod popisuje umístění i rozsah příkazů.

Po rozbalení ověřeno všech 10 hashů a přesný inventář 11 souborů. SPL i SLG soukromého hostitele, v němž proběhlo finální GUI ověření, mají stejné hashe jako moduly v rozbaleném ZIP. GUI ověřilo menu, jeho rozsah bez zvýraznění, zrušení potvrzení přepisu a maximalizaci; žádná souborová operace nebyla potvrzena. Release x64 Werror, 2 251 resource kontrol v každé konfiguraci Debug/Release, 2 355 lifecycle kontrol, 539 engine a 766 copy kontrol prošly. Stále jde o nepodepsaný vývojový x64 ZIP pro ruční instalaci; nové kompletní čisté instalační ověření, přechod DPI mezi monitory a runtime Win32/ARM64 tato etapa nepřidává. Bez mazání složek, SMB testů a změn SDK.

## 2026-10-16 — připojené zastavovací části Compare / Synchronize

Aktuální balíček `C:\Samandarin\builds\dirsync-distribution\joined-stop-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `525644418733257B25F58906C272A82BC80D27D53D2ED0AEC2C7F2202A38D9A6`. Nahrazuje compact-actions balíček. Compare a Synchronize mají připojený úzký křížek Stop se samostatným přístupným názvem; aktualizovaný návod vysvětluje pravou část i klávesové zkratky. Po rozbalení ověřeno 10 hashů manifestu, inventář 11 souborů a shoda obou modulů s testovaným GUI hostitelem. Release x64 Werror a 2 268 resource/font kontrol v Debug i Release prošly, stejně jako 2 355 lifecycle kontrol. Rozsah zůstává místní NTFS x64 preview pro ruční instalaci, nepodepsané. Bez mazání složek, SMB testů a změn SDK.

## 2026-10-16 — zachované výsledky, nezávislé filtry a nulové vnější okraje

Aktuální ZIP: `C:\Samandarin\builds\dirsync-distribution\retained-options-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `0042DAFDB4CD104B2207CB77F07CE4B49AC920FBB11B9C7572E86376E5DDBBB7`. Nahrazuje joined-stop balíček. Obsahuje masku vlevo, šest skenovacích voleb ve dvou řádcích, pravý svislý sloupec Compare/Synchronize/Close, nezávislé kombinovatelné filtry, tlumené Skip řádky a seznam bez vnějších mezer po stranách a nad status barem. Checkboxy se převezmou až dalším Compare; nový návod vysvětluje zachování režimu současného plánu. Potvrzování ztráty výsledků i jeho preference už neexistují.

Po rozbalení ověřeno všech 10 hashů a přesný inventář 11 souborů. Finální balíčkové SPL/SLG byly nasazené do nového soukromého `layout-options-20261016-host`, bez zásahu do instalovaných uživatelských instancí. Hlavní GUI průchod a nativní regresi popisuje poslední záznam v directory-sync-plugin-ui.md. Prošel Release x64 warnings-as-errors, 1 924 resource/font kontrol v každé konfiguraci Debug/Release a 2 356 lifecycle kontrol. Balíček zůstává nepodepsaný místní x64 preview ZIP pro ruční instalaci; tato etapa není novým kompletním čistým instalačním ověřením ani testem přechodu mezi DPI monitory. Bez mazání složek, SMB testů a změn SDK.

## 2026-10-16 — balíček s kompaktní mřížkou checkboxů

Aktuální unsigned x64 preview: `C:\Samandarin\builds\dirsync-distribution\checkbox-grid-20261016\plugin_5.0_dirsync_0.1_x64.zip`.
SHA256: `52d6ce87b4c937fba6118bb70d5843a2fd41242fcd55a3a2f2b29ae8a9d76d8c`.
Po rozbalení ověřeno všech 10 hashů manifestu a přesný inventář 11 souborů. Změna upravuje pouze geometrii šesti voleb; instalace ani nastavení se nemění. Dřívější instalační ověření platí pro předchozí balíček; tento balíček nebyl nově ručně načten v GUI.

## 2026-10-16 — opravené nativní filtry

Aktuální unsigned x64 preview: `C:\Samandarin\builds\dirsync-distribution\native-filters-20261016\plugin_5.0_dirsync_0.1_x64.zip`.
SHA256: `2c6725dbdf7aa361967bfa0f7d13e88b155050c405fe739a0292569077da0b44`.
Po rozbalení ověřeno všech 10 hashů manifestu a přesný inventář 11 souborů. Balíček obsahuje aktuální návod, nativní ikonové přepínače ve dvou řádcích, opravené překreslování a Close vedle operačních tlačítek. Produkční SPL a hlavní dialog s finální geometrií byly ověřeny v soukromém 0.16 x64 hostiteli s tmavým schématem; poslední resource build pouze vrátil nezměněný styl informačního dialogu. Nová čistá instalace se neopakovala.
## 2026-10-16 — Settings, světlé schéma a resize tahem

Aktuální unsigned x64 preview: `C:\Samandarin\builds\dirsync-distribution\settings-light-drag-20261016\plugin_5.0_dirsync_0.1_x64.zip`.
SHA256: `73ed7bafaf795ae3f4e5639c89953a58d3e85d4291dbb637d2efc88a87c24706`.
Balíček obsahuje aktuální návod, Settings/Close zarovnané s Compare/Synchronize a stejné nativní ikonové filtry. Rozbalením ověřeno 10 manifestových hashů a přesný inventář 11 souborů. Produkční SPL/SLG stejného sestavení byly přes Computer Use ověřeny v soukromém světlém 0.16 x64 hostiteli: filtry OFF/ON, skutečné zvětšování i zmenšování tahem, Settings/OK/Cancel a Close. Nová čistá instalace se neopakovala. Podrobnosti a limity jsou v datovaném UI záznamu.
## 2026-10-16 — čtvercové adaptivní filtry

Aktuální unsigned x64 preview: `C:\Samandarin\builds\dirsync-distribution\responsive-filters-20261016\plugin_5.0_dirsync_0.1_x64.zip`.
SHA256: `bd2cc01451054879ce7e4fee2fcb4e0e915e563271ba17fe5999013c1bf8cad7`.
Rozbalením ověřeno všech 10 manifestových hashů a přesný inventář 11 souborů. Produkční SPL/SLG stejného sestavení byly ověřeny přes Computer Use ve světlém 0.16 x64 hostiteli: menší čtvercové filtry, dva řádky při 917 px, jeden při 1223 px, návrat tahem a zachovaný stav/výsledky. Návod popisuje automatické přeuspořádání. Nová čistá instalace se neopakovala.
## 2026-10-16 — evidence tmavého přeuspořádání a DPI metrik

Aktuální unsigned x64 preview: `C:\Samandarin\builds\dirsync-distribution\dark-responsive-dpi-20261016\plugin_5.0_dirsync_0.1_x64.zip`.
SHA256: `a8e25e52b1e55a4ff9fc55f25adf55aa7b2f1171307e94ff94eb712350f56bdd`.
Rozbalením ověřeno všech 10 hashů manifestu a přesný inventář 11 souborů. Produkční SPL/SLG stejného sestavení byly použity v soukromém tmavém 0.16 x64 hostiteli pro drag přechod dvou řad filtrů do jedné a zpět, zachování a změny filtrů přes myš/klávesnici a obnovu původních výsledků. Návod výslovně odděluje testy fontových DPI metrik od dosud neprovedeného běhu na dalších systémových DPI a přechodu mezi monitory. Nová čistá instalace se neopakovala.
## 2026-10-16 — balíček po větším NTFS pilotu

Aktualizovaný nepodepsaný ruční x64 ZIP: `C:\Samandarin\builds\dirsync-distribution\large-pilot-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `8457898bc5752f21784d7d05c5335fc4d20343c88b942d7871e5e28d35ab5f3f`. Obsahuje opravu schování infotipu seznamu během práce a aktuální návod včetně odkazu na evidenci většího pilotu. Rozbalení ověřilo všech 10 hashů manifestu a přesný počet 11 souborů. Produkční SPL bylo načteno v soukromém hostiteli 0.16 x64; potvrzení přepisu a Cancel na malé sadě prošly bez překrývajícího infotipu. Nejde o nové kompletní ověření čisté instalace/restartů této revize ZIP; předchozí evidence zůstává datovaná. [Větší pilot](directory-sync-plugin-pilot.md) rozlišuje původní datový běh, následnou opravu a známé omezení velkých UI Automation seznamů.

## 2026-10-16 — česká lokalizace

Přidán samostatný `lang/czech.slg` (**Synchronizace adresářů**). Přeloženo všech 205 položek tabulky řetězců, tři dialogy, nabídky, tooltipy, stavové zprávy a nápověda F1. ID a pořadí formátovacích značek souhlasí s anglickou verzí. Formáty manifestů a protokolů se nemění; zprávy Windows závisejí na jazyku systému.

Český modul je závislostí projektu pluginu a je zaregistrován i v hlavním řešení. Produkční Release x64 sestavení prošlo; samostatný český modul prošel také Debug Win32/ARM64 sestavením bez varování. Nativní načtení zdrojů českého SLG ověřilo diakritiku názvu, LCID `0x0405`, pevné klíče metadat vyžadované hostitelem a shodu verze SPL/SLG. Blok StringFileInfo záměrně zachovává `040904b0`, protože jej současný hostitel vyhledává pevnou cestou; skutečný jazyk určuje VarFileInfo/Translation.

České kompilované dialogy prošly 1 990 kontrolami rozložení, popisků, nekolidujících klávesových zkratek a Tab přístupu pro fontové metriky 96/120/144/192/240 DPI. Anglická regrese prošla 1 974 kontrolami. Český test má vlastní projekt `tests/dirsync_ui_czech_tests.vcxproj`, který objeví běžný nativní test runner. Jde o kontroly kompilovaných zdrojů a fontových metrik; nový GUI běh, skutečná změna systémového DPI a čistá instalace/restarty české verze nebyly v této etapě provedeny.

Nový nepodepsaný ruční x64 ZIP: `C:\Samandarin\builds\dirsync-distribution\czech-directories-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `a2c0551fc6f2feea63798f5340fdf8c5f5be5554b23e4f511059540ac72ebb21`. Obsahuje aktuální plugin s ikonou, anglický i český SLG a aktualizovaný český návod. Rozbalení ověřilo všech 11 hashů manifestu a přesný počet 12 souborů. Jazyk vybírá nezměněné `LoadLanguageModule` hostitele podle načteného jazyka Salamanderu; po změně jazyka hostitele restartujte program. Bez změn SDK, SMB testů a mazání adresářů.

Česká terminologie sjednocena podle přání uživatele: directory/folder = adresář, subdirectory/subfolder = podadresář. Název pluginu je **Synchronizace adresářů**. Aktualizován návod i balíček; delší popisky prošly kontrolami rozložení.

## 2026-10-16 — GUI ověření formátů panelů

Aktuální nepodepsaný ruční x64 ZIP: `C:\Samandarin\builds\dirsync-distribution\panel-format-gui-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `3a5c157c4e301c5b10d4b7720ef89a920db96cafaeb1d7d2b0b802c5b19778be`. Obsahuje doplněný návod a produkční SPL totožné s GUI ověřeným modulem. Rozbalením ověřeno všech 11 hashů manifestu a inventář 12 souborů včetně obou jazyků. [GUI evidence](directory-sync-plugin-ui.md#2026-10-16--gui-porovnání-show-sizes-a-show-names) dokládá shodu všech tří režimů velikostí a šesti skutečně dostupných režimů názvů s hlavními panely hostitele. Bez chyby vyžadující změnu kódu; nejde o opakovanou čistou instalaci ani změnu systémového locale.

## 2026-10-16 — asymetrické mazání adresářů

Nepodepsaný ruční x64 balíček: `C:\Samandarin\builds\dirsync-distribution\directory-deletion-20261016-r2\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `952f52cc61d2af9cfd1a12e9163f74b8315cef2575593d18ebc3b43621dc38c5`. Obsahuje aktuální Release SPL, anglický i český SLG a aktualizovaný návod. Nové rozbalení ověřilo všech 11 hashů manifestu i přesný inventář 12 souborů. Nejde o nové ověření čisté instalace/restartů.

Rozsah a výsledky místních nativních testů jsou v [evidenci mazání adresářů](directory-sync-plugin-directories.md#implementace-a-ověření): 3 729 společných kontrol adaptéru v Release i Debug x64, engine 542, UI EN/CZ 1 974/1 990, lifecycle 2 357. Produkční Release x64 a Debug Win32/x64/ARM64 byly sestaveny s warnings-as-errors. Následující záznam doplňuje pozdější skutečné GUI ověření. Bez SMB testů a změn SDK.

## 2026-10-16 — GUI mazání adresářů ON/OFF a obnova

Aktualizovaný nepodepsaný ruční x64 ZIP: `C:\Samandarin\builds\dirsync-distribution\directory-deletion-gui-20261016\plugin_5.0_dirsync_0.1_x64.zip`, SHA-256 `4b1aaa4b733d4c3c62c814dfc4640c523875934a8b640c56c3d702fcceab2ef9`. Rozbalením ověřeno všech 11 hashů manifestu a přesný inventář 12 souborů. Obsahuje opravený infotip po změně návrhu, obecný anglický titulek potvrzení mazání a aktuální návod.

[Computer Use evidence](directory-sync-plugin-directories.md#2026-10-16--gui-mazání-a-ruční-obnova): ON i OFF skutečně odstranily každý 1 soubor a 4 adresáře, bez chyb; kontrolní obsah a chránění rodiče zůstali. ON uložilo 5/5 originálů, OFF nezaložilo novou obnovu. Ruční obnova přes GUI hostitele potvrdila adresářovou strukturu, souborová data, ADS a souborový čas; adresářové časy nebyly ručně obnovené. Po novém načtení produkčního SPL byly ověřené také obě drobné opravy GUI. Nejde o nové úplné ověření čisté instalace či systémových DPI. Testovací hostitel je zavřený; vlastní fixtures i záloha zůstávají pro kontrolu. Bez SMB testů a změn SDK.

## 2026-10-16 — balíček pro uživatelský pilot

Identifikátor pilotu `pilot-20261016-r1`; nepodepsaný ruční x64 ZIP: `C:\Samandarin\builds\dirsync-distribution\pilot-20261016-r1\plugin_5.0_dirsync_0.1_x64.zip` (241 173 B), SHA-256 `70e34a26cd9a27fbc757f762ba85176eadd4939dc86d2118df2d00feedb7acb6`. Vedle ZIP jsou `package-sha256.txt` a samostatná kopie `PILOT.cs.md` pro předání uživatelům. ZIP nově obsahuje [krátké pilotní pokyny](directory-sync-plugin-pilot-guide.md) jako `PILOT.cs.md`; hlavní `README.cs.md` na ně odkazuje místním odkazem a pokyny zpět na úplný návod.

Všechny tři produkční moduly jsou po bytech shodné s předchozím GUI ověřeným balíčkem z revize `dfc27edbcca001380998094f2b7dd3acdd27417f`; SPL SHA-256 `6e288f5e0cd0ef8b7a5bf354d0594c321f3e3732bd569445175105b32a6f4d9a`. Nové sestavení ani další zásah do uživatelské instalace nebyly potřeba. Nové rozbalení ověřilo všech **12 hashů manifestu a inventář 13 souborů**, x64 PE vstupy i oba místní odkazy mezi návody. Jde o ověření obsahu nového ZIP, nikoli o nový úplný průchod instalací nebo datovými operacemi; ty dokládá předchozí evidence.

Pilot je omezen na kopie místních NTFS dat v Samandarinu 0.16 x64. Pokyny vysvětlují rozsah viditelných návrhů, samostatné druhy operací, výchozí ON a místo pro obnovu, trvalé důsledky OFF/Stop, zahrnutí Hidden/System, omezení adresářů a ruční obnovy. Formulář hlášení žádá identifikaci ZIP/hash, prostředí/DPI, přesné volby a kroky, očekávaný/skutečný výsledek, souhrn a dostupné protokoly bez soukromého obsahu. Bez SMB testů, změn SDK a nových funkcí.

## 2026-10-16 — pilotní ZIP v čisté české konfiguraci 0.16 x64

Ověřen **přímo pilot-20261016-r1 ZIP**, SHA-256 `70e34a26cd9a27fbc757f762ba85176eadd4939dc86d2118df2d00feedb7acb6`, nikoli volný výstup sestavení. Nové rozbalení do `C:\Samandarin\builds\dirsync-distribution\pilot-clean-czech-20261016\host\plugins` ověřilo všech 12 hashů manifestu. SPL SHA-256 `6e288f5e0cd0ef8b7a5bf354d0594c321f3e3732bd569445175105b32a6f4d9a`, český SLG pluginu `33e5f775a9082591e657bed69bd159a40acf2ad3935855a7ca2857589e074cc2`. ZIP nebyl změněný a není potřeba jej nahrazovat.

### Oddělení hostitele a příprava češtiny

Soukromý hostitel 0.16 x64: EXE SHA-256 `f0b2a56885b363dc0f75926dfedd3036bb1954b775753348cbca0fc4d48574dd`, číselná verze 5.0.0.197. Převzat pouze EXE, potřebné runtime DLL a anglický jazykový modul; žádná předchozí konfigurace. Bootstrap před testem obsahoval pouze StorageType=RegFile a výslovnou vlastní cestu, `config.reg` měl **0 bajtů**; bez InstalledPlugins a plugins.ver. Žádný import uživatelského nastavení ani ruční vložení konfigurace pluginu.

Český modul z nainstalovaného hostitele 0.15.1 měl verzi 5.0.0.196 a hostitel jej správně odmítl. Proto byl pro soukromý test připraven odpovídající český modul 5.0.0.197 z nezměněného aktuálního `translations/czech/salamand.slt` a anglického SLG: `prepare_translation_workspace.ps1 -Languages czech -Modules salamand -ImportArchives`, s existujícím lokálním Translator.exe. Import jednoho archivu prošel bez chyby; výsledný hostitelský český SLG má SHA-256 `942e7d85d7eb62a86bab6c2a226851be08b80f5be4cba878e80709bd2851d8bb`. Jde o běžné vytvoření jazykových prostředků pro tento build, bez změny zdrojů hostitele, pluginu nebo SDK. Po neúspěšném přípravném startu byla vlastní konfigurace znovu vynulována před platným čistým testem.

První platný start použil `-language czech`; oba další starty již byly **bez přepínače jazyka**, jen s uloženou konfigurací. Hostitel používá vlastní výchozí Windows Dark Mode. Minimální kopie opět zobrazovala známé upozornění na neběžící salmon.exe; první start měl nedokreslené hlavní panely, další starty je vykreslily. Tyto vlastnosti testovací kopie se neopravovaly v pluginu. Nativní dialog Color je v jazyce Windows (zde anglicky), vlastní dialogy pluginu jsou české.

### Skutečné kroky přes Computer Use

| Krok | Výsledek |
| --- | --- |
| První start, Správce pluginů | Automaticky nalezen **Synchronizace adresářů**, verze 0.1, `dirsync\dirsync.spl`, celkem 1 / načtených 1, český popis. Ruční Přidat nebylo použito. |
| Konfigurovat | České Nastavení, všech sedm barev a další volby; výchozí Uchovat originály ON, cesta prázdná, popisky operací OFF. |
| Změna přes GUI | Popisky ON, vlastní barva Shoda v tmavém režimu RGB(128,255,255), cesta `obnova-žluťoučký-漢字` mimo oba stromy; poté Uchovat originály OFF. Pole cesty i tlačítko výběru jsou zakázané. OK a běžné ukončení hostitele. |
| První skutečný restart | Český hostitel bez jazykového přepínače. Správce: **celkem 1 / načtených 0**, český název a příkaz zachované z registrace. Po Konfigurovat se plugin načetl, OFF, přesná Unicode cesta, popisky ON a vlastní barva zůstaly. |
| Přepnutí zpět ON | Pole cesty i jeho výběr se aktivovaly; OK a běžné ukončení. |
| Druhý skutečný restart | Český příkaz z uložené nabídky načetl české hlavní okno. Nastavení z hlavního okna potvrdilo ON, stejnou cestu, popisky a vlastní barvu. |
| České Porovnat | Malá vlastní NTFS sada: `left/žluťoučký.txt` má 5 B, right je prázdný. Přesně 1 řádek, 1 viditelný, 0 blokovaných; „Kopírovat doprava“, „Pouze vlevo“, „Protějšek chybí“, stav „Porovnání dokončeno“. Žádná zápisová operace se nespouštěla. |
| Storno nastavení a F1 | Výsledek porovnání zachován, F1 otevřelo českou nápovědu. Plugin i hostitel normálně zavřeny. |

Nezávislá kontrola tří uložených konfigurací (OFF, ON před druhým restartem, finální) potvrdila KeepOriginals=0/1/1, ActionLabels=1, Language=czech.slg, přesnou cestu dekódovanou z UTF-16 REG_BINARY i první DWORD DarkColorsV1 = `0x00ffff80`. Testovací soubor stále obsahuje přesně PILOT, pravý i obnovovací adresář jsou prázdné. Původní uživatelské procesy 3820 a 17148 zůstaly spuštěné; žádný testovací hostitel nezůstal běžet.

Lokální důkazy mimo Git: `initial.json` (přípravný stav), `gui-evidence.json`, `config-off.reg.evidence`, `config-on.reg.evidence`, `verification.json`, finální vlastní konfigurace a překladový workspace pod kořenem této sady. Výsledný jazykový modul hostitele identifikuje verification.json; initial.json ještě eviduje původní nekompatibilní přípravný modul.

**Výsledek:** aktuální pilotní balíček prošel čistou instalací rozbalením, českým načtením, dvěma řádnými restarty a zapamatováním testovaných voleb. Chyba pluginu vyžadující změnu binárek nebyla nalezena. Jde o vlastní RegFile backend a runtime x64 při aktuálním systémovém DPI; test neověřuje registry backend, kompletní Inno instalátor, podpis, změnu DPI mezi monitory ani další datové operace. Bez SMB testů a změn SDK.
