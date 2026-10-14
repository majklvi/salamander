# Synchronizační plugin pro Samandarin

Stav: návrh k projednání před implementací. Rešerše k 14. říjnu 2026.

Cílem je plugin pro porovnání a synchronizaci dvou adresářových stromů s pracovním postupem známým z Total Commanderu: **Porovnat → prohlédnout a upravit navržené operace → Synchronizovat**. Uživatel musí před zápisem vidět, co se zkopíruje, přepíše nebo odstraní. Tento dokument vymezuje chování předlohy, navržený rozsah pluginu, bezpečnost operací a rozhodnutí potřebná před zahájením vývoje.

Výchozí doporučení je nativní plugin pro místní disky a Windows síťové cesty, s ručním spuštěním a bezstavovým porovnáním. FTP, archivy a synchronizace s historií vyžadují další návrh. Tato omezení první verze jsou návrh k rozhodnutí, nikoli odsouhlasené zúžení zadání.

## Vývojová větev a základ

| Položka | Hodnota |
| --- | --- |
| Fork | `majklvi/salamander` |
| Nová větev | `feature/directory-sync-plugin` |
| Výchozí větev forku | `samandarin-0.16` |
| Odpovídající upstream větev | `KRtkovo-eu-AI/salamander:release/5.0-samandarin-0.16` |
| Ověřený společný commit | `a84dbed6db632a9e63b5ed9f4bc6691e4cc5f449` |
| Pracovní adresář | `C:\Samandarin\directory-sync-plugin` |

Výchozí commit byl porovnán s oběma vzdálenými větvemi. Větev nevychází z rozpracované integrační větve s dalšími opravami. Tento přírůstek obsahuje pouze dokumentaci a uvedení zdrojů inspirace; implementace pluginu zatím nezačala.

Po prvním publikování větve musí vzniknout interní validační draft proti `samandarin-0.16` pomocí `C:\Samandarin\tools\Ensure-ForkCI.ps1`. Draft se ponechá otevřený, nemerguje se do referenčního základu a další push spouští původní workflow. Před případným upstream PR se posuzují výsledky pro tehdejší aktuální commit. Úspěšný krok, který skutečnou kompilaci či analýzu přeskočil, není důkazem provedeného buildu či analýzy.

## Co znamená kompatibilita s Total Commanderem

