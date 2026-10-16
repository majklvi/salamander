# Relativní velikosti a časy — 0.1.3

17. října 2026. Změna zobrazení pro stávající Samandarin 0.16, bez změny SDK.

## Chování

Dva nezávislé checkboxy v Nastavení → Sloupce, oba standardně OFF. Uložené jako validované DWORD `RelativeSizesV1` a `RelativeTimesV1`; chybějící nebo neplatná hodnota znamená OFF. OK uloží a překreslí existující seznam, Storno nezmění volby ani výsledky. Změna neovlivňuje soubory, návrhy nebo platnost plánu.

Relativní zobrazení se týká pouze dvojic běžných souborů se stejnou relativní cestou. Každá strana popisuje rozdíl vůči protějšku. Přesný rozdíl velikosti má jednotku B a číselný formát SDK NumberToStr. Čas je absolutní rozdíl UTC FILETIME, s nenulovými složkami d/h/min/s. Zbytky pod sekundou se zahazují; nenulový rozdíl pod sekundou má slovní popis. Není to stáří vůči dnešnímu datu ani rozdíl kalendářních dnů v lokálním čase. Třísekundová tolerance porovnání se nemění. Stejné hodnoty, jednostranné soubory, adresáře a konflikty typů zůstávají absolutní. Oba sloupce nadále řadí číselně podle vlastních původních metadat, nikoli podle rozdílu či zobrazeného textu. Při zapnutí se šířka sloupců podle potřeby zvětší podle metrik fontu a lokalizovaných běžných popisků; uživatel ji může ručně změnit. Šířky se nepamatují přes restart.

## Ověření

- Release x64 a Debug Win32/x64/ARM64 s warnings-as-errors (ARM64 pouze sestavení, nikoli běh). Automaticky 2 357 lifecycle kontrol; testy doplněné o nezávislé ukládání ON/OFF, default OFF, neplatné hodnoty, rozdíl UINT64 bez přetečení, den/hodina/minuta/sekunda, podsekunda a lokalizovanou substituci. Produkční Cell ověřen pro absolutní fallback jednostranných, shodných a konfliktních položek; CompareRows pro řazení metadat i s oběma volbami ON.
- 2 307 anglických a 2 340 českých resource/layout kontrol, metriky 96/120/144/192/240 DPI. Nejde o simulaci změny DPI za běhu.
- Computer Use na soukromém Samandarinu 0.16 x64: izolovaná místní NTFS sada `C:\Samandarin\builds\dirsync-relative-20261017`, 5 řádků (4 dvojice + jednostranný), české/Unicode názvy. Anglický světlý režim: výchozí OFF/OFF, absolutní údaje, zapnutí pouze velikosti. Český tmavý režim: přetrvání velikosti po skutečném restartu, nezávislé zapnutí času, obě volby ON, Storno přes Escape, poté pouze relativní čas. Seznam zůstal 5/5 bez opakování skenu. Zkontrolované protisměrné popisky rozdílů 1 024 / 3 072 B, 1 h, 2 d 3 h 4 min 5 s a 0,5 s; shodné hodnoty a jednostranná položka zůstaly absolutní. Při maximalizaci byly čitelné obě strany. Druhý skutečný restart do anglického světlého režimu zachoval pouze relativní čas a zobrazil newer/older i less than 1 s na obou stranách.
- Zjištěné ořezávání popisků v původních 60px sloupcích vyřešeno rozšířením při zapnutí a při otevření již zapnutého relativního režimu.

Pro tuto změnu nebyly provedeny souborové operace v GUI ani SMB testy. Historické ověření kopírování, přepisů, mazání a obnovy není nově opakované pro tuto úpravu zobrazení.
