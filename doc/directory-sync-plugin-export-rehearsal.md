# Zkušební místní export historie pluginu

17. října 2026. Výsledek: **PASS — žádné odstraňované označení nezůstalo v prověřené exportované historii**. Nevznikl žádný vzdálený repozitář, nic nebylo publikováno. Původní fork a jeho historie zůstaly zachované. Plugin má stále verzi 0.1.4; SDK původního forku ani běhový kód se nemění.

## Fixovaný vstup a rozsah

- Zdroj: větev `feature/directory-sync-plugin`, commit `3552fafbec51af9f47044f9c33484660a0643aa2`.
- Základ: `samandarin-0.16`, `a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449`.
- Git `2.55.0.windows.3`, git-filter-repo `2.47.0`, odděleně instalovaný pod evidenčním kořenem. SHA-256 modulu: `67447413e273fc76809289111748870b6f6072f08b17efe94863a92d810b7d94`.
- Vlastní historie: celý `src/plugins/dirsync/` a `doc/directory-sync-plugin*` včetně obou historických názvů dokumentu pozorování. Filtrační nástroje pod `tools/directory-sync-export/` se nepřenášejí, protože obsahují původní hledané texty.
- Přidán lokální ověřovací commit s 52 vybranými SDK/UI soubory podle manifestu, GPL licencí, neutrálními atribucemi a hashi podpory. Z historických větví a release tagů hostitele se nic dalšího nepřenáší.

## Filtry a zachování obsahu

70 doslovných řádkových pravidel vychází z již commitnuté neutrální úpravy dokumentace. Starší formulace doplňuje omezený filtr s korekturou českých pádů. Neutralizuje se pojmenování předlohy, samostatná zkratka, identifikující weby, názvy nápovědy, testovací prefixy a starý název protokolu. Odkazy směřují na připnutý archiv původního forku. Dokumentační aliasy a redakční zásah jsou v upravených historických dokumentech výslovně označené; skutečná testovací data se nepřejmenovávají.

Filtr neodstraňuje vlastní historické commity, nemění výsledek experimentu, numerická data ani licenční/copyrightové hlavičky. Ze zdrojových změn připouští jen dřívější neutrální diagnostický popisek jednoho testu. Původní 500 vlastních historických blobů bylo porovnáno s očekávaným exportem, včetně celých stromů a režimů souborů každého commitu. Změněno bylo 55 historických textových verzí; všechny ostatní bloby zůstaly shodné. U každého vlastního commitu byly ověřeny původní autor, committer, oba časy včetně časových pásem a úplná zpráva. Žádná kolize cest ani ztracený vlastní commit.

U podpory je 51 z 52 souborů bajtově shodných s původními Git bloby. Jedinou povolenou odchylkou je neutrální komentář o proxy kompatibilitě v **exportované kopii** `vendor/samandarin-sdk/spl_com.h`. Deklarace a ABI se nemění; původní SDK ve forku se neupravuje. Pro každý soubor jsou odděleně uložené vstupní a výstupní SHA-256. Starý manifest hashoval pracovní soubory, někdy s CRLF; zkouška navíc ověřila tento původní hash a rozlišuje jej od hashe kanonického Git blobu. Rozdíl konců řádků není vydáván za změnu SDK.

## Výsledky