Oficiální návod TC popisuje výběr levého a pravého adresáře, případně vybraných podsložek, porovnání, seznam rozdílů se směry kopírování a následné potvrzení synchronizace. Novější soubor běžně navrhuje směr přenosu; rozdílné soubory se stejným datem nemají jednoznačný směr. Přepínače rozlišují položky pouze na jedné straně a na obou stranách. [Oficiální návod TC](https://www.ghisler.com/advanced.htm)

Odkazovaná komunitní wiki, naposledy upravená 13. 10. 2009, rozepisuje rekurzi, masky, práci s výběrem, obsahové porovnání, ignorování data a ruční změnu akce. U ignorování data připouští shodu podle jména a velikosti; společně s porovnáním obsahu rozhoduje obsah. Změny porovnávacích voleb vyžadují nové porovnání. Popisuje i FTP a časový posun, ale výslovně postrádá vysvětlení závěrečného dialogu. Je vhodným přehledem, nikoli úplnou specifikací současné verze. [Synchronize directories](https://www.ghisler.ch/wiki/index.php/Synchronize_directories)

Kontextová nabídka dovoluje operace nad jednotlivými soubory. Aktuální oficiální přehled funkcí uvádí také synchronizaci prázdných adresářů; jejich absence v krátkém wiki popisu proto neznamená, že je TC neumí. [Ukázka synchronizace](https://www.ghisler.com/screenshots/en/08.html), [Přehled funkcí TC](https://www.ghisler.com/index.htm)

### Dva zásadní rozdíly mezi režimy

**Asymetrický režim TC není automaticky přesná kopie zdroje.** Autor 6. 9. 2024 potvrdil, že levá strana je originál a pravá záloha, ale novější soubor vpravo se standardně neoznačí k přepsání starším souborem vlevo; směr lze zadat ručně. Nesmíme tedy bez rozhodnutí zaměnit kompatibilní asymetrický režim za bezpodmínečné zrcadlení. [Odpověď autora TC](https://www.ghisler.ch/board/viewtopic.php?p=459481)

**Obousměrné porovnání není synchronizace s historií.** Z pouhé přítomnosti souboru vlevo nelze odlišit nově vytvořený soubor od souboru, jehož protějšek uživatel vpravo smazal. Stejně tak aktuální dvojice časů nestačí k rozpoznání dvou nezávislých úprav. Autor TC tento limit vysvětluje potřebou předchozího stavu. FreeFileSync pro historický režim používá databázi `sync.ffs_db`; pro detekci přesunů potřebuje i stabilní identifikátory souborů. [Autor TC o konfliktech](https://ghisler.ch/board/viewtopic.php?p=158791), [FreeFileSync o režimech](https://freefilesync.org/manual.php?topic=synchronization-settings)

## Požadavky uživatelů a poučení z předlohy

Jde o konkrétní veřejné požadavky a hlášení, nikoli statistický průzkum četnosti. Staré požadavky nejsou automaticky dnešními chybami TC. Priorita v posledním sloupci je návrh pro náš plugin.

| Potřeba a období | Co zdroj skutečně ukazuje | Dopad na návrh |
| --- | --- | --- |
| Zřetelné vylučování souborů a složek, 2007–2009 | Uživatelé žádali srozumitelnější filtry; masky pro vylučování již existovaly. Část problému byla jejich dohledatelnost. [Diskuse o maskách](https://www.ghisler.ch/board/viewtopic.php?p=178385) | První verze: oddělená pole Zahrnout a Vyloučit, příklady a viditelné shrnutí aktivních omezení. |
| Vynechání celého podstromu před drahým porovnáním, 2007 | Komunitní návrh žádá možnost vyřadit složku i velký soubor. [Návrhy pro Sync window](https://www.ghisler.ch/wiki/index.php?title=Suggestions_for_the_Sync_window) | První verze: filtry použít před čtením obsahu; příkaz Vyloučit podstrom přidá pravidlo a vyžádá nové porovnání. |
| Čerstvé údaje vzdáleného stromu, 2007 | Historické hlášení upozorňuje na obavu ze zastaralé FTP cache; nedokládá současnou chybu. [Návrhy pro Sync window](https://www.ghisler.ch/wiki/index.php?title=Suggestions_for_the_Sync_window) | Každé nové porovnání provede čerstvý sken; budoucí vzdálené adaptéry musí definovat platnost cache. |
| Organizace uložených dvojic a profilů, 2024–2025 | Ukládání profilů existuje, požadavek se týká organizace a pohodlnějšího návratu k cestám. [Diskuse o profilech](https://www.ghisler.ch/board/viewtopic.php?t=84732) | První verze: pojmenované profily cest, režimu a filtrů. Skupiny, hledání a dávky až podle potřeby. |
| Nejprve porovnat obsah a teprve u rozdílů určit směr podle času, 2009 | Uživatel popisuje, že volba Ignorovat datum sice odstraní falešné obsahové rozdíly, ale ztratí návrh směru. Jde o historický požadavek na jinou kombinaci pravidel. [Diskuse o shodném obsahu](https://www.ghisler.ch/board/viewtopic.php?p=186281) | Při ověření V1 rozhodnout, zda zachovat přesné volby TC, nebo nabídnout výslovně odlišený režim „obsah určuje shodu, čas směr“. |
| Rozpoznání přejmenovaných či přesunutých souborů, 2023 | Uživatelé žádají porovnání přes rozdílná jména; samotná shoda obsahu však nemusí určit jednoznačnou dvojici. [Diskuse o ignorování jmen](https://www.ghisler.ch/board/viewtopic.php?p=425814) | Pozdější rozšíření. První verze páruje relativní cesty a neprovádí heuristické přesuny. |
| Bezpečné zacházení s prázdnými složkami, 2008 | Autor upozorňuje, že filtr může skrýt skutečný obsah složky, takže se jen zdá prázdná. [Odpověď o prázdných adresářích](https://www.ghisler.ch/board/viewtopic.php?p=144787) | Povinné pravidlo: prázdná po filtraci neznamená skutečně prázdná; nepoužívat jako podklad pro rekurzivní smazání. |

Některá vylepšení už předloha má: autor v diskusi k TC 11.02 popisuje dostupnost filtrů klávesnicí a pro čtečky obrazovky; oficiální oznámení verze 11.02 uvádí pozastavení porovnávání. Dostupné ovládání, přehledný průběh a možnost přerušení proto patří mezi očekávání od náhrady, nikoli mezi tvrzené novinky proti TC. Pozastavení běhu je kandidát pro další iteraci pluginu. [Autor o přístupnosti filtrů](https://ghisler.ch/board/viewtopic.php?t=80493), [Oficiální oznámení verzí](https://www.ghisler.com/mailing.htm)

## Navržený rozsah první verze

| Oblast | První verze k odsouhlasení | Pozdější rozšíření |
| --- | --- | --- |
| Umístění | Dvě běžné Windows složky, místní disky, USB, UNC a mapované SMB cesty | FTP/SFTP, archivy a jiné pluginové souborové systémy |
| Spouštění | Příkaz v nabídce pluginu, cesty z fyzického levého a pravého panelu, ruční změna cest | Více párů v jedné úloze, dávky a plánování |
| Rozsah stromu | Rekurze, výběr podsložek, zahrnovací a vylučovací masky, prázdné složky | Složitější pravidla podle atributů, věku a velikosti |
| Porovnání | Velikost a čas; obsahové porovnání; ignorování data | Trvalá cache otisků, specializované textové porovnání |
| Režimy | Obousměrné kopírování bez historie; asymetrie podle TC | Výslovné přesné zrcadlení, historický režim a přenos smazání |
| Výsledek | Dvousloupcový seznam, stav a akce, hromadná změna směru, přeskočení, přehled operací | Detekce přejmenování, hierarchické seskupování velkých úloh |
| Provedení | Průběh, přerušení, opakování chybné položky, souhrn a protokol | Pokračování celé úlohy po restartu aplikace |
| Nastavení | Pojmenované profily; uložené volby vždy viditelně zobrazené | Import profilů TC, sdílené profily, skupiny |
| Integrace | Unicode, dlouhé cesty, lokalizace a světlé/tmavé schéma Salamanderu | Samostatné okno mimo proces hostitele |

Mapovaná síťová jednotka je dostupná přes Windows souborové API; není tím přislíben obecný přístup do FTP pluginu. První verze nemá být službou na pozadí ani náhradou zálohovacího systému s historií.

## Navržená pravidla porovnání a plánování

Následující tabulky jsou kontrakt navrhovaného pluginu. Detaily, jejichž přesná shoda s TC ještě není doložena, jsou uvedeny v části Otevřená rozhodnutí a ověření.

### Oddělení stavu od akce

Každý řádek uchová obě relativní cesty, typ položky, velikosti, časy, metodu porovnání a případnou chybu. Stav porovnání bude oddělen od akce: například „obsah rozdílný“ může mít akci Kopírovat vpravo, Kopírovat vlevo nebo Přeskočit. Chyba čtení nebude znamenat rozdíl ani nepřítomnost.

| Stav při obousměrném porovnání | Výchozí akce |
| --- | --- |
| Pouze vlevo, protější podstrom úspěšně načten | Kopírovat vlevo → vpravo |
| Pouze vpravo, protější podstrom úspěšně načten | Kopírovat vpravo → vlevo |
| Shoda podle zvolené metody | Žádná |
| Rozdíl a jednoznačně novější levý soubor, datum se respektuje | Kopírovat vlevo → vpravo |
| Rozdíl a jednoznačně novější pravý soubor, datum se respektuje | Kopírovat vpravo → vlevo |
| Rozdíl při stejném čase nebo ignorovaném datu | Nerozhodnuto; směr zvolí uživatel |
| Chyba, nestabilní soubor, nečitelný podstrom | Zablokováno s důvodem |
| Soubor proti složce nebo nejednoznačné párování názvů | Konflikt; bez automatického odstranění |

Při asymetrii TC se položky přítomné pouze vlevo kopírují doprava a položky přítomné pouze vpravo navrhují k odstranění. Pro existující dvojici návrh zachovává kopírování novějšího levého souboru doprava a žádnou akci u shody. Novější pravý soubor se nepřepisuje automaticky; zobrazí se jako nevybraná odlišná položka s vysvětlením. Tato prezentace je náš návrh; ověřené chování předlohy je absence automatického označení k přepsání. Kombinace asymetrie se stejnými časy či ignorováním data zůstávají k ověření V1. Ruční přepsání musí být viditelné v závěrečném souhrnu.

Budoucí přesné zrcadlení by naopak považovalo levou stranu za autoritu i proti novější pravé verzi. Musí mít vlastní název a nesmí být skrytou změnou významu asymetrie. V obousměrném režimu bez historie se automatické mazání nikdy neodvozuje z pouhé absence protějšku.

### Metody porovnání

| Volby | Navržený význam |
| --- | --- |
| Velikost a čas | Shoda metadat znamená pouze shodu podle těchto kritérií. Obsah nemusí být stejný. |
| Ignorovat datum | U párů rozhoduje velikost; rozdílné páry nedostanou automatický směr podle času. Jednostranné položky si zachovají směr podle přítomnosti. |
| Podle obsahu a ignorovat datum | Různá velikost je rozdíl, u stejné velikosti rozhoduje binární obsah. Rozdílné páry vyžadují ruční směr. |
| Podle obsahu a respektovat datum | Zamýšlená kompatibilita s TC musí ještě přesně určit, které páry číst a jak naložit se stejným obsahem při rozdílném čase. Do uzavření této otázky se implementace pravidla nezačne. |

Obsahové porovnání má používat proudové čtení s možností přerušení a skončit při prvním rozdílu. Nevyžaduje načtení celého souboru do paměti. Kontrolní součet pro cache ani následná kontrola zkopírovaného cíle nejsou totéž co samotné porovnání zdrojů.

Časy porovnáváme interně v UTC. Tolerance musí být výslovná a otestovaná podle filesystému, nikoli skrytá konstanta použitá na vše. FAT má dvousekundovou přesnost času zápisu a odlišné zacházení s místním časem oproti NTFS. Posun o hodinu nelze automaticky považovat za neškodný. [Microsoft o časech souborů](https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times)

### Filtry a výběr

Rozlišit tři věci: rozsah skenu, zobrazení výsledků a výběr operací. Masky a vybrané podstromy vymezují sken. Přepínače shodné/rozdílné/jednostranné vymezují zobrazení. Akce řádku a jeho zařazení do provedení určují skutečný plán.

Změna cesty, porovnávací metody nebo rozsahu skenu zneplatní výsledek. Vyloučené položky musí být chráněné na obou stranách a nesmějí se stát kandidáty k odstranění jen proto, že nebyly načteny. „Pouze vybrané“ se nabídne jen pro zachytitelný a jednoznačný výběr; pravidlo, zda použít sjednocení výběrů obou panelů, je potřeba rozhodnout.

Přesný vliv zobrazovacích filtrů na provedení má být ověřen proti TC. Návrh rozhraní musí vždy ukazovat celkové počty plánovaných operací i počet skrytých vybraných položek. Žádné mazání nesmí být schválené jen implicitně skrytým řádkem.

## Navržené rozhraní a průběh práce

1. Otevřít plugin s levou a pravou cestou podle skutečných stran panelů, nikoli podle aktivního panelu. Zobrazit režim, rekurzi, masky, volby porovnání a případně profil.
2. Spustit Porovnat. Sken probíhá mimo vlákno rozhraní, s průběžným počtem položek a možností přerušení. Porovnání nic nezapisuje do synchronizovaných stromů.
3. Zobrazit název a metadata vlevo, akci uprostřed a metadata vpravo. Směr a mazání mají ikonu i text; barva je doplněk. U rozdílu lze ukázat důvod a podle možností otevřít porovnání dvojice souborů.
4. Umožnit jednotlivě i hromadně změnit směr či přeskočit položku. Nejednoznačné a chybové položky nepovažovat za automaticky vyřešené. Úprava souboru v externím nástroji vyžádá nové ověření jeho řádku.
5. Synchronizovat otevře souhrn konkrétního plánu: počet a objem přenosů v obou směrech, počet přepisů a mazání, způsob obnovy, konflikty a skryté vybrané položky. Změna plánu po potvrzení vyžaduje nové potvrzení.
6. Během provádění zobrazit aktuální operaci, průběh a Přerušit. Po skončení odlišit Úspěch, Dokončeno s přeskočenými položkami, Přerušeno a Chyba. Protokol obsahuje skutečně provedené operace a jejich výsledky.

Podobné oddělení náhledu změn a provedení nabízí WinSCP; mazání má samostatnou volbu a v běžném obousměrném režimu není odvozováno z absence souboru. [WinSCP Synchronize Dialog](https://winscp.net/eng/docs/ui_synchronize)

## Bezpečnostní pravidla navrhovaného pluginu

Tato pravidla jsou součástí návrhu našeho provedení, nikoli tvrzení, že je TC implementuje stejně.

| Situace | Požadované chování |
| --- | --- |
| Kořen neexistuje, síť se odpojí, přístup je odmítnut | Chybu nezaměnit za prázdný adresář. Nepovolit odvozené mazání; pro první verzi zablokovat provedení při neúplném skenu. |
| Záměrně prázdný zdroj | Je platný jen po úspěšném ověření kořene a enumerace. Hromadné odstranění v cíli musí být výslovně vidět v souhrnu. |
| Zdroj či cíl se změní po porovnání | Před operací ověřit očekávanou existenci, typ, identitu a relevantní metadata. Při odchylce označit zastaralý plán a položku neprovést. Samotná kontrola cesty neřeší všechny závody; provedení musí pracovat s handly a ověřenou identitou. |
| Chyba či přerušení při přepisu | Nejprve kopírovat do jedinečného dočasného souboru na cílovém filesystému. Původní cíl nahradit až po úspěchu a opětovném ověření. Definovat úklid a chování při plném disku. |
| Mazání nebo přepis uživatelských dat | Rozhodnout o Koši či verzovacím adresáři. Nedostupná obnova nesmí potichu znamenat trvalé smazání. Verzovací prostor nesmí být součástí synchronizovaného stromu. |
| Prázdné složky a filtry | Rozlišit skutečnou prázdnost od vyfiltrovaného obsahu. Složku odstranit jen po ověření skutečné prázdnosti; nikdy rekurzivně nesmazat neznámé či vyloučené děti. |
| Shodné nebo vnořené kořeny | Porovnat cesty i dostupné identity a odmítnout shodné či překrývající se stromy v obou směrech. Při nevyřešené možné kolizi neprovádět zápisy. Detekce libovolných SMB aliasů není přislíbena; její meze a postup při nejistotě uzavře V6. |
| Symlinky, junctions a jiné reparse points | V první verzi nenásledovat; zobrazit vynechané položky a důvod. Chránit také odpovídající protějšek před odvozeným mazáním. Kontrola zahrne kořen a rodičovské komponenty i změnu na odkaz mezi skenem a operací. Ve V6 upřesnit dopad na cloudové placeholdery a další reparse typy. |
| Kolize jmen a typů | Žádné automatické sloučení různých názvů, normalizace Unicode ani soubor proti složce. Case kolize na odlišných filesystémech jsou konflikt. |
| Metadata mimo základní model | Výslovně rozhodnout o ACL, alternativních datových proudech, hardlinkách, sparse/compressed/encrypted souborech a časech složek. Bez tohoto rozhodnutí neslibovat úplný klon. |

Microsoft dokumentuje, že výsledek enumerace může být zastaralý už před následnou operací a že chyby je nutné rozlišovat přes `GetLastError`. Návrh proto potřebuje vlastní zacházení s neúplným skenem a změnami stavu. [FindFirstFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew)

Dočasná kopie chrání před nedokončeným přepisem, ale nezajišťuje transakci celého stromu ani obnovu při každém výpadku napájení. Autor FreeFileSync také rozlišuje tuto ochranu od následného ověření obsahu. Historie či Koš zase chrání jiný případ, omylem schválenou změnu. [FreeFileSync o fail-safe copy](https://freefilesync.org/forum/viewtopic.php?t=1495), [FreeFileSync o verzování](https://freefilesync.org/manual.php?topic=versioning)

Názvy a cesty mají zůstat v Unicode, bez průchodu přes ztrátové ACP převody. Dlouhé cesty se musí řešit v celém řetězci včetně UNC, dialogů, protokolu a obnovy; existence široké Win32 funkce sama o sobě nestačí. [Microsoft o dlouhých cestách](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)

## Technický návrh pro základ 0.16

Navržené vrstvy jsou oddělené, aby šla rozhodovací pravidla testovat bez Salamanderu a souborové operace bez klikání v dialogu:

| Vrstva | Odpovědnost |
| --- | --- |
| Adaptér hostitele | Nabídka pluginu, zachycení cest a výběru, lokalizace, nastavení, obnova panelů a životní cyklus. |
| Skenování | Vlastněné Unicode cesty, enumerace, filtry, metadata, důkaz úplnosti každého podstromu a evidence chyb. |
| Porovnání | Párování relativních cest, porovnání metadat/obsahu a výslovná nejistota. Žádné zápisy. |
| Plánování | Režim synchronizace, navržené akce, ruční přepsání, chráněné položky a neměnný potvrzený plán. |
| Provedení | Opětovná kontrola stavu, kopírování, obnova, pořadí operací, přerušení a protokol. |
| Rozhraní | Zobrazení stavů a akcí, filtry pohledu, ovládání klávesnicí, souhrn a průběh. |

Plán nejprve zajistí potřebné adresáře a bezpečné přenosy, destruktivní úklid následuje až po úspěchu příslušných závislostí. Každá operace musí mít výsledek; chyba nesmí zmizet za celkovým hlášením Úspěch. Po dokončení se dotčené panely obnoví a nový běh začíná novým porovnáním.

Konkrétní omezení SDK a integrační body doplňuje [technický průzkum základu](directory-sync-plugin-sdk.md). Před vytvořením kostry pluginu je nutné uzavřít zejména získání dlouhých Unicode kořenových cest a provedení zápisů. Nesmíme předpokládat dostupnost API z jiných rozpracovaných větví.

## Akceptační scénáře před vydáním

Scénáře níže jsou plán budoucího ověření. Při přípravě této dokumentace nebyly provedeny jako testy existujícího pluginu.

| Oblast | Minimální příklady a očekávání |
| --- | --- |
| Základní rozhodování | Jen vlevo/vpravo, stejné soubory, novější vlevo/vpravo, stejný čas a různá velikost, stejná metadata a různý obsah. Ověřit akce každého režimu. |
| Obsah a datum | Všechny kombinace obsahu a ignorování data; stejný obsah s jiným časem; prázdné soubory; soubory větší než 4 GiB; změna během čtení. |
| Asymetrie | Novější cíl se automaticky nepřepíše; ruční přepsání je v souhrnu. Jednostranné cílové položky se mažou jen ve schváleném rozsahu. |
| Filtry a výběr | Složka obsahující jen vyloučené soubory, vybrané podsložky, odlišný výběr panelů, skryté naplánované akce, změna filtru po porovnání. |
| Prázdné složky | Nová prázdná složka, zanořené prázdné složky, složka neprázdná po odfiltrování, obsah přidaný těsně před odstraněním. |
| Chyby a síť | Odpojení SMB během skenu a přenosu, zamčený soubor, odmítnutý přístup, plný disk, nečitelný podstrom; žádné odvozené mazání z chyb. |
| Zastaralý plán | Nový či změněný cíl, odstraněný zdroj, záměna adresáře za junction po náhledu; operace nesmí bez povšimnutí zasáhnout jiná data. |
| Přerušení a obnova | Přerušení před náhradou cíle, nedostupný Koš, chyba verzování, částečný běh a jeho protokol; původní cíl zůstává při nedokončené kopii zachován. |
| Cesty | ASCII, Unicode název i adresář, více Unicode komponent, UTF-8 přes 260 bajtů při kratším UTF-16, UTF-16 cesta přes 260 znaků, dlouhé UNC a protokol týchž cest. |
| Identita | Stejný kořen pod různým zápisem, vnořené kořeny, junction cyklus, odkaz mimo kořen, hardlinky a case kolize. |
| Časy | NTFS, FAT/exFAT a SMB s doloženou přesností; dvousekundový rozdíl, přechod letního času a explicitní ignorování data. |
| Rozhraní | Klávesnice, vysoké DPI, dlouhé přeložené řetězce, tmavé schéma Salamanderu a světlá schémata; chyby rozeznatelné bez barev. |
| Zátěž | Nejméně 100 000 položek pro první měření, hluboký strom, velké soubory a pomalá síť; zaznamenat paměť, průchodnost a dobu reakce na přerušení před stanovením závazných limitů. |
| Integrace | Načtení/uvolnění pluginu, zavření hostitele při běhu, obnova panelů, uložení profilu, balíček a lokalizace; build a smysluplné testy Win32/x64/ARM64. |

Rozhodovací tabulky ověřit jednotkově nad syntetickými stromy. Kopírování a obnovu ověřit integračně v dočasných adresářích s řízeně vyvolanými chybami. Rozhraní a součinnost s hostitelem ověřit ručně. Testy nesmějí používat skutečná uživatelská data jako cíl mazání.

## Otevřená rozhodnutí a ověření

### Rozhodnutí uživatele před implementací

| ID | Rozhodnutí | Doporučený začátek | Důsledek jiné volby |
| --- | --- | --- | --- |
| D1 | Mají být FTP/SFTP a archivy už v první verzi? | Místní a UNC/SMB složky. | Vzdálené zdroje vyžadují návrh schopností a spolupráce s dalšími pluginy; nelze jen předat jejich cestu do Win32. |
| D2 | Má obousměrný režim přenášet smazání a rozpoznávat úpravy obou kopií? | Bezstavové chování ve stylu TC, bez automatického přenosu smazání. | Historický režim potřebuje perzistentní stav, pravidla inicializace, ztráty databáze a skutečné konflikty. |
| D3 | Stačí asymetrie TC, nebo je požadováno i přesné zrcadlení? | Asymetrie TC; přesné zrcadlení jako samostatný pozdější režim. | Přesné zrcadlení může přepsat novější cíl starším zdrojem. Musí mít odlišné označení a náhled. |
| D4 | Jak chránit přepsané a mazané soubory? | Navrhnout obnovitelný režim před povolením destruktivních akcí; při nedostupnosti obnovy akci zastavit. | Trvalé mazání musí být výslovná volba. Verzovací adresář přidává nároky na prostor, umístění a úklid. |
| D5 | Jak přesná má být shoda metadat a speciálních souborů? | Obsah běžných souborů, čas posledního zápisu a dohodnuté základní atributy; odkazy zřetelně vynechat. | Úplný klon včetně ACL, ADS a topologie odkazů podstatně rozšiřuje provedení i testy. |

### Technická a behaviorální ověření

| ID | Co uzavřít | Jak to ověřit |
| --- | --- | --- |
| V1 | Přesný význam Podle obsahu bez Ignorovat datum a interakce s asymetrií | Malá sada dvojic se stejnou/různou velikostí, časem a obsahem v konkrétním vydání TC. Zahrnout asymetrii se stejnými časy i ignorovaným datem; zaznamenat verzi a výsledky. |
| V2 | Vliv filtrů zobrazení a označení na provedení v TC | Kontrolovaný test s kopírováním i navrženým mazáním, včetně skrytých položek. Pak rozhodnout o věrnosti či výslovně popsané odchylce. |
| V3 | Obsah závěrečného dialogu TC a jeho výchozí volby | Ověřit přepisy, směry, atributy, NTFS oprávnění, prázdné složky a přerušení v konkrétním vydání. Stará wiki tuto část nepopisuje. |
| V4 | Sémantika výběru obou panelů a masek složek | Stanovit sjednocení/průnik výběrů, rekurzi a syntaxi. Příklady uvést přímo v nápovědě. |
| V5 | Dlouhé Unicode kořeny v SDK 0.16 | Trasovat export kořenové cesty a ověřit, zda potřebujeme minimální kompatibilní rozšíření SDK. |
| V6 | Bezpečné nahrazení, obnova, identity a metadata na Windows/SMB | Ověřit schopnosti API, síťové aliasy, kořeny/rodiče s reparse points a cloudové placeholdery. Nevyvozovat atomický běh celého stromu z atomické náhrady jednoho souboru. |

V1–V4 vyžadují cílené porovnání s konkrétním vydáním Total Commanderu. Online rešerše nenahrazuje provedený test aplikace. Nejasnosti nemají být potichu doplněny domněnkou v kódu.

## Navržené etapy vývoje po projednání

1. Uzavřít D1–D5 a V1–V5; schválit tabulky chování a rozsah první verze.
2. Ověřit technické předpoklady SDK a připravit kostru pluginu s lokalizovaným dialogem, tmavým schématem a správným životním cyklem.
3. Implementovat sken, čisté porovnání a náhled bez zápisů; otestovat rozhodování a cesty.
4. Uzavřít V6 včetně obnovy při přepisu; teprve potom povolit přepis existujících cílů. Doplnit kopírování s kontrolou zastaralého plánu, přerušením a protokolem.
5. Po ověření obnovy také pro odstraněné položky doplnit asymetrické mazání a prázdné složky, poté profily a dokončit ergonomii.
6. Provést akceptační scénáře a CI, připravit distribuci pluginu. Teprve potom řešit upstream PR a další režimy.

## Zdrojová základna a její meze

Primární referencí je oficiální návod TC a konkrétní odpovědi jeho autora. Komunitní wiki a diskuse dokládají popis funkcí a potřeby jednotlivých uživatelů. FreeFileSync a WinSCP slouží jako zdroje návrhových postupů; jejich chování se nepřipisuje Total Commanderu. Dokumentace Microsoftu vymezuje vlastnosti Windows API.

Všechny odkazy jsou u příslušných tvrzení. Data uvedená u historických požadavků nejsou potvrzením, že je dnešní TC stále neřeší. Návrh nevychází z převzetí zdrojového kódu, grafiky ani textů nápovědy TC. Autoři a projekty jsou uvedeni také v [přehledu poděkování](third_party.md).
