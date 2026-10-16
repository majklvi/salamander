# Seskupení náhledu podle adresářů — 0.1.1

Ověření 16. října 2026. Změna se týká zobrazení náhledu; algoritmus porovnání, adaptéry zápisů a SDK zůstávají stejné. Bez SMB testů.

## Pravidla zobrazení

- Každý relativní rodičovský adresář má samostatnou skupinu, kořen je `\`. Skupiny jsou řazené podle Windows ordinal porovnání bez rozlišení velikosti písmen; kořen je první.
- Sloupec Název obsahuje koncový název, adresář má koncové `\`. Záhlaví obsahuje relativní cestu rodiče. Formát názvů přebírá nastavení panelů také pro záhlaví.
- Záhlaví není modelový řádek, nelze jím vybrat celou skupinu a nepočítá se do statistik, Ctrl+A, změn návrhů ani operací.
- Návrh odstranění adresáře je skutečný řádek pod jeho rodičem. Záhlaví podstromu jej nenahrazuje a nepřidává potomky k odstranění.
- Filtr ponechá pouze skupiny s viditelnými položkami. Výběr se zachovává podle identity modelového řádku; skrytý výběr se neobnovuje. Skip nezasáhne jinou položku po přeuspořádání.
- Schránka a manifest nadále používají původní úplné cesty. Žádná cesta se neodvozuje ze zkráceného textu sloupce.

## Pozorování referenčního programu

Přes Computer Use bylo na izolované sadě porovnáno zobrazení kořenových souborů, dvou úrovní vnoření, Unicode adresáře a prázdného pravostranného adresáře při asymetrii. Referenční seznam odděluje soubory cestou rodiče; vypnutí směrového filtru odstraní také předěly bez souborů. Kliknutí na předěl neprovádí souborovou operaci. Náš seznam výslovně označuje i kořen a záhlaví používá pouze pro orientaci. Nebyla spuštěna synchronizace ani mazání v referenčním programu.

## Technické řešení

Používá se nativní skupinový režim Windows ListView. [Microsoft dokumentuje](https://learn.microsoft.com/en-us/windows/win32/controls/lvm-enablegroupview), že není podporovaný s LVS_OWNERDATA; seznam proto nyní ukládá členství položek ve skupinách a text získává callbackem ze stejného modelu. Záhlaví mají kompaktní font seznamu a barvy hostitele. Jejich obdélníky se dokreslují po seznamu, protože některá témata neposílají samostatné group custom-draw notifikace.

Při nezměněné viditelnosti se seznam znovu nevytváří. První vložení 6 000 syntetických řádků do 60 skupin v nativním testu trvalo přibližně 2 sekundy (poslední běh 1 953 ms). Jde o změřený kompromis nativního seznamu, nikoli záruku odezvy pro libovolně velký strom; fáze naplnění seznamu běží v UI a nemá vlastní Stop.

## Ověření

Nativní lifecycle sada: **2 357 kontrol**. Rozšířený probe pokrývá skutečné ListView skupiny, kořen, vnoření, Unicode i dlouhého rodiče nad 260 znaky, shodné koncové názvy v různých adresářích, oddělený návrh odstranění adresáře, hit-test záhlaví, Ctrl+A, Skip konkrétního modelového řádku, skrytí/prázdný seznam/obnovení filtrů, zachování úplné relativní cesty a 6 000 položek. Počet hlavních kontrol se nezvýšil: nové scénáře jsou součástí existujícího nativního probe.

Resource/layout testy prošly: angličtina **1 974**, čeština **1 990**, měřítka 96/120/144/192/240 DPI. Release x64 a Debug Win32/x64/ARM64 se sestavily s warnings-as-errors; ARM64 není runtime test.

GUI používá vlastní hostitel 0.16 x64 a sadu `C:\Samandarin\builds\dirsync-grouping-011`, nikoli uživatelskou konfiguraci. V českém tmavém režimu ověřeno: čtyři čitelná záhlaví, Ctrl+A pouze pro pět souborů, kliknutí na záhlaví bez výběru potomků, Skip pouze vnořeného souboru, skrytí prázdných skupin směrovým filtrem a jejich návrat. Nové porovnání v asymetrii s prázdnými adresáři zobrazilo šest skutečných návrhů ve čtyřech skupinách; `delete-dir\` zůstal vlastním řádkem. Maximalizace zachovala zarovnání záhlaví a řádků. Pokus o resize tahem nezměnil šířku, proto není vydáván za úspěšné ověření plynulého resizování.

V anglickém světlém schématu Open Salamander bylo po změně přes Configuration → Colors ověřeno nové porovnání: pět souborů ve čtyřech skupinách, čitelná šedá záhlaví, Unicode cesta, názvy a směrové ikony. Přípravný pokus o offline změnu konfigurace měl nesprávné kódování a hostitel jej odmítl beze změny souboru; soubor byl opraven do UTF-8 a světlé schéma nakonec nastaveno přímo v GUI. Nejde o chybu pluginu ani o nové ověření čisté instalace.

Tyto GUI kroky pouze četly data a měnily návrhy v paměti. Žádná souborová operace nebyla spuštěna. Lokální textová evidence je `gui-evidence.json` pod kořenem testu.
