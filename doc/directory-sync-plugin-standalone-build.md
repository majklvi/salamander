# Ověření samostatného sestavení, balíčku a CI kandidáta

17. října 2026. **PASS pro místní sestavení, testy, balíček a audit nezávislosti.** Připraveno pouze v izolovaném místním klonu. Nebyl založen vzdálený repozitář, nic nebylo publikováno a původní zdroje, SDK, sestavení, workflow ani Git historie forku se nemění. U forku vzniká pouze tato dokumentace a její propojení s plánem/manifestem. Plugin zůstává ve verzi 0.1.4.

## Vstup a uložený kandidát

- Zdrojový fork: `fa7c9c0ca18a51de5d9367f2c889c2678554efec`, větev `feature/directory-sync-plugin`.
- Nová neutrální exportní zkouška zahrnula všech **66 vlastních commitů** tohoto vstupu a 52 podpůrných souborů; její HEAD je `3801beeebc66a1bf5bd164009b8f08c6cf1ebc6b`. Filtry zůstaly mimo exportovaný rozsah.
- Výsledný kandidát: `5d76d12832f0749457317880ab419ef436a85b62`, pouze místní větev stejného názvu, bez remote.
- Pracovní klon: `C:\Samandarin\builds\dirsync-standalone-20261017\work`.
- Nezávislý čistý klon: `C:\Samandarin\builds\dirsync-independent-20261017\čistý klon 漢字`. Vznikl pomocí `--no-local`, bez alternates nebo přenášení build výstupů. Pozdější opravy skriptů a dokumentace převzal pouze fast-forwardem místních commitů, nikoli kopírováním necommitnutých souborů.
- Úplný místní bundle: `C:\Samandarin\builds\dirsync-standalone-20261017\standalone-candidate-final.bundle`. `git bundle verify` potvrzuje úplnou historii bez vnějších předpokladů; neobsahuje remote a nebyl odeslán.

Pět nových místních commitů: vlastní uspořádání/build/CI, odstranění duplicitních starých testovacích projektů, UTF-8 diagnostika buildu oprava historických zdrojových odkazů a zachování původních CRT/resource definic. Původní autory a commity zachované historie nenahrazují ani nepřepisují.

## Nejmenší adaptér sestavení

Vlastní zdroje jsou v `src/`, testy v `tests/`, všechny aktivní projekty v `build-support/`, dokumentace v `docs/`, skripty v `tools/`. Připnuté SDK/UI zůstává v `vendor/`. Původní pluginové `.props`, globální tabulky baseaddr, podpisový hook, output root hostitele a jeho knihovny se nepoužívají. Automatické hledání nadřazených `Directory.Build.props/targets` je vypnuté. Vlastní `common.props` importují pouze projekty kandidáta; platformní podporu poskytují systémové Visual Studio/Windows SDK.

Zachovány v145, Windows SDK 10, UTF-8 zdroje, unsigned char `/J`, dynamický CRT, warnings-as-errors a účinný původní standard C++: plugin `stdcpplatest`, testovací EXE C++17. Release používá optimalizaci MaxSpeed, Debug standardní debug CRT a ASLR bez tabulek hostitele. Podpora vzhledu má místní Unicode definice; starší SDK není globálně přepnuto na Unicode.

Porovnání 31 vlastních zdrojových/resource souborů proti vstupnímu forku připouští **pouze úpravu include cest**. Funkční změny: 0. Všech 52 vendor souborů odpovídá zaznamenaným exportovaným hashům. Jedinou již dříve deklarovanou odchylkou od původního SDK zůstává neutrální komentář v exportované kopii `spl_com.h`; žádná deklarace ani ABI se nemění.

Pro nový build stačí Windows, VS2026 C++ desktop tools/v145, Windows SDK 10 a PowerShell 7.3+. Skript vyhledá MSBuild systémovým `vswhere`. Není potřeba Python, hostitelský checkout, jeho importní knihovna, vcpkg, síťové stahování build závislostí ani podpisový certifikát. Ikonový skript zapisuje do `src/`. Pro vývoj existují nové kompaktní README a AGENTS.

