# Řazení a volitelné sloupce — 0.1.2

## 2026-10-17 — symetrické sloupce a volitelné označení stran (0.1.3)

Pořadí hlavních sedmi sloupců je Název | Velikost | Změněno | Operace | Změněno | Velikost | Název. Stav, Důvod a Chyba zůstávají za nimi. Jde pouze o vizuální pořadí nativního ListView; logické identity sloupců zůstaly zachované, takže uložené řazení a viditelnost odpovídají původním údajům. Výchozí záhlaví neobsahuje označení strany. Checkbox „Uvádět stranu v záhlaví“ / „Show side names in headers“ je v dosud volné buňce dvousloupcového nastavení. Dialog se nezvětšil. Volba je default OFF, uložená jako validovaný DWORD `SideNamesV1`; chybějící nebo neplatná hodnota znamená OFF. Explicitní označení stran zůstává v položkách nastavení viditelnosti a v nabídce sloupců, aby tyto položky byly rozlišitelné.

Ověření: Release x64 s warnings-as-errors; 2 357 lifecycle kontrol včetně nových výchozích/uložených/neplatných hodnot, mapování všech krátkých a úplných záhlaví, skutečného nativního pořadí a sousedství pravého data, velikosti a názvu. Řazení a filtrování nadále pracuje s původními modelovými identitami. Anglické/české resource a layout testy: 2 357 / 2 389 kontrol při 96–240 DPI metrikách; nový checkbox se vejde do původního dialogu.

Computer Use na soukromém Samandarinu 0.16 x64 a místní NTFS sadě `dirsync-relative-20261017`: anglický světlý náhled ukazuje výchozí krátká symetrická záhlaví, metadata a relativní časy na správných stranách. Zapnutí označení stran změní jen záhlaví a zachová všech 5 výsledků. Kliknutí na nově umístěné pravé datum seřadí podle pravého FILETIME; chybějící protějšek je první, nejnovější pravý soubor poslední. Skutečný restart do českého tmavého režimu zachoval zapnuté označení stran i řazení pravého data. Český checkbox byl vyzkoušen myší i mezerníkem a Enterem; návrat ke krátkým záhlavím zachoval všech 5 výsledků a aktivní řazení. Žádná souborová operace v GUI nebyla spuštěna; bez SMB testů a změn SDK.

Následující oddíly jsou historická evidence předchozího uspořádání.


## Doplnění 17. října — oboustranné názvy, Settings a posouvání

Aktuální náhled má deset sloupců. **Název vlevo** a **Název vpravo** opakují stejný koncový název na obou stranách operace. Pravá část začíná názvem před pravou velikostí a časem. Oba názvy i Operace jsou povinné; relativní cesta zůstává v záhlaví skupiny. Zobrazení názvu na obou stranách se používá také u jednostranného výsledku a nepředstírá fyzickou existenci protějšku: jeho velikost a čas zůstávají prázdné, stav a návrh se nemění. Oba názvy mají stejné formátování hostitele a lze podle obou řadit.

**Columns… bylo odstraněno z kontextového menu souborových řádků.** Sedm volitelných sloupců se nastavuje v kompaktní samostatné sekci Settings / Nastavení, mezi vzhledem a obnovou. OK použije a uloží změny, Cancel je zahodí. Přímé standardní zaškrtávací menu na pravý klik záhlaví sloupců zůstává. Dosavadní maska konfigurace je zachována a nový pravý název je při načtení vždy viditelný. Nastavení nemění řádky, návrhy ani rozsah operací. Bez změny řazení není potřeba obnovovat obsah seznamu, takže nastavení nemá posouvat dosavadní pohled k začátku.

Mizení záhlaví způsobovalo vlastní kreslení adresářových předělů v CDDS_POSTPAINT: obdélník částečně odsunuté skupiny zasahuje do oblasti záhlaví sloupců. Kreslení se nyní ořezává výhradně na klientskou část pod spodní hranou nativního záhlaví; SaveDC/RestoreDC zachová původní kreslicí stav. Nativní regresní test vytvoří skutečný ListView s 6 000 řádky, posune jej do prokazatelného překryvu, vykreslí post-paint do bitmapy s kontrolní barvou a ověří, že všechny pixely kontrolního pásu záhlaví zůstaly beze změny.

