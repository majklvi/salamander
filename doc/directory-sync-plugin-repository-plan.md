# Plán samostatného repozitáře Directory Synchronization

17. října 2026. **Pouze audit a plán; přesun nebyl proveden.** Připraveny neutrální texty a přejmenován dokument pozorování. Nebyl vytvořen repozitář, exportována/přepsána historie ani změněno sestavení, CI nebo SDK. Verze pluginu zůstává 0.1.4. Bez SMB testů.

## Závěr a doporučení

Samostatný repozitář je proveditelný bez rozšíření SDK a bez sestavování salamand.exe. Doporučená varianta: vyfiltrovat celou historii vlastních zdrojů a dokumentace, přibalit přesně připnutý malý snapshot potřebného SDK a podpory vzhledu, potom samostatnými migračními commity upravit cesty, sestavení, balení a CI. Závislosti se při běžném buildu nestahují z pohyblivé větve. Přesun nespojovat s refaktoringem operací, výměnou knihovny vzhledu ani novými funkcemi.

Název k odsouhlasení: `majklvi/salamander-directory-sync`, výchozí větev `main`. Název je návrh, nikoli existující či vytvořený repozitář. Před publikací určit také public/private. Import zachová verzi 0.1.4; změna repozitáře sama není funkční vydání.

## Auditovaný výchozí stav

- Větev `feature/directory-sync-plugin`, HEAD `67ef93f1abf2a66bdb3feefb0ce05acd30830130`.
- Referenční `samandarin-0.16`: `a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449`.
- 46 sledovaných souborů pod `src/plugins/dirsync/`, 30 existujících dokumentů `doc/directory-sync-plugin*.md` před vznikem tohoto plánu.
- Celá větev od reference má 63 commitů, všechny zasahují vlastní zdroje nebo uvedenou dokumentaci; žádné merge commity. První návrh: `cdab3172d76754856ab5860563d4751c45de5a57`; první implementace: `8e1ac3169271c3b3746642cdc559652e95e67ed8`.
- [Strojově čitelný manifest](directory-sync-plugin-repository-manifest.json) obsahuje všech 76 vlastních souborů, návrh cílových cest, 63 původních SHA a SHA-256 jednotlivých vstupů. Navíc rozlišuje 52 podpůrných souborů a závislosti sestavení určené k nahrazení. Jde o inventář tohoto HEAD, nikoli seznam již provedených operací. Plán a manifest jsou další vstupy budoucího exportu; při stanovení skutečného migračního HEAD se seznam i počty obnoví.

Zjištění vycházejí z projektů a jejich importů, zdrojových include, historie a existujících záznamů CL/link/RC z Release x64 i Debug x64. Záznamy obsahují 41 různých sledovaných vstupů mimo plugin: 40 zdrojových/resource vstupů a jednu debug tabulku baseaddr_x64. Přenositelnost v jiném adresáři zatím nebyla prokázána čistým samostatným buildem; ten patří až do vlastní migrace.

## Závislosti sestavení