## Sestavení a testy

| Ověření | Výsledek |
| --- | --- |
| Debug Win32, x64 a ARM64 — SPL + obě SLG | PASS, warnings-as-errors; Win32/ARM64 v pracovním klonu, x64 v nezávislém klonu |
| Release x64 — SPL + obě SLG | PASS, čistá kompilace v nezávislém Unicode klonu |
| Pět x64 testovacích sad, Debug | PASS finálního kandidáta v nezávislém klonu |
| Pět x64 testovacích sad, Release | PASS finálního kandidáta v nezávislém klonu |
| Engine | 542 assertions v každé konfiguraci |
| Kopie / obnova / přepisy / mazání | 3 729 assertions v každé konfiguraci |
| Životní cyklus a skutečná probe DLL | 2 366 checks v každé konfiguraci; obsahuje také 57 Shell kontrol |
| Anglické resources/layout | 2 419 checks v každé konfiguraci |
| České resources/layout | 2 451 checks v každé konfiguraci |
| Načtení skutečného Release SPL | PASS přes systémový loader; `SalamanderPluginGetReqVer()` vrací 106 |
| Exporty SPL | Tři původní exporty: `SalamanderPluginEntry`, `SalamanderPluginGetReqVer`, `SalamanderAllowDarkScrollbars` |
| Importy SPL | Pouze Windows a Visual C++/UCRT runtime; žádná knihovna hostitele ani samostatná darkmodelib DLL |

Win32 a ARM64 nebyly spouštěné. Načtení SPL a nativní probe nejsou nové plné GUI ověření instalace/registrace v hostiteli. Historická administrátorská sonda se do standardních testů nezahrnuje. SMB se netestovalo.

## Důkaz nezávislosti