Lifecycle sada: **2 357 kontrol**, včetně obou názvů, řazení deseti sloupců, migrace masky a ochrany pravého názvu. Poslední naplnění 6 000 položek trvalo 2 250 ms. Resource/layout: **2 208 anglických / 2 232 českých kontrol**, včetně skutečných popisků nové sekce při 96–240 DPI. Release x64 a Debug Win32/x64/ARM64 s warnings-as-errors; ARM64 pouze sestavení.

GUI používá izolovanou sadu `C:\Samandarin\builds\dirsync-scroll-20261017`: 240 dvojic souborů v 20 Unicode adresářích a vlastní hostitel Samandarin 0.16 x64 z předchozí etapy. V českém tmavém náhledu oba názvy odpovídají, po menším i velkém posunu dolů a návratu nahoru zůstávají všechna záhlaví viditelná bez přejíždění myší. V Settings bylo ověřeno zahození rozepsané změny i potvrzené skrytí levé velikosti s uchováním 240 výsledků a zvýraznění. Menu řádků neobsahuje Columns. Po skutečném restartu s finálními moduly v anglickém světlém hostiteli zůstala levá velikost skrytá; po novém Compare a velkém posunu jsou hlavičky čitelné a oba názvy zachované. Otevření Settings a Cancel zachovaly posunutý pohled v oblasti adresářů 09/10. Lokální textová evidence je `gui-evidence.json` pod kořenem sady. Žádná souborová operace nebyla spuštěna, bez SMB testů a změn SDK. Následující části zachycují původní první průchod 0.1.2 před tímto doplněním.

17. října 2026. Navazuje na [seskupení náhledu](directory-sync-plugin-grouping.md) a [aktuální návod](directory-sync-plugin.md). Bez SMB testů a změn SDK nebo zápisových adaptérů.

## Průzkum a rozhodnutí

V referenční synchronizaci 11.58 bylo přes Computer Use ověřeno klikání do názvu, levé velikosti a levého data změny v obou směrech. Použita izolovaná sada z předchozí etapy rozšířená o `a-sort.bin` (30 B, 4. října), `m-sort.bin` (10 B, 2. října) a `z-sort.bin` (20 B, 3. října) v adresářích alpha a beta-漢字. Celkem 11 souborových výsledků a samostatný pravostranný prázdný adresář. Nebyla spuštěna synchronizace.

Pořadí adresářových předělů zůstává kořen, alpha, alpha/nested, beta-漢字, delete-dir. Řazení mění pořadí souborů uvnitř těchto předělů: velikosti 4/10/20/30 a opačně, data 2./3./4. října/původní novější soubor a opačně. Chybějící protějšek má prázdnou hodnotu. Toto oddělení adresářů a souborů zachovává i plugin; záhlaví nejsou samostatným řaditelným datovým řádkem.

## Chování pluginu