| Oblast | Skutečné vstupy / závislost | Zacházení v samostatném repozitáři |
| --- | --- | --- |
| SDK | `spl_base.h`, `spl_com.h`, `spl_gen.h`, `spl_gui.h`, `spl_menu.h`, `spl_vers.h`, `dbg.h`, `dbg.cpp`, `versinfo.rc2` ze `src/plugins/shared` | Přesný snapshot 9 souborů do `vendor/samandarin-sdk/`; zachovat API, licenční hlavičky a verzi 0.16. |
| Společné UI | `src/darkmode.cpp/.h`, `darkmode_backend_darkmodelib.cpp/.h`, `common/winlibdpi.h`, `filenamecase.h`, `salamand.rh` | Snapshot 7 souborů do `vendor/samandarin-ui/` se zachováním vnitřních relativních cest. Nezávisí na kompilaci jádra. |
| Knihovna vzhledu | `src/third_party/darkmodelib/`: 36 sledovaných souborů včetně zdrojů, licencí a VENDORED_FROM | Zachovat celý tento malý již přibalený adresář pod `vendor/samandarin-ui/third_party/darkmodelib/`. Nevybírat při migraci novější upstream verzi. |
| Sdílené projekty | `root.props`, x86/x64/arm64 a plugin/lang base/debug/release props | Nahradit vlastními přehlednými props. Jsou to pravidla build systému hostitele, nikoli požadavek ABI. |
| Debug base addresses | `baseaddr_x86.txt`, `baseaddr_x64.txt`, `baseaddr_arm64.txt`, položky dirsync/lang_dirsync | Není nutné přenášet globální tabulky. V novém projektu odstranit jejich odkazy, používat standardní relocatable DLL/ASLR a znovu ověřit Debug. |
| Podpisový hook | `shared/vcxproj/plugin_release.props` volá `tools/codesign/sign_with_retry.cmd` | Odstranit vazbu na certifikát a pracovní prostředí forku. Běžný build a pilotní ZIP zůstanou nepodepsané; podpis případně samostatně později. |
| Výstup a root targets | `OPENSAL_BUILD_DIR`, cesty `salamander/<config>_<arch>/plugins/dirsync`, nadřazený `Directory.Build.targets` | Vlastní output root, pluginové výstupy a vlastní explicitní cíle; nepřebírat globální cleanup ani metriky jádra. |
| Nástroje | VS/MSBuild, MSVC v145, Windows SDK `10.0`; aktuální projekty efektivně používají `stdcpplatest`, `/utf-8`, `/J`, PCH a dynamický CRT `/MD` nebo `/MDd` | Nejprve zachovat současné parametry. Audit použitých knihoven nepotvrdil potřebu vcpkg/NuGet ani jiného pluginu. Není to potvrzení podpory starších toolsetů. |

Všech 52 doporučených vendored souborů se v auditovaném HEAD shoduje s referenčním commitem `a84dbed6...`; žádná změna SDK není potřebná. Darkmodelib má navíc vlastní zaznamenaný upstream původ `b58eb027c4a114c1b1c8fd09870ead982f1b1e72`; pro migraci je rozhodující skutečný snapshot ze Samandarinu včetně místních úprav. Seznam/hashes zachovává manifest. Historie těchto společných komponent se neplete do 63 vlastních pluginových commitů; jejich původ zůstane popsaný a dohledatelný v původním repozitáři.

`darkmode.cpp` a backend používají `#include "precomp.h"`. Při novém sestavení jim musí include/PCH konfigurace nadále dodat **pluginové** precomp.h, nikoli precomp.h hlavního Salamanderu. Ponechat `USE_DARKMODELIB=1`, `_DARKMODELIB_NO_INI_CONFIG` a samostatný `UNICODE/_UNICODE` pro zdroje darkmodelib; nezapnout Unicode globálně pro starší SDK. Vlastní soubory budou používat include cesty SDK/UI místo současných `../../...`.

### Běhové vazby

Kontrola aktuálního Release x64 SPL přes dumpbin: importuje Windows DLL a dynamický Visual C++/UCRT runtime; neimportuje salamand.exe, žádnou importní knihovnu jádra ani darkmodelib.dll. Darkmodelib je kompilována do SPL. SDK služby dostává plugin od hostitele při vstupu, vlastní načítání zůstává nutné pro běh v Samandarinu.

Vedle dvou vstupních exportů pluginu obsahuje SPL i `SalamanderAllowDarkScrollbars` ze sdílené podpory vzhledu. Podpora navíc dynamicky hledá stejnojmenný export v běžícím hostiteli. To je dodatečná vazba podpory vzhledu na současný Samandarin, nikoli veřejná SDK metoda: pokud export nenajde, příslušné opt-in volání skončí bez účinku. Přesun ji zachová včetně ochrany proti callbacku z uvolněného pluginu. Nelze z toho vyvozovat úplnou vizuální kompatibilitu se všemi jinými Salamandery. Není důvod měnit SDK ani v rámci migrace tuto vazbu přepracovávat.

