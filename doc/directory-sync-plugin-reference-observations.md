# Ověření synchronizace v referenčním správci souborů

Redakční příprava exportu, 17. října 2026: označení předlohy a jejích zdrojů je neutrální. Cesty s prefixem `reference-sync-probe-` a zástupný název nápovědy jsou dokumentační aliasy původních cest; žádná testovací data nebyla přejmenována. Výsledky, časy, hashe a meze ověření zůstávají původní. Nezkrácené zdroje a skutečné cesty zachovává původní fork před touto úpravou.

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


Datum: 14. října 2026. Předimplementační ověření V1–V4 k [návrhu pluginu](directory-sync-plugin-design.md).

Dokument obsahuje tři série. První ověřuje náhled bez zápisů; druhá zahrnuje skutečné kopie, schválené testovací mazání a další hraniční případy. Třetí dokončuje přerušení přenosu a chybu Verify. Starší seznamy neověřených případů je nutné číst spolu s následujícími doplněními.

## První série: prostředí a meze důkazů

- Běžící **referenční správce souborů (x64), verze 11.58**, anglické rozhraní, existující uživatelská konfigurace. Uvedené počáteční volby nejsou důkazem továrního nastavení čisté instalace.
- Ovládání přes Computer Use (`@oai/sky`): nabídka Commands → Synchronize Dirs, pole cest, zaškrtávací volby, Compare, filtry a závěrečný dialog Synchronize.
- Izolované kořeny `C:\Samandarin\reference-sync-probe-20261014\left` a `right`, oba na místním **NTFS**. Testovací soubory vznikly přímo přes PowerShell; referenční program byl ovládán výhradně přes Computer Use. SMB se v této sadě netestovalo.
- Ověřené jsou **zobrazené výsledky porovnání, navržené akce a souhrny před spuštěním**. Finální OK nebylo stisknuto; žádné kopírování, přepis ani mazání v předloze neproběhlo. Nejde o test jeho zápisového enginu, přerušení přenosu ani skutečného zachování metadat.
- Na konci byly ověřeny původní velikosti a časy posledního zápisu všech 30 souborů v obou kořenech proti manifestu. Manifest leží mimo porovnávané stromy. Dočasné panelové záložky byly zavřeny a původní porovnávací přepínače i velikost hlavního okna obnoveny. Testovací data zůstala pro další ověření.

## V1: osm kombinací porovnávacích voleb

Pro každý běh bylo po změně voleb znovu stisknuto Compare. Subdirs, Empty directories a Only selected byly vypnuté; maska `*.*`; všechny přepínače Show zapnuté.

Základní čas `t0` = `2026-10-01T12:00:00Z`, v rozhraní 14:00:00. Novější soubor má `t0 + 120 sekund`, není-li uvedeno jinak. Data jsou ASCII bez BOM a bez koncového nového řádku. Soubory mají příponu `.txt`.

| Soubor | Obsah vlevo | Obsah vpravo | Čas vlevo / vpravo |
| --- | --- | --- | --- |
| a_equal | AAAA | AAAA | t0 / t0 |
| b_same_left_new | BBBB | BBBB | novější / t0 |
| c_same_right_new | CCCC | CCCC | t0 / novější |
| d_diff_same_time | DDDD | EEEE | t0 / t0 |
| e_diff_left_new | FFFF | GGGG | novější / t0 |
| f_diff_right_new | HHHH | IIII | t0 / novější |
| g_size_same_time | JJJJ | KKKKKK | t0 / t0 |
| h_size_left_new | LLLL | MMMMMM | novější / t0 |
| i_size_right_new | NNNN | OOOOOO | t0 / novější |
| j_left_only | PPPP | chybí | t0 / — |
| k_right_only | chybí | QQQQ | — / t0 |
| l_same_plus_1s | RRRR | RRRR | t0 + 1 s / t0 |
| m_same_plus_2s | SSSS | SSSS | t0 + 2 s / t0 |
| n_same_plus_1h | TTTT | TTTT | t0 + 1 h / t0 |

Označení běhů: **S** = obousměrné porovnání, **A** = Asymmetric; první číslice = by content, druhá = ignore date (0 vypnuto, 1 zapnuto).