- Kliknutí na jiný sloupec začíná vzestupně; opakovaný klik přepíná směr. Všech devět sloupců lze řadit. Skupiny jsou vždy seřazené podle relativní cesty vzestupně, kořen první.
- Velikosti používají uint64, časy původní FILETIME a Error číselný kód. Chybějící strana a velikost adresáře jsou prázdné, odlišné od skutečně nulového souboru. Vzestupně jsou prázdné hodnoty první, sestupně poslední. Texty se porovnávají Windows ordinal bez rozlišení velikosti písmen; nejde o jazykové ani přirozené řazení čísel v názvech.
- Při shodné hodnotě se použije název vzestupně a následně stabilní index modelu. Změna návrhu při řazení podle operace může položku přesunout; její zvýraznění se zachová podle identity modelu.
- Název a Operace nelze schovat ani stáhnout na nulovou šířku; cestu představují povinná adresářová záhlaví. Ostatních sedm sloupců je volitelných. Skrytý sloupec nelze omylem rozšířit tažením dělicí čáry; obnovuje se přes nabídku.
- Skrývání sloupců nemění souborové filtry, výběr, počet návrhů ani rozsah operací. Skrytí řazeného sloupce vrací Název vzestupně. Nabídka používá běžné menu hostitele, existující přeložené názvy sloupců a zaškrtnutí; povinné položky jsou zakázané.
- Nastavení VisibleColumnsV1, SortColumnV1 a SortDescendingV1 používají stávající SDK konfiguraci. Staré konfigurace zachovají všech devět sloupců a Název vzestupně. Neplatné hodnoty se omezí na platný rozsah a povinné sloupce se obnoví. Šířky se neukládají přes restart.
- Během workeru nelze měnit řazení ani viditelnost sloupců. Řazení mění jen mapování viditelných indexů, žádný modelový řádek se nepřepisuje.

## Ověření

Nativní lifecycle sada prošla **2 357 kontrolami**; rozšířený probe testuje vzestupné/sestupné hodnoty, pevné pořadí skupin, výběr stejného modelového řádku po řazení, prázdnou hodnotu oproti nule, číselné velikosti 2/10/1000 i 10^12, UINT64_MAX časy, číselné chyby 2/10, obě strany, všech devět indikátorů, skrytí/obnovení sedmi sloupců, ochranu povinných šířek a zákaz roztažení skrytého sloupce. Nastavení pokrývá 18 dílčích kontrol včetně nových voleb, migrace a neplatných hodnot; jde o rozšíření existujícího probe, nikoli o navýšení počtu vnějších kontrol. Sada nadále zahrnuje dlouhé Unicode rodiče a 6 000 položek / 60 skupin (poslední sestavení seznamu 2 359 ms).

Anglické resource/layout testy: **1 974**, české **1 990**, metriky 96–240 DPI. Release x64 a Debug Win32/x64/ARM64 s warnings-as-errors; ARM64 pouze sestavení.

GUI na vlastním hostiteli Samandarin 0.16 x64: anglický světlý náhled, 11 souborů ve čtyřech skupinách, levá velikost 4/10/20/30 i 30/20/10/4 a neměnné adresářové předěly. Nabídka záhlaví ukázala devět zaškrtávacích položek, Název a Operace zakázané. Skrytí State zachovalo 11 výsledků. Po řádném ukončení a restartu do českého tmavého rozhraní zůstal State skrytý a levá velikost sestupně; kontrola uloženého souboru potvrdila masku 0x1bf, sloupec 1 a směr 1. Skrytí Důvodu v tmavém režimu také zachovalo počty a cesty.

Při prvním GUI průchodu se po nabídce sloupců otevíralo navíc menu řádků; opraveno zachycením pravého tlačítka přímo v záhlaví a spotřebováním celé události. Nový průchod potvrdil, že po skrytí Důvodu již druhé menu nevzniká. Tmavé téma neukazovalo standardní sort šipku spolehlivě, proto aktivní záhlaví navíc obsahuje kompaktní ↑/↓; nativní HDF příznaky zůstávají pro sémantiku a světlé téma.

Finální tmavý průchod potvrdil viditelný ↓ u velikosti po dalším restartu, skryté State i Reason, následné skrytí právě řazené velikosti s přechodem na ↑ Název a zachovaných 11 výsledků. Sloupec byl obnoven klávesnicí přes Shift+F10 → End → Enter → Home → Down → Enter. Zvýraznění `alpha/m-sort.bin`, návrhy i cesty zůstaly zachované; druhé menu nevzniklo. Lokální textová evidence: `sorting-012-evidence.json` pod kořenem izolované sady.

Nebyly provedeny žádné souborové operace v GUI. Testovací hostitel používá vlastní RegFile konfiguraci; nejde o opakování čisté instalace, registry backendu, změn systémového locale nebo živého přesunu mezi DPI monitorů. Uživatelské instance hostitele nebyly zavírány ani aktualizovány.