Identifikace pluginu `DirectorySync`, exporty, cíl `dirsync.spl`, názvy SLG, konfigurace a recovery formát se při přesunu zachovají. Uživatelé tak nebudou nuceni nastavení převádět nebo instalovat druhý odlišný plugin.

## Cílové uspořádání

```text
README.md, LICENSE, CHANGELOG.md, AGENTS.md
src/                         # plugin, vlastní resources a ikony
  lang/
tests/                       # všech pět sad, lifecycle DLL a ruční sondy
build-support/               # plugin + dva SLG projekty, vlastní props, solution
tools/                       # build/test/package a generování ikony
vendor/
  samandarin-sdk/             # 9 přesně určených SDK souborů
  samandarin-ui/              # 7 společných souborů + third_party/darkmodelib
    common/
    third_party/darkmodelib/
docs/                        # zachovaná dokumentace, krátký rozcestník
  migration/                 # původ, mapa commitů a manifest
.github/workflows/           # samostatné build/test/CodeQL/package workflow
```

Manifest mapuje každý dnešní vlastní soubor na konkrétní budoucí cestu. `package.ps1` a `create-icon.ps1` přejdou do tools, projekty z vcxproj do build-support, testy do tests. SDK a společná podpora zůstanou v vendor s původními SPDX/copyrighty. Původní SDK/libs adresář se nepotřebuje přenášet; současný linker nevyžaduje žádnou Salamander knihovnu z něj.

## Přenos historie