Legenda: `=` shoda podle zvolených kritérií; `→` / `←` navržené kopírování; `≠` rozdíl bez určeného směru; `□` novější pravá položka bez vybrané akce; `DEL` navržené odstranění vpravo. Modré/červené/zelené řádky samy o sobě nejsou důkazem provedení.

| Případ | S00 | S10 | S01 | S11 | A00 | A10 | A01 | A11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| a: shodný obsah i čas | = | = | = | = | = | = | = | = |
| b: shodný obsah, levý novější | → | → | = | = | → | → | = | = |
| c: shodný obsah, pravý novější | ← | ← | = | = | □ | □ | = | = |
| d: jiný obsah, stejná velikost i čas | = | ≠ | = | ≠ | = | ≠ | = | ≠ |
| e: jiný obsah, stejná velikost, levý novější | → | → | = | ≠ | → | → | = | ≠ |
| f: jiný obsah, stejná velikost, pravý novější | ← | ← | = | ≠ | □ | □ | = | ≠ |
| g: různá velikost, stejný čas | ≠ | ≠ | ≠ | ≠ | ≠ | ≠ | ≠ | ≠ |
| h: různá velikost, levý novější | → | → | ≠ | ≠ | → | → | ≠ | ≠ |
| i: různá velikost, pravý novější | ← | ← | ≠ | ≠ | □ | □ | ≠ | ≠ |
| j: pouze vlevo | → | → | → | → | → | → | → | → |
| k: pouze vpravo | ← | ← | ← | ← | DEL | DEL | DEL | DEL |
| l, m: shodný obsah, rozdíl 1 s nebo 2 s | = | = | = | = | = | = | = | = |
| n: shodný obsah, levý o hodinu novější | → | → | = | = | → | → | = | = |

Kontrolní součty ve stavovém řádku: při respektování data a vypnutém obsahu 14 položek, 4 shodné, 8 rozdílných, 1 pouze vlevo a 1 pouze vpravo; se zapnutým obsahem 3 shodné a 9 rozdílných. Při ignorování data bez obsahu 9 shodných a 3 rozdílné; s obsahem 6 shodných a 6 rozdílných. Počty jsou stejné pro S a A, odlišují se navržené akce.

### Význam pro návrh

1. By content bez ignore date **neznamená „shodný obsah vždy vynechat“**. Dvojice b/c/n se stále nabízí podle času. Pozorování dokládá výsledek, nikoli interní pořadí čtení nebo skutečnost, zda referenční program obsah těchto dvojic četl.
2. Ignore date odstraní automatický směr u rozdílných párů, také v asymetrii. Samotná asymetrie tyto nejednoznačné rozdíly nepřevede na přepis doprava.
3. V asymetrii s respektováním data jsou novější pravé dvojice nevybrané. Stejný čas a zjištěný rozdíl znamená ruční rozhodnutí.
4. Na tomto místním NTFS a s touto konfigurací jsou rozdíly **1 s i 2 s tolerované**; hodinový rozdíl není. Přesná hranice, zlomky sekundy, opačné znaménko rozdílu, nastavení časové tolerance a jiné filesystémy/SMB zatím nejsou ověřené. Z těchto dvou vzorků neodvozovat univerzální algoritmus.

## V2: filtry ovlivňují souhrn provádění

Výchozí běh A10, všechny filtry Show zapnuté: souhrn nabídl **5 kopií doprava / 20 B, 0 kopií doleva, 1 odstranění vpravo**. Pět kopií tvoří b, e, h, j, n; odstranění k.

| Změna pouze filtrů Show, bez nového Compare | Pozorovaný závěrečný souhrn |
| --- | --- |
| Skrýt zelenou šipku L to R | 0 kopií doprava, 1 odstranění vpravo |
| Navíc skrýt Singles | Po kliknutí Synchronize se souhrn neotevřel; nejsou viditelné navržené operace |
| Znovu zobrazit L to R, Singles ponechat skryté | 4 kopie doprava / 16 B, 0 odstranění vpravo |

Referenční program současně ukazoval poměr viditelných a všech položek a indikaci skrytých položek. Skrytí operace tedy neznamená její ztrátu z výsledku porovnání: po opětovném zobrazení se vrací i do souhrnu.