| Kontrola | Výsledek |
| --- | --- |
| Vlastní commity / protějšky | 65 / 65, bez vypuštění; původní zprávy, autoři a časy zachovány |
| Výsledná historie včetně podpůrného ověřovacího commitu | 66 commitů |
| Prověřené dosažitelné objekty | 554 blobů, 374 stromů, 66 commitů, žádný tag |
| Historické výskyty cest prověřené napříč všemi commity | 3 602 |
| Zbývající odstraňované zmínky | 0 v textech/blobových řetězcích, úplných commitových záznamech, refs a historických cestách |
| Zachování současných vlastních zdrojů pluginu | Všechny Git bloby shodné se vstupním HEAD |
| Druhý export stejného vstupu | Shodný konečný HEAD, úplná mapa commitů i mapa upravených blobů |
| Nový klon výsledku s `--no-local` | Audit znovu PASS, všechna uložená data dosažitelná; žádné původní nedosažitelné objekty |
| Izolace | Žádné alternates ani sdílené objektové soubory; žádný remote; čistý pracovní strom |
| Integrita | `git fsck --full --no-reflogs` PASS v exportu i ověřovacím klonu |
| Pozitivní kontrola detektoru na vadném repozitáři | Zachyceny historický blob, smazaná cesta, zpráva commitu, ref i anotovaný tag, i když konečný checkout byl prázdný |
| Okrajové případy | PASS: prázdný název u deleteall, přejmenování, UTF-16, nedotčená binární data a odmítnutí neprověřeného textu/atribuce |
| Původní zdrojové refs a stav | Shodné před a po každé úspěšné zkoušce; žádný push |

Výsledný místní HEAD: `4d3e8003aa3806afb755dad0781529e4778f0b9b`. Protějšek původního posledního commitu před přidáním podpory: `7bada5be386677ef82c3b300ce9d6c292e4f3172`. SHA-256 úplné mapy commitů: `993c25e81d86d787cd33d061e41d05eb952bb346db80a7890cf1851cfa93501a`.

## Evidence a opakování

Evidenční kořen mimo Git: `C:\Samandarin\builds\dirsync-export-dryrun-20261017`.

- `trial-05` a `trial-06`: úspěšné nezávislé exporty se shodným výsledkem.
- `trial-06-result.json`: ověření všech původních commitů, mapa upravených blobů a audit výsledku.
- `trial-06-commit-map.txt`: úplná originální mapa nástroje, včetně odfiltrovaných commitů hostitele; do veřejného projektu nepatří automaticky všechny její pomocné položky.
- `trial-05-check2-fresh`: čerstvý ověřovací klon bez remote; `trial-05-check2-verification.json` obsahuje opakovaný audit a výsledky kontrol detektoru.
- `trial-05-check2-negative`: **úmyslně vadná** kontrolní sada. Není kandidát k publikaci. Totéž platí pro nedokončené vývojové pokusy `trial-01` až `trial-03` a první ověřovací klon s neuklizeným symbolickým remote HEAD.
- Pravidla s původními texty a konzolové logy zůstávají v evidenčním kořeni a nepřenášejí se do nového projektu. Postup spuštění je v `tools/directory-sync-export/README.md` původního forku.

Při přípravě se opravilo použití klonu bez checkoutu, obsluha deleteall a omezení blob callbacku na skutečně vybrané objekty. Audit nového klonu také odhalil, že tento Windows Git po odstranění remote může ponechat neplatný symbolický remote HEAD; ověřovací postup jej nyní odstraní výslovně před odpojením remote. Úspěšné výsledky výše pocházejí z opraveného postupu, nikoli z těchto nedokončených pokusů.

## Co tato zkouška nedokládá

Audit pracuje s texty a rozpoznatelnými řetězci, neprovádí OCR obrázků, dekompilaci, hledání uvnitř komprimovaných archivů ani procházení vzdálených odkazů. Ve vybraných historických souborech se neobjevilo neznámé textové kódování s odstraňovaným názvem; takový případ filtr zastaví k ručnímu posouzení. V exportu nejsou binární buildy, balíčky ani VHD.

Toto je ověření historie a podpůrných vstupů. Nedošlo k přeskládání vlastních zdrojů na budoucí cílové cesty ani k opravě MSBuild/CI/balení. Samostatný build, nový instalační balíček a jeho ověření zůstávají další etapou; nynější export není připravený distribuční repozitář. Žádné nové GUI, SMB nebo runtime testy se netvrdí.

Před skutečným přesunem zafixovat nový schválený vstupní HEAD, zahrnout pozdější dokumentační commity a celý postup zopakovat. Čistota prokázaná touto zkouškou platí pro uvedený fixovaný vstup. Publikovat lze až výsledek, který znovu projde auditem po všech zbývajících migračních změnách.