V nezávislém klonu byl starý `OPENSAL_BUILD_DIR` záměrně nastaven na `Z:\unused-old-fork-output\`; výstupy přesto vznikly výhradně v místním `out/`. Vstupní zdroje a testy nebyly přebírány z předchozího klonu.

Audit 42 `CL/link/rc.read` tracker logů našel **638 jedinečných skutečně čtených cest**. Všechny leží buď v nezávislém klonu, nebo v systémových adresářích Visual Studia / Windows SDK / Windows. Neočekávané vstupy: 0, čtení původního checkoutu či pracovního migračního klonu: 0.

Preprocessing osmi aktivních projektů a jedné probe závislosti zjistil **78 jedinečných importovaných projektových souborů**. Žádný nebyl z původního forku, jiného pracovního klonu ani nadřazeného `Directory.Build.*`. Jde o pozorované čtení a importy této úspěšné x64 konfigurace, nikoli tvrzení, že původní adresář byl fyzicky odpojen nebo že je prokázaná každá myslitelná konfigurace stroje.

Klon nemá remote ani alternates, je čistý a `git fsck --full --no-reflogs` prošel. Cesta s mezerami, českými znaky a ideogramy byla použita skutečně při kompilaci, načítání probe DLL, testech i balení. Zkomolené konzolové cesty původního MSBuild výpisu vyřešila explicitní UTF-8 file diagnostika a souhrnný výstup skriptu.

## Balíček

Finální místní ZIP: `C:\Samandarin\builds\dirsync-independent-20261017\čistý klon 漢字\out\package-03\dirsync-0.1.4-x64.zip`.

SHA-256: `8d8be1317c2947a8c3cbed9859335b6982bb31b0259431444e5540414b7cc3a3`. Stabilní kopie stejného ZIPu a kontrolního součtu je také pod `C:\Samandarin\builds\dirsync-standalone-20261017\artifacts`.

Obsah: SPL, česká/anglická SLG, aktuální české/pilotní pokyny, lokální dokumentace, GPL a licence dílčích komponent. 47 položek hashového manifestu + samotný `files.json`, celkem 48 souborů. Všechny tři moduly byly ověřené jako x64 a verze 0.1.4. ZIP byl skutečně rozbalen a všech 47 hashů znovu ověřeno; žádný přebývající soubor.

Kontrola našla a opravila 19 historických zdrojových odkazů v sedmi dokumentech: místo neexistujících cest uvnitř distribučního ZIP odkazují na připnutý archiv původních zdrojů. Následná kontrola všech Markdown souborů ZIPu: **0 chybějících lokálních odkazů**. Dokumentace je lokální, zdrojové historické reference mohou být nadále online. NOTICE uvádí přesný místní zdrojový commit a výslovně neslibuje neexistující nový repozitář. Balíček je nepodepsaný a nebyl publikován jako Release ani katalogová položka.

## CI kandidát a jeho meze

- `ci.yml`: Debug Win32/x64/ARM64, Release x64, všech pět sad v obou x64 konfiguracích; package job až po úspěšném buildu a testech. Publikuje jen budoucí CI artefakt, ne GitHub Release. Bez fork helperů a pravidel pro jádro.
- `codeql.yml`: C/C++ security-extended, manual build skutečného pluginu, žádná podmínka omezující analýzu na změny jádra. Čistý job nemá cache předkompilovaných objektů.
- Použit explicitní image `windows-2025-vs2026`, akce připnuté na ověřené SHA. Pouze checkout a artefakty mají běžná oprávnění; zápis security-events je u analytického workflow.
- Obě workflow prošla **actionlint 1.7.12**, staženým z oficiálního vydání a ověřeným kontrolním součtem. PowerShell skripty prošly parserem. Build/test/package příkazy byly provedené lokálně.
- **GitHub CI a CodeQL analýza nebyly spuštěné.** Neexistuje remote a CodeQL CLI není nainstalované. Statická kontrola YAML a lokální testy nejsou vydávány za cloudový běh. Po schválení názvu/viditelnosti a publikace zůstává první skutečný CI/CodeQL běh podmínkou převzetí.

Oficiální podklady pro kandidáta: [runner VS2026](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md), [manual build pro CodeQL](https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/codeql-for-compiled-languages), [actionlint](https://github.com/rhysd/actionlint). Tyto zdroje popisují konfiguraci nástrojů; výsledky výše pocházejí z místních logů.

## Nový audit historie a evidence

Finální kandidát byl znovu prohledán nezávislým auditorem: **600 blobů, 72 commitů, 394 stromů a 4 399 historických výskytů cest; 0 zbývajících odstraňovaných označení**. Kontrola zahrnula celé commitové záznamy, refs, staré/smazané texty a rozpoznatelné binární řetězce. Stejná kontrola proběhla nad rozbaleným obsahem ZIP: 0 výskytů. Meze detektoru zůstávají stejné jako v [předchozí zkoušce exportu](directory-sync-plugin-export-rehearsal.md): nejde o OCR, dekompilaci či procházení vzdálených odkazů.

Evidence mimo Git pod `C:\Samandarin\builds\dirsync-standalone-20261017`: `independence-result.json` se skutečnými vstupními/importními cestami, `package-audit.json`, `final-history-audit.json`, původní generátor uspořádání a ověřovací skript, bundle a ověřený actionlint. Build binlogy/tlogy, testové JSON a rozbalený balíček jsou pod `out/` obou klonů; nejsou součástí Git historie ani standardního ZIP.

Další krok: zkontrolovat místní migrační kandidát a rozhodnout o názvu/viditelnosti nového repozitáře. Při změně zdrojového HEAD zopakovat export i audit. Teprve po samostatném schválení lze publikovat kandidáta, ověřit jeho cloudové CI/CodeQL a znovu otestovat instalaci výsledného balíčku v čistém hostiteli. Původní fork se do té doby nemaže ani neodpojuje.