**Doporučení k projednání:** zachovat tuto vazbu filtrů na provádění pro kompatibilitu s předlohou a zřetelně zobrazit počty prováděných i filtrovaných akcí. Původní obecný návrh oddělení pohledu a plánu nesmí vést k neohlášenému provádění skrytých položek. Zde bylo ověřeno sestavení souhrnu; skutečné provedení omezeného plánu zatím testované není. Ruční změna akcí a hromadné označování řádků jsou rovněž mimo tento test.

## V3: závěrečný dialog a prázdné složky

V pozorovaném dialogu Synchronize byly samostatné volby směrů s počtem položek a objemem, cílové cesty, Right: Delete N file(s), Confirm overwrites, Verify a Copy NTFS permissions. V této relaci bylo Confirm overwrites zapnuté; Verify a Copy NTFS permissions vypnuté. Nedostupný směr s nulovým počtem byl zakázaný. Nezkoušelo se přepínání NTFS oprávnění ani jeho účinek.

Při zapnutých Subdirs a Empty directories se zobrazily také prázdné složky `empty_left` a `empty_right`. V obousměrném režimu měly šipky do protější strany; v asymetrii měla pravá složka značku odstranění. Při A01 souhrn uváděl **2 kopírované položky / 4 B a 2 odstraňované položky**: jednostranný soubor plus příslušná prázdná složka. Označení „files“ tak v tomto souhrnu zahrnovalo i složku.

Samostatná volba Right: Delete all empty directories byla v zobrazených souhrnech zakázaná. Její podmínky dostupnosti ani její dosah nebyly ověřeny. Plugin má nadále vlastní schválená pravidla D4/D5; nemá přebírat širší mazání nebo zdrojová oprávnění z předlohy.

Cancel/Escape zavřelo souhrn a vrátilo výsledek porovnání. To není ověření přerušení rozběhnutého přenosu. Prázdnost po filtraci nebyla vydávána za ověřenou bezpečnost skutečného mazání.

## V4: výběr panelů, rekurze a maska

V obou kořenech existují `selA\child.txt` a `selB\child.txt`. Obsah vlevo `left` (4 B), vpravo `right` (5 B), oba časy t0.