Použít git-filter-repo v novém odděleném klonu s nezávislými Git objekty, nikdy v aktivním checkoutu nebo původním remote. [GitHub popisuje oddělení adresáře s historií](https://docs.github.com/en/get-started/using-git/splitting-a-subfolder-out-into-a-new-repository). Výběr bude zahrnovat celý `src/plugins/dirsync/`, všechny vlastní dokumenty i nynější plán/manifest. Neomezovat se jen na zdrojový podadresář: přišli bychom o počáteční návrh i dokumentační commity.

Doporučené pořadí: nejprve filtrovat historii a současně neutralizovat všechny její exportované texty a problematické názvy; obsah porovnat proti manifestu se samostatně zaznamenanými redakčními rozdíly. Teprve poté normálním migračním commitem přeuspořádat neutrální cesty na cílové. Autory, autorské i committer časy ponechat; zprávy upravit pouze tam, kde je nutná níže definovaná neutralizace. Dnešní vlastní zprávy commitů takovou úpravu nepotřebují. SHA commitů se kvůli novým stromům/rodičům změní, případné původní kryptografické podpisy nepůjde zachovat jako platné. Uložit `.git/filter-repo/commit-map` a seznam původních SHA pod `docs/migration/`; zkontrolovat, že každý z 63 auditovaných vlastních commitů má dohledatelný protějšek, a vysvětlit případné sloučené/prázdné záznamy. Použít zaznamenanou verzi nástroje. [Oficiální manuál](https://github.com/newren/git-filter-repo/blob/main/Documentation/git-filter-repo.txt) popisuje filtry cest, mapu commitů a skutečnost, že historická přejmenování je třeba zahrnout výslovně.

Smíšené commity zasahují i solution hostitele, AGENTS/README, installer, globální tabulky baseaddr a doc/third_party.md. Jejich pluginová část se zachová; celé soubory hlavního produktu se neimportují do nové historie. Pluginové atribuce z doc/third_party.md a relevantní GUI/Unicode pravidla z AGENTS se samostatně přenesou do nového README/AGENTS/attributions s odkazy na původní SHA. Zachovat autorství SDK a knihovny vzhledu. Záznam inspirace přenést v neutrálním znění s odkazem na archiv původního forku, nikoli kopírováním původního řádku atribucí. Nevytvářet zdání, že jde o nově napsaný kód bez původu.

Výchozí historie je lineární. Neimportovat všechny větve a release tagy Salamanderu; ty označují jiný produkt. Pluginové historické verze lze popsat v CHANGELOG s odkazem na mapované commity. Původní větev zůstane dostupná pro audit; žádný force-push nebo přepis původního forku. PR #30, review komentáře a staré CI logy nejsou součást Git historie: uložit jejich odkazy a podstatná rozhodnutí do migračního záznamu. Nové PR budou patřit novému repozitáři.

## Neutrální export celé historie

Toto je závazné upřesnění exportu z 17. října 2026. Neutralizace není provedena v historii forku. Připraveny jsou pouze aktuální vlastní texty a názvy; provedení filtrů patří až ke schválenému přesunu.

### Rozsah a pravidla

- Referenční software označovat jako „referenční správce souborů“, „referenční program“ nebo „předloha“. Zachovat význam rozhodnutí, výsledky, verze, data, hashe a rozlišení pozorování od domněnek. Nezobecňovat výsledky předlohy na záruky pluginu.
- Dokument pozorování nese neutrální název `directory-sync-plugin-reference-observations.md`. Ve všech exportovaných stromech přejmenovat i jeho dřívější názvy a opravit všechny relativní odkazy. Historický název zjistit z původního blob ID uvedeného v manifestu a z historie přejmenování; nevypustit starší verze kvůli filtrování jen dnešního názvu. Při kolizi dvou názvů zastavit export a vyřešit ji výslovně.
- Z celého přenášeného obsahu odstranit vlastní název referenčního programu, jeho samostatnou dvoupísmennou zkratku, názvy instalačního adresáře/nápovědy a identifikující webové domény. Nahradit také výskyty v komentářích a názvech testů; nezasahovat do algoritmů, API, identifikátorů bez souvislosti ani binárních dat. Odkazy na původní výzkum směřují do připnutého archivu forku, nikoli přímo na identifikující web. Neutrální názvy fixture jsou výslovně označené jako dokumentační aliasy.
- Zahrnout všechny dosažitelné historické textové bloby, i později smazané či přejmenované soubory, úplné zprávy commitů, případných anotovaných tagů a názvy exportovaných refs/cest. Zpracovat UTF-8 i případné UTF-16/legacy texty podle deklarovaného kódování. Neprovádět slepé bajtové náhrady v resources či binárních souborech. Neznámé kódování s podezřelým výskytem musí export zablokovat k ručnímu posouzení.
- Nepřenášet původní celý `doc/third_party.md`. Nový záznam: „Referenční správce souborů a jeho komunita — podklady pro chování a použitelnost; pouze výzkum, bez převzetí kódu, grafiky nebo nápovědy. Původní podklady a autorství: archiv forku, commit `af0e81bc1bccb6c733025fe31a97d53adf5cb32f`, `doc/third_party.md`.“ Odkaz může mířit na tento soubor v připnutém commitu. Copyrighty, SPDX a licence převzatého kódu se zachovají.
- Sdílené SDK ve forku zůstává beze změny. Pouze budoucí kopie `vendor/samandarin-sdk/spl_com.h` dostane neutrální komentář `// (including proxy builds) pass CFileData with the original` v místě poznámky o kompatibilitě. Žádná deklarace, pořadí metod, licence ani ABI se nemění. Manifest musí rozlišit původní hash a hash exportované kopie a tuto komentářovou odchylku popsat. Neprohlašovat poté celý vendor snapshot za bajtově totožný s referencí.

### Provedení až při migraci

1. V odděleném klonu bez sdílených objektů sestavit úplný seznam cest a verzí z dosažitelné historie zafixovaného migračního HEAD. Seznam historických názvů musí zahrnout i jméno před přejmenováním dokumentu pozorování. Originální bundle a podrobný audit s původními texty uchovat mimo budoucí export.
2. Připravit deterministické textové a cestové filtry pro zaznamenanou verzi git-filter-repo. Prostá náhrada finálního checkoutu ani následný opravný commit nestačí. Použít transformaci blobů/cest a podle potřeby zpráv v každém exportovaném commitu; korekturu českých pádů provést i ve starších formulacích. Licence/oznámení o autorství nepřepisovat. Filtry samotné a logy obsahující původní hledané názvy nepřibalovat do exportu; uchovat je jako migrační evidenci u původního forku mimo vybraný rozsah.
3. Z původních vstupních hashů a připravených neutrálních vstupů sestavit mapu původní blob → upravený blob a původní commit → exportovaný commit. Porovnat změny s povoleným rozsahem: redakční text, relativní odkazy, aliasy a názvy. Vyžadovat zachování obsahu datových tabulek, výsledků, autorů/časů a zdrojového kódu kromě komentářů a diagnostického popisku testu. Další úpravy sestavení patří až do následných migračních commitů.
4. Přidat vybrané SDK/UI snapshoty a neutrální atribuce, přeuspořádat cesty a provést ostatní kroky migrace. Uložit konečnou mapu commitů a manifest s původními i výslednými hashi. Dřívější inventář 63 commitů zůstává datovaným auditem; při exportu zahrnout také všechny pozdější přípravné commity.
5. Před publikací opakovat kontrolu na všech dosažitelných objektech exportovaných refs, nikoli jen pomocí `git grep` v HEAD. Kontrolovat case-insensitivně název včetně českých přípon a variant s mezerou/pomlčkou/podtržítkem, samostatnou zkratku včetně názvů oddělených pomlčkou, instalační názvy a domény. Zkontrolovat názvy cest/refs, zprávy, všechna podporovaná textová kódování, migrační manifesty i rozbalený balíček. Nulový počet výskytů je podmínka publikace; žádné výjimky pro staré dokumenty. Nesouvisející podřetězce v identifikátorech neoznačovat za samostatnou zkratku.
6. Do nového remote pushnout pouze schválenou čistou větev a vybrané čisté tagy, nikdy `--mirror`, původní refs ani záložní objekty. V čerstvém klonu z nového remote zopakovat úplný audit dosažitelné historie a standardní build/test/package ověření. Při zbylém výskytu neopravovat jen HEAD: před publikací znovu vytvořit export z originálu a aktualizovat mapy/hash manifest. Původní fork neforce-pushovat a jeho refs, historii ani výzkumné zdroje neodstraňovat.

### Stav přípravy

V aktuálních vlastních souborech je neutrální znění, přejmenovaný dokument a opravené odkazy; produkční chování se nemění. Původní zdroje výzkumu a původní SDK komentář nadále existují v původním forku mimo exportovaný vlastní rozsah. Manifest rozlišuje datovaný audit a připravené vstupy. Kontrola přípravy: ve vlastních zdrojích a dokumentech není výskyt odstraňovaných názvů, zkratky, instalačních názvů ani identifikujících domén; relativní odkazy a SHA-256 připravených vstupů byly ověřeny. `git diff --check` prošel. Změna v C++ je pouze diagnostický popisek testu, běhový kód se nemění; nové sestavení ani GUI test nebyly potřebné. Úplný důkaz čisté přenesené historie, konečné hashe vendor kopie a mapa commitů vzniknou až při skutečné migraci; nyní nebyl proveden žádný export ani přepis historie.

## Dokumentace a balíček

Přenést všech 30 dosavadních dokumentů a tento plán, zachovat historické datované výsledky. README bude stručný rozcestník na aktuální návod, pilotní pokyny a omezení; historické sondy a překonané návrhy označit jako historii. Názvy a texty všech starých verzí podléhají neutralizaci popsané níže; historické výsledky zůstávají zachované. Nový hlavní README nemá kopírovat popis celého Salamanderu.

Zachovat GPL text z kořenového LICENSE, veškeré relevantní atribuce a přibalené licence darkmodelib včetně původu jejích dílčích komponent. Přidat DEPENDENCIES/provenance s původními cestami, commitem, hashi a pravidlem ručních aktualizací vendored souborů.

`package.ps1` nyní předpokládá root tři adresáře nad sebou, doc/, src/third_party a URL větve majklvi/salamander. Upravit kořenové a licenční cesty, zdrojovou URL v NOTICE, odkazy v README.cs/PILOT.cs a metadata zdrojového webu SLG. Odkazy v novém balíčku mají ukazovat na odpovídající verzi/commit nového repozitáře. Původní hashe a výsledky historických testů zachovat. Lokální cesty obsahující odstraňované označení přepsat na dokumentační alias a tuto redakční změnu výslovně označit; skutečné adresáře ani mimorepozitářové důkazy nepřejmenovávat. VHD skripty, binární testovací data a GUI evidence mimo Git nejsou automaticky součástí exportu: případné malé záznamy později vybrat výslovně, s kontrolou osobních cest; nepřenášet VHD ani objemné fixture stromy.

Zachovat balíček dirsync.spl + české/anglické SLG + návody + licence + files.json. Generování ikony upravit na výslovný výstup do src/, protože se mění umístění skriptu. Nezávislost sestavení na podpisovém prostředí nesmí vyústit v nové požadavky na instalaci certifikátu.

## Samostatné CI

Dnešní `tools/run_native_tests.ps1` vyhledává rekurzivně všechny `*_tests.vcxproj` v adresářích tests. Plugin tím už má v x64 CI tyto sady:

1. `dirsync_tests`: sken/porovnání.
2. `dirsync_copy_tests`: adaptéry kopie, přepisů, obnovy a mazání.
3. `dirsync_lifecycle_tests`: řízený SDK caller + skutečná implementace v probe DLL.
4. `dirsync_ui_tests`: anglické resources a layout.
5. `dirsync_ui_czech_tests`: české resources a layout.

`dirsync_lifecycle_probe` je závislost testu, nikoli šestý spouštěný program. `protocol_probe` se argumenty a pevným bezpečnostním prefixem `C:\Samandarin\builds\dirsync\protocol-` je historická ruční sonda; nepatří do standardního CI. Totéž administrátorský VHD postup a GUI přes Computer Use. U běžných sad zůstane izolovaný lokální NTFS TEMP; nové CI nesmí kvůli ruční sondě vyžadovat administrátora, SMB či tuto konkrétní pracovní cestu.

| Workflow | Nejmenší potřebný rozsah |
| --- | --- |
| build.yml | Windows runner s v145; Debug Win32/x64/ARM64 s warnings-as-errors a Release x64. Sestavit SPL i obě SLG přímo, bez solution hostitele, jiných pluginů a vcpkg. ARM64 je build, nikoli runtime ověření. |
| tests.yml | Pět uvedených sad x64; zachovat Debug jako dnešní CI a ověřit také Release používaný pilotním balíčkem. Vlastní malý runner nebo oddělená obecná část dnešního runneru, bez Salamatrix/Python kontraktů celého produktu. Logy a JUnit jako artefakty. |
| codeql.yml | Vlastní C/C++ analýza s explicitním sestavením pluginu (manual build), nikoli převzatá podmínka src/(?!plugins/). Aktuální fork tuto analýzu pro samotný plugin přeskakuje. Vyhodnotit dostupnost code scanning podle public/private a oprávnění nového repozitáře. |
| package.yml | Po úspěšném Release sestavení vytvořit x64 ZIP, znovu rozbalit, ověřit manifest, PE architektury/verze tří modulů a přiložené licence/návody; běžně artefakt, publikaci Release navázat na schválený tag. |

Spouštění na push, pull_request a workflow_dispatch. Kontroly samostatného repozitáře nepoužijí Ensure-ForkCI.ps1 ani interní PR proti samandarin-0.16. Nedědit source-growth/squash pravidla specifická pro hlavní produkt. Závislosti akcí/nástrojů při implementaci připnout. Běžné PR mají jen nezbytná oprávnění; artefakty a job summary mohou zveřejnit výsledky i bez dodatečného PATCH do checks API. Poslední fork běh 38987118939 totiž selhal pouze na `unexpected EOF` při odesílání souhrnu, přestože testy prošly; opakování attempt 2 již celé prošlo. Nepřenášet tento dodatečný křehký reporting jako povinnou závislost úspěchu testů.

Pro CodeQL vycházet z [oficiálního postupu sestavení kompilovaných jazyků](https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/codeql-for-compiled-languages). Ruční sestavení musí opravdu zkompilovat zdroje, nesmí pouze použít cache hotových objektů. Aktuální workflow zatím nebylo změněno.

## Kroky migrace po schválení

1. Odsouhlasit název a viditelnost. Zafixovat skutečný výchozí HEAD, zelené současné CI, verzi 0.1.4 a obsah balíčku; obnovit inventář včetně nových plánovacích souborů. Záloha původních refs/objektů do odděleného bundle, beze změny originálu.
2. V novém pracovním klonu vyfiltrovat a neutralizovat celou přenášenou historii podle níže uvedeného kontraktu. Ověřit seznam commitů, metadata autorů, časů a obsah všech vstupů včetně redakčních odchylek; uložit konečný commit-map. Zatím bez publikace.
3. Samostatný commit: doplnit 52 souborů připnuté podpory + LICENSE/provenance, přejmenovat vlastní cesty a upravit projekty/includes/PCH/output. Zachovat SDK a běhové chování; žádné přidávání funkcí.
4. Samostatný commit: vlastní solution/build/test/package skripty, opravené návody, atribuce, odkazy a GUI pravidla. Ponechat ID DirectorySync i konfigurační formát. Odstranit závislost na globálních baseaddr tabulkách a podpisovém hooku.
5. Samostatný commit: výše uvedené CI; z čistého klonu mimo C:\Samandarin zkusit sestavení a všech pět sad. Ověřit SHA-256 původních vendored vstupů i exportované kopie s deklarovanou komentářovou odchylkou; připustit odlišnou binární podobu způsobenou cestami/debug informacemi, ale porovnat exporty, architektury, resources, verze a funkční výsledek.
6. Až poté vytvořit/publikovat samostatný repozitář, spustit jeho CI a ověřit instalaci výsledného ZIP v čisté soukromé konfiguraci současného hostitele 0.16: registrace po restartu, čeština/angličtina, zapamatování voleb a krátký NTFS postup včetně vzhledu. SMB se nezkouší, SDK se nemění.
7. Teprve po úspěšném převzetí samostatného buildu upravit odkazy původního forku a rozhodnout o distribuci s hostitelem. Doporučení: fork může nadále přibalovat **připnutý vydaný ZIP s hashem** z nového repozitáře; neudržovat dvě editované kopie pluginu a nevytvářet kruhový submodule vztah. Stávající Inno skript obsahuje dirs, soubory a AddPlugin('dirsync',...) a nesmí zůstat odkazovat na neexistující build výstup.
8. Původní větev zachovat jako archiv vývoje. Draft PR #30 po přechodu uzavřít s odkazem na nové místo; nemergovat jej do reference samandarin-0.16. Žádnou větev/PR nyní nezavírat a žádný kód nyní neodstraňovat.

### Podmínky převzetí

Nový čistý klon se sestaví bez okolního checkoutu Salamanderu, bez jeho knihoven, bez lokálních C:\Samandarin cest a bez podpisového certifikátu. Všech pět sad a potřebná sestavení jsou zelená, CodeQL analyzuje skutečný plugin nebo je výslovně popsána nedostupnost služby. Dokumentace i balíček odkazují na nový zdroj, licence a historie jsou dohledatelné; konfigurace 0.1.4 se dál načte. Původní fork zůstává použitelný až do tohoto bodu. Při neúspěchu migrace se vývoj vrátí na nezměněnou původní větev, bez potřeby obnovovat přepsanou historii.

Pro rozhodnutí už zbývá hlavně název/viditelnost nového repozitáře a způsob přibalování do hostitele. Technicky doporučená varianta je samostatná historie + lokální připnuté SDK/UI snapshoty + vlastní MSBuild/CI; nový SDK projekt nebo zásah do jádra nejsou podmínkou.
