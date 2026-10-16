# Directory Synchronization: balíček a čistá konfigurace 0.16

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