- S vypnutým Subdirs se děti do výsledku nezahrnuly. Se zapnutým Subdirs se porovnaly oba podstromy.
- Maska `child.txt` s vyloučením `selB\` (v poli zadáno `child.txt | selB\`) a zapnutými Subdirs vedla k jedinému souborovému páru `selA\child.txt`. Podstrom selB chyběl. Se současně zapnutým Empty directories zůstaly viditelné také obě prázdné jednostranné složky, přestože jejich názvy neodpovídaly `child.txt`.
- Před otevřením synchronizace bylo v levém panelu označeno selA a v pravém selB. Only selected se otevřelo zapnuté a pole kořenů byla zakázaná. S maskou `*.*` a Subdirs byly porovnány **oba** podstromy s oběma protějšky: pro tento případ jde o **sjednocení výběrů**, nikoli průnik.
- Po zrušení pravého označení a ponechání pouze levého selA se porovnal jen `selA\child.txt`, včetně pravého neoznačeného protějšku. Aktivní byl přitom pravý panel; fyzické levé/pravé kořeny se neprohodily.

Neověřeno: výběr jen vpravo, samostatně označené soubory, směs souborů a složek, vnořené a opakované názvy složek v maskách, masky relativních cest a interakce změny kořenů s výběrem. Jediný příklad vyloučení podstromu nedokládá úplnou gramatiku masek.

## Stav po první sadě (před doplňujícím ověřením níže)

| Bod | Co už máme | Co zbývá |
| --- | --- | --- |
| V1 | Osm kombinací voleb, konkrétní tabulka výsledků; vzorky 1 s, 2 s a 1 h | Hraniční časová tolerance, konfigurace a SMB; určit výchozí časovou politiku pluginu |
| V2 | Prokázaný vliv filtrů na souhrn kopií i mazání | Uzavřít navrženou kompatibilitu rozhraní; případně doplnit provedení kontrolovaného plánu a ruční označování |
| V3 | Obsah dialogu v existující konfiguraci, zrušení před spuštěním, náhled prázdných složek | Podmínky globální volby prázdných složek, přerušení přenosu a skutečný účinek zápisových voleb |
| V4 | Výběr dvou různých složek z panelů a výběr jen vlevo, rekurze, příklad vylučovací masky | Zbývající kombinace výběru a přesná gramatika masek; dokončit specifikaci rozsahu |

V5–V6 (SDK, Unicode cesty, náhrada souborů, obnova a metadata Windows/SMB) touto UI sadou vyřešené nejsou. Produktová rozhodnutí D1–D5 zůstávají beze změny. Implementace pluginu nezačala.

## Doplňující ověření 14. října 2026

Provedeno opět přes Computer Use v předloze 11.58; nová izolovaná sada `C:\Samandarin\reference-sync-probe-20261014-r2`. Disk C: je NTFS, Windows hlásí 10.0.26200.0. Uživatel výslovně zvolil pokračování bez SMB testovací složky. Žádný zdejší výsledek není runtime test SMB. Původní sada zůstala oddělená. Následující zjištění doplňují, případně nahrazují starší seznamy neověřených případů výše.

### V1: časové hranice

V `time-left` a `time-right` má každá dvojice stejně velká data. Pravý čas je 2026-10-01 12:00:00 UTC, levý čas se liší o uvedený počet sekund. Syntetické časy byly nastaveny přes `SetLastWriteTimeUtc`; nejde jen o odečet sekund zaokrouhlených v UI.

| Rozdíl vlevo minus vpravo | Stejné bajty, respektovat datum | Rozdílné bajty, respektovat datum |
| --- | --- | --- |
| −3, −2.1, −2, −1, 0, +1, +1.999, +2, +2.001, +2.1, +3 s | Shoda s vypnutým i zapnutým by content | U +1, +2, +2.001 a +3 s: bez by content shoda, s by content ručně řešený rozdíl |
| +3.001, +3.999, +4, +4.001, +5, +10, +60 s | Kopie doprava i při zapnutém by content | V této doplňkové skupině netestováno |
| −3.999, −4, −5, −10, −60 s | Kopie doleva i při zapnutém by content | Netestováno |
| +3599, +3600, +3601 s | Kopie doprava i při zapnutém by content | Netestováno |

Celkem 30 párů: bez by content 15 shodných a 15 rozdílných; s by content 11 shodných a 19 rozdílných. Kladná hranice byla rozlišena na 3.000 versus 3.001 s; záporná mezi −3 a −3.999 s, nikoli na milisekundu. Hodinová korekce nebyla v této konfiguraci aktivní.

Přes F1 byla přečtena také dodaná nápověda `<nápověda referenčního správce>`, téma `dlg_synchronize_dirs.htm`. Popisuje toleranci 1–3 s a obsahové porovnání párů stejné velikosti a data. To podporuje pozorované výsledky, nejde však o trasování interního I/O předlohy. Návrh pluginu: výchozí kompatibilní tolerance **3 s včetně**, explicitně uvedená ve volbách; hodinové rozdíly automaticky neignorovat. Neoznačovat toleranci za skutečnou přesnost NTFS. FAT, jiné filesystémy, jiné konfigurace předlohy a SMB nebyly v této sadě testovány.

### V2: skutečné provedení a ruční vynechání

`transfer-left` obsahoval `one.txt` a `two.txt`, každý 15 B; `transfer-right` byl prázdný. Kliknutí na šipku prvního řádku vyřadilo `one.txt`. Souhrn ukázal jednu kopii 15 B; po spuštění vznikl jen `two.txt`. SHA-256 zdroje a cíle byl shodný: `7856A73BCAA93EB3C891026EBA58B0EF2654D96AD5C3B5D7717CFE48ABC2DD90`.

Poté byl do pravého kořene přidán syntetický `three.txt` (17 B), takže existovala kopie v každém směru. Vypnutí zobrazovacího filtru Right to left skrylo `three.txt`. Souhrn obsahoval pouze `one.txt` doprava (15 B). Po spuštění se skutečně přenesl pouze `one.txt`; levý `three.txt` nevznikl. Hash `one.txt` na obou stranách: `363298ADEFD37EC310AE879EB458EA7F4E372AD25BDDB79DA5E927A586D92B94`.

Při druhém běhu bylo zapnuto Verify. Úspěšný výsledek a shodný hash potvrzují tuto kopii; **neprokazují algoritmus Verify ani detekci úmyslně poškozeného přenosu**. Po dokončení referenční program znovu porovnal stav a dříve ručně vynechané chybějící položce znovu navrhl kopii. Ruční rozhodnutí tedy nelze považovat za trvale uložený profil.

Návrh: provádět jen viditelné, explicitně zařazené akce; před potvrzením ukázat také počet skrytých akcí. Změna filtru musí přepočítat souhrn, nově vytvořený plán musí znovu vyžádat potvrzení.

### V3: mazání, prázdné složky a zrušení při přepisu

Do obou přenosových kořenů byla přidána prázdná `empty-common`, pouze vpravo navíc `empty-right`. Asymmetric + Subdirs + Empty directories navrhlo odstranit pravý `three.txt` a `empty-right`; společná prázdná složka v plánu nebyla. Souhrn uváděl Delete 2 file(s), tedy počítal složku jako položku. Globální Delete all empty directories zůstalo zakázané.

Uživatel potvrdil přesně tyto dvě testovací položky. Referenční program následně vyžádal další potvrzení přesunu do koše. Po potvrzení zmizely právě `three.txt` a `empty-right`; `empty-common`, `one.txt` a `two.txt` vpravo zůstaly. Nešlo o trvalé mazání se Shiftem ani o test obnovy z koše.

Nápověda `dlg_synchronize.htm` říká, že samostatná globální volba Delete all empty directories může odstranit prázdné složky i tehdy, když existují vlevo. **Podmínka jejího zpřístupnění se nepodařila v testovaných kombinacích určit a její globální účinek nebyl spuštěn.** Pro první verzi navrhujeme jen plánované položky: žádný globální úklid mimo plán; to respektuje D4 a ochranu vyloučeného obsahu.

Pro Confirm overwrites byl levý syntetický `one.txt` změněn na `NEW SOURCE TEST` s novějším časem. Po spuštění plánu referenční program zobrazil samostatný dialog s oběma soubory a volbami Overwrite, Skip, Cancel a dalšími. Cancel ukončilo tento běh a vrátilo porovnání; hash pravého souboru zůstal původní `363298...B94`. **Jde o přerušení na potvrzovacím dialogu, nikoli uprostřed přenosu dat.** Copy NTFS permissions nebylo zapínáno, protože D5 přenos zdrojových ACL nepřebírá.

### V4: ukotvení masek a smíšené výběry

Sada `scope-left/right` má osm párů: `rootA.txt`, `rootB.txt`, `selA\child.txt`, `selA\deep\nested.txt`, `selB\child.txt`, `outer\selB\child.txt`, `outer\keep.txt`, `filtered\hidden.bin`. Vlevo jsou 4 B, vpravo 5 B, shodný čas. By content zapnuté, Ignore date vypnuté.

| Maska při zapnutém Subdirs | Pozorovaný výsledek |
| --- | --- |
| `*.txt | selB\` | 5 párů; vyloučeno kořenové i vnořené selB |
| `*.txt | \selB\` | 6 párů; vyloučeno jen kořenové selB, `outer\selB\child.txt` zůstalo |
| `\*.txt` | 2 páry: jen rootA a rootB |
| `selA\**\*.txt` | 2 páry: přímý child i hlubší nested |

Dodaná nápověda `proc_select.htm` potvrzuje význam kořenového `\`, rekurzivního `**` a oddělovače vyloučení `|`. Zmiňuje též více masek oddělených mezerami, regulární výrazy a uložená hledání. Poslední tři možnosti nebyly touto sadou provozně ověřeny; regulární výrazy a uložená hledání se tím nepřidávají do v1. Tlačítko `1x` se podle nápovědy týká aplikace hledacích filtrů, nikoli časové tolerance.

Pouze v pravém panelu označený `rootB.txt` vedl při Only selected k jedinému páru rootB, včetně neoznačeného levého protějšku. Přidání levé označené složky selA vedlo se Subdirs ke třem párům: rootB, child a nested. **Bez Subdirs zůstaly dva páry: rootB a přímý child v explicitně vybrané selA.** Výběr složky tak určuje další počáteční rozsah; rekurze rozhoduje o jeho dalších podadresářích. To se liší od obyčejného celoplošného skenu bez Subdirs. Fyzické kořeny se při aktivitě pravého panelu nepřehodily. Při Only selected byly kořenové cesty zakázané. Dočasné panelové karty byly po testech zavřené.

### Co tato série ještě neprokazuje

- Přerušení předlohy uprostřed velkého přenosu, chybu Verify při poškození dat ani obnovu po pádu.
- Dostupnost a spuštění globálního úklidu prázdných složek; jeho širší význam je doložen nápovědou, nikoli během.
- Úplnou gramatiku masek, všechny kombinace konfigurace předlohy či jiné filesystémy.
- Chování skutečného SMB serveru. Uživatel testovací share neposkytuje; tuto mez nelze odstranit místním testem.

Technické výsledky V5–V6 a návrh chybových stavů jsou v [auditu SDK](directory-sync-plugin-sdk.md). D1–D5 se nemění; zjištěné detaily jsou podkladem k návrhu před implementací.

## Třetí série: přerušení přenosu a chyba Verify

Provedeno v noci ze 14. na 15. října 2026 (Europe/Budapest), referenční program x64 11.58, stejné místní NTFS a uživatelská konfigurace. Ovládání předlohy výhradně přes Computer Use; příprava a řízené změny syntetických souborů přes běžné souborové API. SMB nebylo použito. Nový izolovaný kořen `C:\Samandarin\reference-sync-probe-20261014-r3`; žádný plugin ani zdroj hostitele nebyl implementován.

### Příprava a důkazy

Zdroj `cancel-left\large.bin` má přesně 2 147 483 648 B (2 GiB). Obsah je 2048 opakování bloku 1 MiB, v němž byte na pozici i má hodnotu `i % 251`. Druhá položka `later.txt` má 39 B a obsah `Must remain uncopied after cancellation`. Cíle byly před příslušnými běhy prázdné. V Synchronize bylo zapnuté Verify, Confirm overwrites zapnuté, Copy NTFS permissions vypnuté; oba soubory měly směr doprava. Asymmetric zapnuté, Subdirs vypnuté, by content zapnuté, Ignore date vypnuté, maska `*.*`, všechny zobrazovací filtry zapnuté. V plánu nebyly přepisy ani mazání.

Protokoly mimo repozitář: `evidence.json`, `verify-fault.json`, `verify-read-lock.json` a `verify-fault-while-verifying.json` pod novým testovacím kořenem. Obsahují UTC časy; pro místní čas v této sérii přičíst dvě hodiny. Hashování bylo nezávislé na předloze. Po zaznamenání důkazů byly tři velké syntetické soubory uklizeny běžnými filesystem operacemi; malé položky a protokoly zůstaly.

### V3: přerušení během zápisu

1. Spuštěn souhrn dvou kopií o objemu 2 147 483 687 B do `cancel-right`.
2. Průběhové okno skutečně zobrazovalo přenos large.bin, nikoli Verify. První snímek ukázal 655,3 MiB z 2048 MiB, 0/2 hotových položek a 16 % celkového průběhu (běh zahrnoval následnou kontrolu).
3. Kliknutí Cancel zastavilo běh v přenosové fázi. V následujícím dialogu bylo **User abort!** a cílová cesta; podkladový průběh měl 33 %. Nabídka: **OK (Stop)**, **Cancel**, **Finish copying the current file**. Otestováno pouze OK (Stop).
4. Po OK (Stop) se referenční program vrátil do porovnání. `cancel-right` byl podle následné enumerace včetně skrytých položek **prázdný**: částečný large.bin nezůstal a later.txt se nepřenesl. Zdroj si zachoval velikost a byl následně nezávisle zahashován.

Závěr: pro tento běh do prázdného místního cíle referenční program po potvrzeném zastavení uklidil rozpracovaný soubor a nezačal další položku. Neprokazuje to zachování starého cíle při přerušení přepisu, rollback hotových předchozích položek ani chování dvou nevybraných možností dialogu.

### V3: řízený rozdíl dat při Verify

Bylo nutné odlišit pokus o vyvolání chyby od skutečně prokázané chyby:

- První běh do `verify-right` byl pozastaven během kopírování. Pokus otevřít cíl pro ReadWrite a změnit první byte byl odmítnut chybou sdílení; `verify-fault.json` zaznamenává `success: false`. **Žádný byte nebyl změněn.**
- Poté pomocný proces otevřel metadata cíle s nulovým požadovaným přístupem a FILE_SHARE_WRITE. Referenční program přesto kopii i fázi Verifying dokončil bez chyby; tato metoda čtecí chybu nevyvolala. Zámek byl uvolněn signálem a pomocný proces skončil; otevření trvalo od 22:03:33.3395048 do 22:04:16.5810871 UTC. Měl též vlastní limit 70 s. Z tohoto pokusu nelze vyvozovat mechanismus interních handlů předlohy.
- Úspěšný chybový běh použil nový prázdný `verify-fault-right`. Po dokončení zápisu bylo na snímku výslovně **Verifying** a 2048,0 MiB / 2048,0 MiB zapsaných dat. Kontrola byla tlačítkem Pause pozastavena při 69 % celkového průběhu, tedy před koncem verifikace.
- V tomto stavu se již podařilo otevřít testovací cíl přes FileStream ReadWrite s FileShare.ReadWrite. V 22:05:59.8869786 UTC byl na offsetu **2 147 483 647** změněn poslední byte z **148 na 255**, bez změny délky; následovalo `Flush(true)` a uzavření. `verify-fault-while-verifying.json` zaznamenává úspěch. Zdroj nebyl měněn.
- Po Start kontrola pokračovala a referenční program zobrazil dialog **CRC error**, text **Verifying / The two files are DIFFERENT!**, obě cesty a volby **Ignore**, **Ignore all**, **Abort**. Název dialogu sám neurčuje použitý checksum algoritmus.
- Zvoleno **Abort**. Běh skončil a vrátil porovnání. Na disku zůstal large.bin v plné délce, včetně změněného posledního bytu 255. `later.txt` do tohoto cíle nepřešlo. Referenční program při novém porovnání viděl rozdíl; kvůli novějšímu času pravého souboru byl v asymetrii řádek bez automatické akce, druhá položka si zachovala návrh kopie doprava.

| Soubor před úklidem | Poslední byte | SHA-256 |
| --- | --- | --- |
| `cancel-left\large.bin` (zdroj) | 148 | `D5A53858E3B0189C9DF2D03C86946A7A44314170681550F87C36F6B2D546B221` |
| `verify-right\large.bin` (kontrolní dokončený běh) | 148 | `D5A53858E3B0189C9DF2D03C86946A7A44314170681550F87C36F6B2D546B221` |
| `verify-fault-right\large.bin` (po chybě a Abort) | 255 | `A7C32985B1164000B817A7412E3A69DC4E757EDF1DDAF8F9016B0A5ED9B1AC0E` |

Závěr: Verify v synchronizačním běhu **detekovalo skutečný rozdíl dat**, zobrazilo rozhodovací dialog a Abort zastavilo další položky. Neodstranilo již zapsanou chybnou kopii ani ji automaticky neopravilo. Toto je řízená souběžná změna kopie během Verify, nikoli simulace poškozeného disku, síťového přenosu nebo paměti. Varianty Ignore/Ignore all nebyly spuštěny. Samostatné menu Verify checksums se pro tento test nepoužilo.

### Důsledky pro návrh pluginu a aktualizovaný stav

- Přerušení musí explicitně odlišit zastavení od dokončení aktuálního souboru; již dokončené položky zůstanou evidované. Vlastní staging smí plugin odstranit pouze po ověření jeho identity. Při přepisu musí starý cíl zůstat chráněný D4.
- Chyba verifikace je neúspěch operace. Pokud vznikne před commitem, vadný staging se nesmí publikovat. Pokud vznikne po commitu, zachovat obnovu a nabídnout postup opravy; výsledek nesmí být úspěch jen podle plné velikosti a dokončeného zápisu.
- Náhled po chybě nenahrazuje protokol neúspěchu: čas změněného cíle může následně potlačit automatickou akci v asymetrii.
- **Dva požadované místní případy V3 jsou tím doložené:** přerušení v přenosové fázi a dialog při skutečně rozdílných datech ve Verify. Zůstávají další větve chybového UI, přerušení přepisu, crash recovery a integrační bezpečnost budoucího pluginu; runtime SMB nadále neověřeno.
