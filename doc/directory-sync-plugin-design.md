# Synchronizační plugin pro Samandarin

Redakční příprava exportu (17. října 2026): názvy referenčního programu a odkazy na jeho web jsou nahrazeny neutrálním označením. Historické zdroje v plném znění zachovává [archiv původního forku](https://github.com/majklvi/salamander/tree/af0e81bc1bccb6c733025fe31a97d53adf5cb32f/doc). Cesty s prefixem `reference-sync-probe-` jsou dokumentační aliasy, nikoli přejmenované testovací adresáře. Výsledky a meze ověření se nemění.

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


Stav k 15. říjnu 2026: D1–D5 a doplňující pravidla schválená. Na pokyn uživatele implementována [první etapa: kostra, sken, porovnání a náhled bez zápisů](directory-sync-plugin-implementation.md) nad nezměněným SDK Samandarin-0.16. V1–V4 dokládá [protokol předlohy](directory-sync-plugin-reference-observations.md); V5/V6 mají [technický audit](directory-sync-plugin-sdk.md). Navazujícím pokynem byla implementována [kopie do nových cílů](directory-sync-plugin-copy-new.md), [samostatné uchování původních cílových souborů s jednoduchým protokolem](directory-sync-plugin-recovery.md) a nyní [přepisy podle D4/D5](directory-sync-plugin-overwrite.md) se zapamatovanou volbou obnovy, zachováním cílové DACL a ověřením dat/ADS/metadat. Navazuje [asymetrické mazání souborů s volitelnou obnovou](directory-sync-plugin-delete.md). Mazání složek, SMB testy a změny SDK/hostitele zůstávají mimo rozsah. Nižší počty testů a starší omezení ve starších etapách jsou historická.

Cílem je plugin pro porovnání a synchronizaci dvou adresářových stromů s pracovním postupem známým z referenčního správce souborů: **Porovnat → prohlédnout a upravit navržené operace → Synchronizovat**. Uživatel musí před zápisem vidět, co se zkopíruje, přepíše nebo odstraní. Tento dokument vymezuje chování předlohy, navržený rozsah pluginu, bezpečnost operací a rozhodnutí potřebná před zahájením vývoje.

**Doplněné schválení z 15. října 2026:** uživatel schválil doporučení 1–4 po ověření V1–V6: toleranci 3 s včetně bez hodinové korekce, provádění pouze viditelných zařazených akcí, sjednocení panelových výběrů s přímými dětmi vybrané složky a základní masky bez regex/uložených hledání či globálního úklidu prázdných složek. U V5 rozhodl jinak: nyní pouze plugin pro současný Samandarin-0.16, **bez rozšíření SDK a bez změny hostitele**. Omezení této hranice přijímá; jejich rozsah se ověří a zdokumentuje během vývoje. Následný pokyn uživatele z 15. října zahájil pouze první etapu bez zápisů.

Uživatel v D1 rozhodl, že první verze bude podporovat pouze místní a SMB složky: místní disky včetně USB, UNC cesty a mapované SMB jednotky. FTP/SFTP, archivy a další pluginové souborové systémy zůstávají mimo první verzi. V D2 zvolil pro první verzi variantu A bez historie: obousměrné porovnání vychází z aktuálního stavu a neodvozuje automatické mazání z chybějícího protějšku. V D3 zvolil variantu A: jednosměrný režim bude pouze asymetrie podle předlohy; samostatné přesné zrcadlení je odložené. V D4 zvolil variantu A: vlastní složku obnovy pro přepisy i mazání. V D5 schválil variantu A podle projednané tabulky: běžná data a vybraná metadata, podmíněné zachování ADS a výslovné meze pro speciální soubory. Ruční spouštění zůstává doporučeným návrhem.

## Schválená úroveň spolehlivosti (15. října 2026)

Uživatel výslovně stanovil jako cíl **úroveň současného jádra Samandarin-0.16 a jeho souborových operací**. D1–D5 zůstávají schválené včetně nezávislé obnovy před přepisem/mazáním. Současné read-only kontroly se zachovávají. Nevyvíjí se vlastní transakční filesystem backend a neslibuje se atomická neměnnost stromu či všech ADS při libovolném souběhu.

Závazné minimum, odložené záruky a další krok určuje [aktualizovaný návrh provedení a obnovy](directory-sync-plugin-execution-protocol.md), který nahrazuje dřívější přísnější technické požadavky V6. Pokročilé reparse/ACL/ADS závody, nativní relativní API infrastruktura, transakční journal a fyzické power-loss kampaně nejsou podmínkou zahájení běžného vývoje. Základní bezpečnost cest, zjištěné změny plánu, běžné chyby, Stop a podporované vlastnosti se dál ověřují.

Mapování existujících postupů jádra a SDK na D4/D5, Unicode/dlouhé cesty a běžné akceptační scénáře je [zdokumentováno samostatně](directory-sync-plugin-core-adapter.md). Doporučený malý privátní adaptér nad wide Win32 API se nepokouší volat interní worker ani rozšiřovat SDK. Po samostatném pokynu má následovat jen kopie do nového cíle; přepis/mazání později s ověřenou obnovou. Následným pokynem uživatel povolil úzkou copy-only etapu, jejíž [výsledky a meze](directory-sync-plugin-copy-new.md) jsou zdokumentované. Přepisy, mazání, SMB testy a změny SDK/hostitele zatím nejsou povoleny.

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

## Co znamená kompatibilita s referenčním správcem souborů

Oficiální návod předlohy popisuje výběr levého a pravého adresáře, případně vybraných podsložek, porovnání, seznam rozdílů se směry kopírování a následné potvrzení synchronizace. Novější soubor běžně navrhuje směr přenosu; rozdílné soubory se stejným datem nemají jednoznačný směr. Přepínače rozlišují položky pouze na jedné straně a na obou stranách. Oficiální návod předlohy (historický zdroj v archivu původního forku)

Odkazovaná komunitní wiki, naposledy upravená 13. 10. 2009, rozepisuje rekurzi, masky, práci s výběrem, obsahové porovnání, ignorování data a ruční změnu akce. U ignorování data připouští shodu podle jména a velikosti; společně s porovnáním obsahu rozhoduje obsah. Změny porovnávacích voleb vyžadují nové porovnání. Popisuje i FTP a časový posun, ale výslovně postrádá vysvětlení závěrečného dialogu. Je vhodným přehledem, nikoli úplnou specifikací současné verze. Synchronize directories (historický zdroj v archivu původního forku)

Kontextová nabídka dovoluje operace nad jednotlivými soubory. Aktuální oficiální přehled funkcí uvádí také synchronizaci prázdných adresářů; jejich absence v krátkém wiki popisu proto neznamená, že je referenční program neumí. Ukázka synchronizace (historický zdroj v archivu původního forku), Přehled funkcí předlohy (historický zdroj v archivu původního forku)

### Dva zásadní rozdíly mezi režimy

**Asymetrický režim předlohy není automaticky přesná kopie zdroje.** Autor 6. 9. 2024 potvrdil, že levá strana je originál a pravá záloha, ale novější soubor vpravo se standardně neoznačí k přepsání starším souborem vlevo; směr lze zadat ručně. Nesmíme tedy bez rozhodnutí zaměnit kompatibilní asymetrický režim za bezpodmínečné zrcadlení. Odpověď autora předlohy (historický zdroj v archivu původního forku)

**Obousměrné porovnání není synchronizace s historií.** Z pouhé přítomnosti souboru vlevo nelze odlišit nově vytvořený soubor od souboru, jehož protějšek uživatel vpravo smazal. Stejně tak aktuální dvojice časů nestačí k rozpoznání dvou nezávislých úprav. Autor předlohy tento limit vysvětluje potřebou předchozího stavu. FreeFileSync pro historický režim používá databázi `sync.ffs_db`; pro detekci přesunů potřebuje i stabilní identifikátory souborů. Autor předlohy o konfliktech (historický zdroj v archivu původního forku), [FreeFileSync o režimech](https://freefilesync.org/manual.php?topic=synchronization-settings)

## Požadavky uživatelů a poučení z předlohy

Jde o konkrétní veřejné požadavky a hlášení, nikoli statistický průzkum četnosti. Staré požadavky nejsou automaticky dnešními chybami předlohy. Priorita v posledním sloupci je návrh pro náš plugin.

| Potřeba a období | Co zdroj skutečně ukazuje | Dopad na návrh |
| --- | --- | --- |
| Zřetelné vylučování souborů a složek, 2007–2009 | Uživatelé žádali srozumitelnější filtry; masky pro vylučování již existovaly. Část problému byla jejich dohledatelnost. Diskuse o maskách (historický zdroj v archivu původního forku) | První verze: oddělená pole Zahrnout a Vyloučit, příklady a viditelné shrnutí aktivních omezení. |
| Vynechání celého podstromu před drahým porovnáním, 2007 | Komunitní návrh žádá možnost vyřadit složku i velký soubor. Návrhy pro Sync window (historický zdroj v archivu původního forku) | První verze: filtry použít před čtením obsahu; příkaz Vyloučit podstrom přidá pravidlo a vyžádá nové porovnání. |
| Čerstvé údaje vzdáleného stromu, 2007 | Historické hlášení upozorňuje na obavu ze zastaralé FTP cache; nedokládá současnou chybu. Návrhy pro Sync window (historický zdroj v archivu původního forku) | Každé nové porovnání provede čerstvý sken; budoucí vzdálené adaptéry musí definovat platnost cache. |
| Organizace uložených dvojic a profilů, 2024–2025 | Ukládání profilů existuje, požadavek se týká organizace a pohodlnějšího návratu k cestám. Diskuse o profilech (historický zdroj v archivu původního forku) | První verze: pojmenované profily cest, režimu a filtrů. Skupiny, hledání a dávky až podle potřeby. |
| Nejprve porovnat obsah a teprve u rozdílů určit směr podle času, 2009 | Uživatel popisuje, že volba Ignorovat datum sice odstraní falešné obsahové rozdíly, ale ztratí návrh směru. Jde o historický požadavek na jinou kombinaci pravidel. Diskuse o shodném obsahu (historický zdroj v archivu původního forku) | Při ověření V1 rozhodnout, zda zachovat přesné volby předlohy, nebo nabídnout výslovně odlišený režim „obsah určuje shodu, čas směr“. |
| Rozpoznání přejmenovaných či přesunutých souborů, 2023 | Uživatelé žádají porovnání přes rozdílná jména; samotná shoda obsahu však nemusí určit jednoznačnou dvojici. Diskuse o ignorování jmen (historický zdroj v archivu původního forku) | Pozdější rozšíření. První verze páruje relativní cesty a neprovádí heuristické přesuny. |
| Bezpečné zacházení s prázdnými složkami, 2008 | Autor upozorňuje, že filtr může skrýt skutečný obsah složky, takže se jen zdá prázdná. Odpověď o prázdných adresářích (historický zdroj v archivu původního forku) | Povinné pravidlo: prázdná po filtraci neznamená skutečně prázdná; nepoužívat jako podklad pro rekurzivní smazání. |

Některá vylepšení už referenční program má: autor v diskusi k verzi 11.02 referenčního programu popisuje dostupnost filtrů klávesnicí a pro čtečky obrazovky; oficiální oznámení verze 11.02 uvádí pozastavení porovnávání. Dostupné ovládání, přehledný průběh a možnost přerušení proto patří mezi očekávání od náhrady, nikoli mezi tvrzené novinky proti předloze. Pozastavení běhu je kandidát pro další iteraci pluginu. Autor o přístupnosti filtrů (historický zdroj v archivu původního forku), Oficiální oznámení verzí (historický zdroj v archivu původního forku)

## Navržený rozsah první verze

| Oblast | První verze a stav rozhodnutí | Pozdější rozšíření |
| --- | --- | --- |
| Umístění | **Schváleno v D1:** dvě místní nebo SMB složky v libovolné kombinaci; místní disky, USB, UNC a mapované SMB jednotky | FTP/SFTP, archivy a jiné pluginové souborové systémy mimo první verzi |
| Spouštění | Příkaz v nabídce pluginu, cesty z fyzického levého a pravého panelu, ruční změna cest | Více párů v jedné úloze, dávky a plánování |
| Rozsah stromu | Rekurze, výběr podsložek, zahrnovací a vylučovací masky, prázdné složky | Složitější pravidla podle atributů, věku a velikosti |
| Porovnání | Velikost a čas; obsahové porovnání; ignorování data | Trvalá cache otisků, specializované textové porovnání |
| Režimy | **Schváleno v D2 a D3:** obousměrné kopírování bez historie a bez automatického přenosu smazání; jednosměrně pouze asymetrie podle předlohy. | Historický režim, přenos smazání v obousměrném režimu a samostatné přesné zrcadlení mimo první verzi. |
| Výsledek | Dvousloupcový seznam, stav a akce, hromadná změna směru, přeskočení, přehled operací | Detekce přejmenování, hierarchické seskupování velkých úloh |
| Provedení | Průběh, přerušení, opakování chybné položky, souhrn a protokol | Pokračování celé úlohy po restartu aplikace |
| Obnova souborů | **Schváleno v D4:** vlastní složka obnovy pro přepisy i mazání, mimo oba synchronizované stromy; ruční obnova a úklid | Automatická obnova celého běhu a pravidla automatického promazávání |
| Metadata a speciální soubory | **Schváleno v D5:** varianta A podle podrobné tabulky níže; čas zápisu a základní atributy, cílová pravidla oprávnění, podmíněný přenos ADS | Samostatná synchronizace dalších metadat, přenos zdrojových oprávnění a širší zachování vlastností filesystému |
| Nastavení | Pojmenované profily; uložené volby vždy viditelně zobrazené | Import profilů předlohy, sdílené profily, skupiny |
| Integrace | Unicode, dlouhé cesty, lokalizace a světlé/tmavé schéma Salamanderu | Samostatné okno mimo proces hostitele |

SMB zahrnuje sdílené složky jiného počítače či NAS, dostupné přes UNC nebo mapovanou jednotku. Zůstávají požadavky na přerušení spojení, oprávnění, dlouhé Unicode cesty a neúplný sken; podpora SMB není příslibem podpory všech síťových protokolů. Schválené jsou D1–D5 a doplňující rozhodnutí výše; ostatní řádky tabulky zůstávají návrhy. Běh jako služba na pozadí se nenavrhuje; historie synchronizace a samostatné přesné zrcadlení jsou podle D2 a D3 mimo první verzi.

## Navržená pravidla porovnání a plánování

Následující tabulky jsou kontrakt navrhovaného pluginu. Detaily, jejichž přesná shoda s předlohou ještě není doložena, jsou uvedeny v části Rozhodnutí a zbývající ověření.

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

Při asymetrii předlohy se položky přítomné pouze vlevo kopírují doprava a položky přítomné pouze vpravo navrhují k odstranění. Pro existující dvojici návrh zachovává kopírování novějšího levého souboru doprava a žádnou akci u shody. Novější pravý soubor se nepřepisuje automaticky; zobrazí se jako nevybraná odlišná položka s vysvětlením. V referenčním programu 11.58 byly při respektování data novější pravé dvojice nevybrané; rozdíl při stejném čase nebo ignorovaném datu zůstal bez automatického směru i v asymetrii. Podrobnou matici a dosud neověřené časové hranice uvádí [protokol V1](directory-sync-plugin-reference-observations.md). Ruční přepsání musí být viditelné v závěrečném souhrnu.

Samostatné přesné zrcadlení je podle schváleného D3 mimo první verzi. Případné budoucí rozšíření by považovalo levou stranu za autoritu i proti novější pravé verzi; musí mít vlastní název a nesmí být skrytou změnou významu asymetrie. V obousměrném režimu bez historie se automatické mazání nikdy neodvozuje z pouhé absence protějšku.

### Metody porovnání

| Volby | Navržený význam |
| --- | --- |
| Velikost a čas | Shoda metadat znamená pouze shodu podle těchto kritérií. Obsah nemusí být stejný. |
| Ignorovat datum | U párů rozhoduje velikost; rozdílné páry nedostanou automatický směr podle času. Jednostranné položky si zachovají směr podle přítomnosti. |
| Podle obsahu a ignorovat datum | Různá velikost je rozdíl, u stejné velikosti rozhoduje binární obsah. Rozdílné páry vyžadují ruční směr. |
| Podle obsahu a respektovat datum | V referenčním programu 11.58 vede rozdílný čas mimo toleranci k návrhu podle času i při shodném obsahu. Při shodném čase a velikosti obsahové porovnání odhalí rozdíl. Náš kompatibilní návrh má zachovat tyto výsledky; test UI sám nedokládá interní pořadí čtení předlohy. |

Obsahové porovnání má používat proudové čtení s možností přerušení a skončit při prvním rozdílu. Nevyžaduje načtení celého souboru do paměti. Kontrolní součet pro cache ani následná kontrola zkopírovaného cíle nejsou totéž co samotné porovnání zdrojů.

Časy porovnáváme interně v UTC. Tolerance musí být výslovná a otestovaná podle filesystému, nikoli skrytá konstanta použitá na vše. FAT má dvousekundovou přesnost času zápisu a odlišné zacházení s místním časem oproti NTFS. Posun o hodinu nelze automaticky považovat za neškodný. [Microsoft o časech souborů](https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times)

Doplňující test referenčního programu 11.58 a jeho dodaná nápověda podporují návrh výchozí kompatibilní tolerance **3 s včetně**, viditelné ve volbách. Rozdíl +3.000 s byl shoda, +3.001 s již novější soubor; +3599/+3600/+3601 s nebyly ignorovány. Pro v1 navrhujeme žádnou automatickou hodinovou korekci. To je politika porovnání, nikoli tvrzení o přesnosti NTFS nebo všech SMB serverů. Podrobné meze ověření uvádí [protokol](directory-sync-plugin-reference-observations.md).

### Filtry a výběr

Rozlišit tři věci: rozsah skenu, zobrazení výsledků a výběr operací. Masky a vybrané podstromy vymezují sken. Přepínače shodné/rozdílné/jednostranné vymezují zobrazení. Akce řádku a jeho zařazení do provedení určují skutečný plán.

Změna cesty, porovnávací metody nebo rozsahu skenu zneplatní výsledek. Vyloučené položky musí být chráněné na obou stranách a nesmějí se stát kandidáty k odstranění jen proto, že nebyly načteny. „Pouze vybrané“ se nabídne jen pro zachytitelný a jednoznačný výběr. Referenční program 11.58 použil sjednocení výběrů včetně neoznačených protějšků; doplněný test potvrzuje výběr jen vpravo i směs souboru a složky. Navrhujeme stejnou sémantiku. U explicitně vybrané složky se zahrnou přímé děti i bez rekurze; rekurze přidá hlubší úrovně. Při tomto režimu jsou kořeny zamčené, jejich změna vyžaduje opuštění omezení původním výběrem.

V referenčním programu 11.58 skrytí řádků změnilo souhrn před provedením a skutečné kopírování; ručně vynechaná kopie se také neprovedla. Skryté mazání bylo ověřeno v souhrnu, běžné zobrazené mazání bylo provedeno na dvou schválených testovacích položkách. Podrobnosti uvádí [protokol V2/V3](directory-sync-plugin-reference-observations.md). Doporučení je zachovat vazbu zobrazení a provádění pro kompatibilitu a zobrazit odděleně prováděné a filtrované akce. Oddělení datového modelu pohledu a plánu nesmí znamenat neohlášené provádění skrytých položek.

Ověřené příklady masek: `*.txt | selB\` vyloučí všechny stejnojmenné podstromy, `*.txt | \selB\` jen podstrom v kořeni, `\*.txt` jen kořenové soubory, `selA\**\*.txt` přímé i hlubší soubory pod selA. Pro v1 navrhujeme tuto základní cestovou gramatiku a názvové masky, bez regulárních výrazů a uložených hledání předlohy; přesné escapování a oddělovače je třeba dopracovat a otestovat při tvorbě parseru a nápovědy.

Prázdné složky patří do explicitního plánu. Globální volbu předlohy Delete all empty directories navrhujeme nepřebírat: podle nápovědy může mazat i složky přítomné vlevo. Její zpřístupnění a globální provedení nebyly ověřeny. Normální asymetrické odstranění složky pouze vpravo bylo ověřeno a společná prázdná složka zůstala zachována.

## Navržené rozhraní a průběh práce

1. Otevřít plugin s levou a pravou cestou podle skutečných stran panelů, nikoli podle aktivního panelu. Zobrazit režim, rekurzi, masky, volby porovnání a případně profil.
2. Spustit Porovnat. Sken probíhá mimo vlákno rozhraní, s průběžným počtem položek a možností přerušení. Porovnání nic nezapisuje do synchronizovaných stromů.
3. Zobrazit název a metadata vlevo, akci uprostřed a metadata vpravo. Směr a mazání mají ikonu i text; barva je doplněk. U rozdílu lze ukázat důvod a podle možností otevřít porovnání dvojice souborů.
4. Umožnit jednotlivě i hromadně změnit směr či přeskočit položku. Nejednoznačné a chybové položky nepovažovat za automaticky vyřešené. Úprava souboru v externím nástroji vyžádá nové ověření jeho řádku.
5. Synchronizovat otevře souhrn konkrétního plánu: počet a objem přenosů v obou směrech, počet přepisů a mazání, způsob obnovy, konflikty a skryté vybrané položky. Změna plánu po potvrzení vyžaduje nové potvrzení.
6. Během provádění zobrazit aktuální operaci, průběh a Přerušit. Po skončení odlišit Úspěch, Dokončeno s přeskočenými položkami, Přerušeno a Chyba. Protokol obsahuje skutečně provedené operace a jejich výsledky.

Podobné oddělení náhledu změn a provedení nabízí WinSCP; mazání má samostatnou volbu a v běžném obousměrném režimu není odvozováno z absence souboru. [WinSCP Synchronize Dialog](https://winscp.net/eng/docs/ui_synchronize)

## Bezpečnostní pravidla navrhovaného pluginu

Tato pravidla jsou součástí návrhu našeho provedení, nikoli tvrzení, že je referenční program implementuje stejně.

| Situace | Požadované chování |
| --- | --- |
| Kořen neexistuje, síť se odpojí, přístup je odmítnut | Chybu nezaměnit za prázdný adresář. Nepovolit odvozené mazání; pro první verzi zablokovat provedení při neúplném skenu. |
| Záměrně prázdný zdroj | Je platný jen po úspěšném ověření kořene a enumerace. Hromadné odstranění v cíli musí být výslovně vidět v souhrnu. |
| Zdroj či cíl se změní po porovnání | Před operací ověřit očekávanou existenci, typ, identitu a relevantní metadata. Při odchylce označit zastaralý plán a položku neprovést. Použít dostupné kontroly otevřeného objektu; neslibovat odstranění všech závodů mezi kontrolou a operací ani kontinuální uzamčení stromu. |
| Chyba či přerušení při přepisu | Nejprve kopírovat do jedinečného dočasného souboru na cílovém filesystému. Původní cíl nahradit až po úspěchu a opětovném ověření. Definovat úklid a chování při plném disku. |
| Mazání nebo přepis uživatelských dat | Podle D4 uchovat původní soubor ve vlastní složce obnovy mimo oba synchronizované stromy. Neúspěšné uchování zablokuje příslušný přepis či odstranění; nepřecházet na trvalou operaci bez obnovy. |
| Prázdné složky a filtry | Rozlišit skutečnou prázdnost od vyfiltrovaného obsahu. Složku odstranit jen po ověření skutečné prázdnosti; nikdy rekurzivně nesmazat neznámé či vyloučené děti. |
| Shodné nebo vnořené kořeny | Porovnat cesty i dostupné identity a odmítnout shodné či překrývající se stromy v obou směrech. Při nevyřešené možné kolizi neprovádět zápisy. Detekce libovolných SMB aliasů není přislíbena; její meze a postup při nejistotě uzavře V6. |
| Symlinky, junctions a jiné reparse points | V první verzi nenásledovat; zobrazit vynechané položky a důvod. Chránit také odpovídající protějšek před odvozeným mazáním. Kontrola zahrne kořen a rodičovské komponenty; zjištěnou změnu na odkaz mezi skenem a operací odmítnout. Úplná ochrana proti úmyslné výměně v každém okamžiku se neslibuje. Ve V6 upřesnit dopad na cloudové placeholdery a další reparse typy. |
| Kolize jmen a typů | Žádné automatické sloučení různých názvů, normalizace Unicode ani soubor proti složce. Case kolize na odlišných filesystémech jsou konflikt. |
| Metadata a speciální soubory | Dodržet schválený rozsah D5; technické provedení ověřit ve V6. Nepodporované vlastnosti nesmějí vést k tiché ztrátě dat. Úplný klon filesystému se neslibuje. |

Microsoft dokumentuje, že výsledek enumerace může být zastaralý už před následnou operací a že chyby je nutné rozlišovat přes `GetLastError`. Návrh proto potřebuje vlastní zacházení s neúplným skenem a změnami stavu. [FindFirstFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew)

Dočasná kopie chrání před nedokončeným přepisem, ale nezajišťuje transakci celého stromu ani obnovu při každém výpadku napájení. Autor FreeFileSync také rozlišuje tuto ochranu od následného ověření obsahu. Historie či Koš zase chrání jiný případ, omylem schválenou změnu. [FreeFileSync o fail-safe copy](https://freefilesync.org/forum/viewtopic.php?t=1495), [FreeFileSync o verzování](https://freefilesync.org/manual.php?topic=versioning)

Názvy a cesty mají zůstat v Unicode, bez průchodu přes ztrátové ACP převody. Dlouhé cesty se musí řešit v celém řetězci včetně UNC, dialogů, protokolu a obnovy; existence široké Win32 funkce sama o sobě nestačí. [Microsoft o dlouhých cestách](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)

### Schválená ochrana souborů podle D4

**Doplnění na pokyn uživatele:** uchování originálů je uživatelsky vypnutelné, zejména kvůli objemu velkých úloh. Výchozí stav je zapnutý, plugin si pamatuje zapnutí/vypnutí i obnovovací složku přes existující konfiguraci SDK. Následující požadavky na zálohu před destruktivní operací platí při zapnutém uchování. Při vypnutí se obnova nevytváří a složka obnovy není podmínkou operace; potvrzení přepisu tento stav jasně uvádí, budoucí mazání bude mít stejný požadavek. Staging, Verify, D5 vlastnosti cíle a běžné kontroly se tím nevypínají. Vypnutí nemaže již uložené zálohy. Přepisy jsou nově implementované a [ověřené běžnými testy](directory-sync-plugin-overwrite.md), mazání zatím nikoli.

Profil určí vlastní složku obnovy mimo oba synchronizované stromy. Před přepisem se uchová původní cílový soubor na měněné straně; před odstraněním se uchová odstraňovaná položka. Uspořádání oddělí dvojici kořenů, jednotlivé běhy a levou/pravou stranu a zachová relativní cestu i původní název. Uložené kopie se nesmějí navzájem přepsat kvůli kolizi názvů nebo času běhu.

Pokud je složka obnovy nedostupná, nezapisovatelná nebo nemá dost místa, příslušná destruktivní operace se neprovede. Uchování ani následné selhání přepisu nesmějí ztratit původní soubor. Dočasná kopie a ověření stavu před nahrazením cíle zůstávají samostatnými povinnostmi provedení; celý synchronizační běh není atomická transakce.

První verze nabídne otevření složky obnovy a protokol s mapováním původních a uložených cest. Obnova i úklid budou ruční, bez automatického promazávání. Podporovaná metadata a speciální typy souborů určuje schválené D5 také pro uchované kopie. Uložené soubory se nepoužijí jako historický základ porovnávání: rozhodnutí D2 zůstává beze změny.

### Schválené zachování vlastností podle D5

Uživatel schválil pro první verzi variantu A podle projednané tabulky: běžné datové soubory se zachováním vybraných vlastností při přenosu. Varianta B je odložena; přidala by samostatnou synchronizaci dalších rozdílů metadat, přenos zdrojových oprávnění a širší podporu speciálních vlastností filesystému. Ani tato budoucí varianta by sama o sobě nebyla příslibem obrazu disku.

| Oblast | Schválená varianta A |
| --- | --- |
| Data, názvy a struktura | Zachovat obsah souborů, názvy a adresářovou strukturu ve vybraném rozsahu. |
| Časy | Zachovat čas posledního zápisu v přesnosti podporované cílem. Čas vytvoření, posledního přístupu a časy složek nezahrnovat do samostatné synchronizace ani neslibovat jejich přesnou shodu. |
| Základní atributy | Při přenosu zachovat Jen pro čtení, Skrytý a Systémový. Samotný rozdíl těchto atributů nevyvolá operaci. |
| Oprávnění | Nekopírovat vlastnictví, audit ani přístupová oprávnění ze zdroje. Nové soubory mají dědit oprávnění cílové složky, přepsané zachovat existující přístupová oprávnění cíle. Selhání požadovaného zachování není úspěšný přepis. |
| Symbolické odkazy, junctions a cloudové placeholdery | Viditelně vynechat a chránit odpovídající protějšek před odvozeným mazáním; platí kontroly kořene a rodičů ve V6. Běžný soubor zástupce `.lnk` se kopíruje jako soubor. |
| Hardlinky | Kopírovat jako nezávislé soubory, bez zachování vazby mezi jmény. Přepis nesmí upravit data ani atributy jiných jmen téhož cílového souboru. Pokud to backend neumí zajistit, operaci zablokovat. Obnovovací kopie musí být nezávislá na původních hardlinkách. |
| Doplňkové datové proudy ADS | Při kopírování přenést proudy zdroje, pokud je umí zachovat cíl a při zapnutém uchování také složka obnovy. Staré cílové proudy při zapnutém uchování uložit v obnovovací kopii, neslučovat je náhodně do nového cíle. Při nepodporované operaci položku zablokovat a protějšek chránit. Samostatné rozdíly ADS nevyvolají synchronizaci; obsahové porovnání označuje shodu hlavního datového proudu. |
| Šifrování EFS | V první verzi viditelně vynechat operace s EFS na kterékoli dotčené straně, včetně mazání a uchování původního cíle. Nepovolovat tichý přenos do nešifrované podoby. |
| Komprese a sparse soubory | Zachovat logický obsah, neslibovat stejné úsporné uložení. Předem počítat s potřebným prostorem v cíli i obnově; kopie může zabírat více místa. |

Ochrana D4 se vztahuje na data a podporované vlastnosti podle D5. Varianta A neslibuje automatickou obnovu původního vlastnictví, oprávnění nebo vazeb hardlinků. Ztráta podporovaných dat či atributů nesmí být skrytá za výsledkem Úspěch. Zacházení s atributem Archive při zápisu je technický detail k upřesnění ve V6; nebylo součástí schválené tabulky a nepřidává samostatné porovnávání atributů.

ADS mohou obsahovat další data připojená k souboru. Hardlinky jsou více názvů jednoho souboru, takže pouhý přesun jednoho jména do složky obnovy nevytvoří nezávislou kopii. Tyto vlastnosti jsou důvodem pro výslovná pravidla přenosu a obnovy. [Microsoft o datových proudech](https://learn.microsoft.com/en-us/windows/win32/fileio/file-streams), [Microsoft o hardlinkách](https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions)

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

Konkrétní omezení SDK a integrační body doplňuje [technický průzkum základu](directory-sync-plugin-sdk.md). První etapa používá existující panelový export s přijatými omezeními V5. Před přidáním zápisů je nutné prokázat bezpečnost podle V6. Nesmíme předpokládat dostupnost API z jiných rozpracovaných větví.

## Akceptační scénáře před vydáním

Scénáře níže tvoří širší testovací plán včetně budoucích zápisů. Provedené případy první etapy (místní cesty, náhled a Stop/Close) a jejich omezení zaznamenává [protokol implementace](directory-sync-plugin-implementation.md); zbývající případy nelze považovat za ověřené.

| Oblast | Minimální příklady a očekávání |
| --- | --- |
| Základní rozhodování | Jen vlevo/vpravo, stejné soubory, novější vlevo/vpravo, stejný čas a různá velikost, stejná metadata a různý obsah. Ověřit akce každého režimu. |
| Obsah a datum | Všechny kombinace obsahu a ignorování data; stejný obsah s jiným časem; prázdné soubory; soubory větší než 4 GiB; změna během čtení. |
| Asymetrie | Novější cíl se automaticky nepřepíše; ruční přepsání je v souhrnu. Jednostranné cílové položky se mažou jen ve schváleném rozsahu. |
| Filtry a výběr | Složka obsahující jen vyloučené soubory, vybrané podsložky, odlišný výběr panelů, skryté naplánované akce, změna filtru po porovnání. |
| Prázdné složky | Nová prázdná složka, zanořené prázdné složky, složka neprázdná po odfiltrování, obsah přidaný těsně před odstraněním. |
| Chyby a síť | Odpojení SMB během skenu a přenosu, zamčený soubor, odmítnutý přístup, plný disk, nečitelný podstrom; žádné odvozené mazání z chyb. |
| Zastaralý plán | Nový či změněný cíl, odstraněný zdroj, záměna adresáře za junction po náhledu; operace nesmí bez povšimnutí zasáhnout jiná data. |
| Přerušení a obnova | Přerušení před náhradou cíle, nedostupná/plná složka obnovy, kolize uložených cest a oddělení stran, chyba uchování, částečný běh a ruční obnova podle protokolu; původní cíl zůstává při nedokončené kopii zachován. |
| Cesty | ASCII, Unicode název i adresář, více Unicode komponent, UTF-8 přes 260 bajtů při kratším UTF-16, UTF-16 cesta přes 260 znaků, dlouhé UNC a protokol týchž cest. |
| Identita | Stejný kořen pod různým zápisem, vnořené kořeny, junction cyklus, odkaz mimo kořen, hardlinky a case kolize. |
| Časy | NTFS, FAT/exFAT a SMB s doloženou přesností; dvousekundový rozdíl, přechod letního času a explicitní ignorování data. |
| Rozhraní | Klávesnice, vysoké DPI, dlouhé přeložené řetězce, tmavé schéma Salamanderu a světlá schémata; chyby rozeznatelné bez barev. |
| Zátěž | Nejméně 100 000 položek pro první měření, hluboký strom, velké soubory a pomalá síť; zaznamenat paměť, průchodnost a dobu reakce na přerušení před stanovením závazných limitů. |
| Integrace | Načtení/uvolnění pluginu, zavření hostitele při běhu, obnova panelů, uložení profilu, balíček a lokalizace; build a smysluplné testy Win32/x64/ARM64. |

Rozhodovací tabulky ověřit jednotkově nad syntetickými stromy. Kopírování a obnovu ověřit integračně v dočasných adresářích s řízeně vyvolanými chybami. Rozhraní a součinnost s hostitelem ověřit ručně. Testy nesmějí používat skutečná uživatelská data jako cíl mazání.

## Rozhodnutí a zbývající ověření

### Rozhodnutí uživatele před implementací

| ID | Rozhodnutí | Dohodnutý stav nebo doporučení | Důsledky a alternativy |
| --- | --- | --- | --- |
| D1 | Podporovaná umístění první verze | **Rozhodnuto uživatelem:** zatím jen místní a SMB složky, včetně USB, UNC a mapovaných SMB jednotek. | Místní ↔ místní, místní ↔ SMB i SMB ↔ SMB. FTP/SFTP, archivy a jiné pluginové souborové systémy odloženy; první verze nepotřebuje jejich provider API. |
| D2 | Historie obousměrné synchronizace | **Rozhodnuto uživatelem:** pro první verzi varianta A bez historie a bez automatického přenosu smazání. | Jednostranný soubor může být navržen ke zkopírování zpět; dvě nezávislé úpravy nelze spolehlivě rozpoznat bez předchozího stavu. Náhled a ruční volba směru zůstávají podstatné. Výslovně zvolené jednosměrné mazání řeší D3. |
| D3 | Jednosměrný režim první verze | **Rozhodnuto uživatelem:** varianta A, pouze asymetrie podle předlohy; samostatné přesné zrcadlení odloženo. | Položky pouze vpravo mohou být navrženy k odstranění. Novější pravý soubor se automaticky nepřepíše ani nekopíruje zpět; ruční přepsání zůstává možné. Přesné kombinace porovnávacích voleb uzavře V1. |
| D4 | Ochrana přepsaných a mazaných souborů | **Rozhodnuto uživatelem:** pro první verzi varianta A, vlastní složka obnovy pro přepisy i mazání, mimo oba synchronizované stromy. | Při chybě uchování se příslušná destruktivní operace neprovede. Obnova a úklid jsou v první verzi ruční; kopie jsou oddělené podle dvojice kořenů, běhu, strany a relativní cesty. Vyžaduje další prostor a někdy další přenos dat. |
| D5 | Metadata a speciální soubory | **Rozhodnuto uživatelem:** pro první verzi varianta A podle podrobné tabulky výše: běžná data, čas zápisu, základní atributy a podmíněný přenos ADS; bez přenosu zdrojových oprávnění. | Odkazy, cloudové placeholdery a EFS se vynechávají; hardlinky se kopírují nezávisle; komprese a sparse uložení se nezaručují. Samotné rozdíly atributů či ADS nevyvolají synchronizaci. Varianta B je odložena. |

### Technická a behaviorální ověření

| ID | Co uzavřít | Stav a další ověření |
| --- | --- | --- |
| V1 | Přesný význam Podle obsahu bez Ignorovat datum a interakce s asymetrií; časová tolerance | Osm kombinací a doplňující hraniční časy doloženy. Návrh výchozí tolerance 3 s včetně, bez automatické hodinové korekce. Jiné filesystémy a SMB runtime neověřeny. |
| V2 | Vliv filtrů zobrazení a označení na provedení v předloze | Ověřen souhrn i skutečné kopie při filtru a ručním vynechání. Návrh provádět jen viditelné zařazené akce, přehledně uvést skryté. |
| V3 | Obsah závěrečného dialogu předlohy a jeho výchozí volby v našem rozsahu | Provedeny kopie a schválené testovací mazání. Cancel při potvrzení přepisu ponechal cíl. Doplněno skutečné přerušení 2GiB přenosu: po OK (Stop) prázdný cíl, další položka se nekopírovala. Řízená změna posledního bytu při Verify vyvolala CRC error / The two files are DIFFERENT!; po Abort chybná kopie zůstala, další položka se nepřenesla. Ignore/Ignore all, přerušení přepisu a globální úklid neověřeny; globální úklid do v1 nenavržen. |
| V4 | Sémantika výběru obou panelů a masek složek | Doplněn pravostranný i smíšený výběr, přímé děti vybrané složky bez Subdirs, ukotvené masky a **. Zbývá přesná specifikace parseru a její testy; nikoli úplné převzetí hledacích funkcí předlohy. |
| V5 | Dlouhé Unicode kořeny v SDK 0.16 | Stávající SDK, bez rozšíření. Místní Unicode/emoji snapshot a kořeny 269/270 UTF-8 B při 129/130 UTF-16 jednotkách prošly; kořeny 278/279 UTF-16 fungují při ručním zadání v pluginu. Navigace hostitele do 278jednotkového kořene padá i bez pluginu; nelze tvrdit bezpečné nahlášení všech omezení ani úplnou podporu přes SDK. Důkazy, nativní matice a místní Stop/Close jsou v protokolu implementace. UNC/SMB a další délky zůstávají neověřené. |
| V6 | Provedení, obnova a metadata na úrovni jádra | Dosavadní API a NTFS sondy zůstávají evidencí. Dle nového rozhodnutí se odkládají úplné souběhové záruky, native backend a transakční journal. Zachovat D4/D5, běžné kontroly, chyby a Stop; konkrétní mechanismus vybrat mapováním jádra/SDK a ověřit při vývoji. SMB testy nyní nejsou povoleny. |

V1–V4 byly rozšířeny druhou sadou přes Computer Use v předloze 11.58. [Protokol](directory-sync-plugin-reference-observations.md) odlišuje pozorování, nápovědu a zbývající případy. [Audit SDK a WinAPI](directory-sync-plugin-sdk.md) obsahuje V5/V6 včetně konkrétní sondy a návrhu operací. Uživatel zvolil pokračování bez SMB testovací složky; žádný místní výsledek neprokazuje SMB. Nejasnosti nemají být potichu doplněny domněnkou v kódu.

Třetí sada dokončila požadované místní scénáře přerušení přenosu a chybového Verify. Referenční program při Abort po chybě ponechal již zapsanou vadnou kopii. Pro náš návrh musí neúspěšné ověření stagingu blokovat commit; chyba zjištěná až po commitu vyžaduje explicitní neúspěch a dostupnou obnovu podle D4. Nový náhled porovnání nenahrazuje záznam chyby běhu.

### Co zbývá před implementací a co se ověřuje až při ní

- **Produktová rozhodnutí D1–D5 a doplňující doporučení 1–4 jsou schválená.** Platí tolerance 3 s bez hodinové korekce, provádění pouze viditelných zařazených akcí, sjednocení výběrů včetně přímých dětí vybrané složky a základní cestové masky. Žádný globální úklid prázdných složek ani regulární výrazy/uložená hledání v1. Tyto body již nevyžadují další schválení.
- **Technické předpoklady:** V5 je rozhodnuté jako adaptér nad nezměněným SDK Samandarin-0.16 s přijatými a dokumentovanými omezeními; rozšíření SDK ani hostitele není podmínkou zahájení vývoje. V6 nyní požaduje postupy na úrovni jádra se zachováním D4/D5; dřívější handle-based transakční návrh není závazný. Konkrétní mechanismus přepisu se vybere mapováním jádra a SDK. Bezpečnost backendu a kompatibilitu pluginu musí prokázat testy během vývoje, destruktivní kroky před jejich povolením. Jde o technický návrh, nikoli o další povinné volby uživatele.
- **Dopracovat běžné detaily návrhu:** výchozí volby porovnání a rekurze, kolizně bezpečné názvy obnovovacích kopií, formát protokolu a hlášení nepodporovaných položek. Konkrétní umístění složky obnovy nastaví uživatel při používání; její meze a ruční úklid už určuje D4.
- **Při implementaci a před vydáním:** prokázat zachování dat a podporovaných vlastností, chování při přerušení, plném disku, ztrátě SMB, změně souboru či rodičovského adresáře a selhání obnovy. Doplnit integrační, UI a build kontroly podle testovací matice. Dosavadní NTFS sondy pokrývají jen výslovně zdokumentované případy; budoucí integrační testy nejsou zaměnitelné s rešerší a nelze je všechny dokončit před vznikem kódu. Pokročilé souběhové a power-loss kampaně jsou dle aktuálního rozhodnutí odložené.

## Navržené etapy vývoje po projednání

1. Na základě schválených D1–D5 a doplňujících rozhodnutí dopracovat běžné detaily chování a testovací případy; nové podstatné odchylky výslovně předložit.
2. Připravit kostru pluginu nad stávajícím SDK a ověřit načtení i panelový vstup na nezměněném Samandarin-0.16; lokalizovaný dialog, tmavé schéma a správný životní cyklus. Zdokumentovat zjištěná omezení cest.
3. Implementovat sken, čisté porovnání a náhled bez zápisů; otestovat rozhodování a cesty.
4. Implementovat a otestovat V6 včetně obnovy při přepisu; teprve po ověření povolit přepis existujících cílů. Doplnit kopírování s kontrolou zastaralého plánu, přerušením a protokolem.
5. Po ověření obnovy také pro odstraněné položky doplnit asymetrické mazání a prázdné složky, poté profily a dokončit ergonomii.
6. Provést akceptační scénáře a CI, připravit distribuci pluginu. Teprve potom řešit upstream PR a další režimy.

## Dlouhodobý plán — společné přenosové prostředky hostitele

**Odloženo na vzdálenější budoucnost, schváleno uživatelem 16. října 2026.** Až upravené API Salamandera zpřístupní potřebné služby pluginům, integrovat synchronizační plugin se společnými přenosovými prostředky jádra: nativním kopírovacím/přesouvacím oknem, přenosem na pozadí, nastavitelným rychlostním limitem a společnou frontou čekajících přenosů. Tyto funkce přednostně využít z hostitele namísto jejich samostatného duplikování v pluginu.

Současné SDK 0.16 poskytuje jednodušší progress dialog, nikoli obecnou službu pro předání synchronizačního seznamu do plného přenosového okna a fronty jádra. Panelové příkazy F5/F6 ani SDK `MoveFiles` tento kontrakt nenahrazují; poslední přesouvá obsah celé složky a je v jádře výslovně vyřazené z běžné fronty Copy/Move. Zdrojové opory: `CSalamanderForOperationsAbstract` v [spl_com.h](../src/plugins/shared/spl_com.h), `PostSalamanderCommand` v [spl_gen.h](../src/plugins/shared/spl_gen.h), `CFilesWindow::MoveFiles` v [fileswn6.cpp](../src/fileswn6.cpp), `StartProgressDialog` v [dialogs.h](../src/dialogs.h) a `COperationsQueue` v [worker.h](../src/worker.h).

Budoucí API musí umožnit propojit společný přenos a jeho výsledky s potvrzeným plánem pluginu, uchováním originálů podle zapamatované volby, stagingem, Verify a pravidly D4/D5. Při čekání ve frontě je nutné znovu ověřit platnost plánu před skutečným provedením. Konkrétní rozhraní a rozsah integrace se navrhnou až podle dostupného API. Zmínka o přesouvacím okně zde nezavádí nový synchronizační režim přesunu souborů.

**Není to podmínka první verze ani aktuální implementační úkol.** Do té doby zůstává vlastní přenosový adaptér pluginu a kompatibilita se současným Samandarinem 0.16. Toto rozhodnutí nepovoluje nynější změny SDK/hostitele ani implementaci vlastní kopie fronty, rychlostního limitu či přenosového okna.

## Zdrojová základna a její meze

Primární referencí je oficiální návod referenčního programu a konkrétní odpovědi jeho autora. Komunitní wiki a diskuse dokládají popis funkcí a potřeby jednotlivých uživatelů. FreeFileSync a WinSCP slouží jako zdroje návrhových postupů; jejich chování se nepřipisuje referenčnímu správci souborů. Dokumentace Microsoftu vymezuje vlastnosti Windows API.

Zachované odkazy jsou u příslušných tvrzení; nahrazené zdroje lze dohledat v připnutém archivu původního forku. Data uvedená u historických požadavků nejsou potvrzením, že je dnešní referenční program stále neřeší. Návrh nevychází z převzetí zdrojového kódu, grafiky ani textů nápovědy předlohy. Autoři a projekty jsou uvedeni také v [přehledu poděkování](third_party.md).
