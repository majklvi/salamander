# Directory Synchronization: provedení a obnova na úrovni jádra


**Navazující stav (15. října 2026):** na samostatný pokyn uživatele implementována [kopie pouze do nových cílů](directory-sync-plugin-copy-new.md), včetně stagingu, Verify, metadat, průběhu a Stop. Původní návrhové pasáže níže nejsou tvrzením o přepisech či mazání; ty zůstávají neimplementované. SDK/hostitel beze změn, bez SMB testů. Přesné runtime výsledky a odchylku vlastní CREATE_NEW rezervace stagingu uvádí nový protokol.

Aktualizováno 15. října 2026 podle výslovného rozhodnutí uživatele: **cílem první verze je úroveň spolehlivosti současného jádra Samandarin-0.16**, nikoli nový transakční filesystem backend. Tento dokument nahrazuje dřívější přísnější návrh. D1–D5 z [hlavního návrhu](directory-sync-plugin-design.md) platí dál, zejména nezávislá obnova před přepisem i mazáním a zachování podporovaných vlastností. Plugin zatím pouze skenuje, porovnává a zobrazuje náhled. Zápisový executor, SMB testy ani změny SDK/hostitele nejsou tímto pokynem povoleny.

## Referenční úroveň a omezení

Referencí jsou existující souborové operace v [src/worker.cpp](../src/worker.cpp), zejména otevření zdroje pro kopii se share READ | WRITE a vytvoření nového cíle přes CREATE_NEW (oblast řádků 5745–5778 v této verzi), zpracování chyb, přerušení a metadat. To dokládá konkrétní běžný postup; není to tvrzení, že celé jádro má jediný režim sdílení nebo všechny níže uvedené záruky. Přímé znovupoužití interního workeru z pluginu není prokázané.

Podpora SDK 0.16 se musí ověřit podle konkrétního rozhraní, jeho Unicode/cestových omezení, threadingu a UI. Například [CSalamanderSafeFileAbstract](../src/plugins/shared/spl_file.h) poskytuje ošetřené file I/O, ale samo o sobě není veřejnou službou pro spuštění libovolného kopírovacího plánu jádra. Dosavadní [SDK audit](directory-sync-plugin-sdk.md#proč-nepřevzít-existující-kopírování-bez-auditu) již doložil jeho limity MAX_PATH a bajtových cest; návrh je nesmí ignorovat. Nepředpokládat nové SDK metody ani přímé volání interních tříd hostitele. Vhodné stávající rozhraní použít tam, kde splňuje požadavky; jinak zvolit malý pluginový adaptér nad běžnými wide Win32 API podle postupů jádra. Neportovat celý worker ani experimentální native API backend.

Předpokladem je běžná práce se složkami. Pokud se obsah mění během operace, nelze slíbit neměnný snapshot celého stromu ani všech ADS. Pozorované změny se ohlásí a konfliktní operace se zastaví či přeskočí; neprokázaná úplná ochrana proti úmyslným reparse/ACL závodům není překážkou běžného vývoje. Základní kontrola cest, nepodporovaných položek a chyb tím nezaniká.

## Povinné chování první verze

| Oblast | Požadavek |
| --- | --- |
| Potvrzený rozsah | Pouze viditelné zařazené akce ze schváleného náhledu; blokované, skryté a nerozhodnuté položky nevykonávat. Změna kořenů/voleb a nové Compare vyžadují nový plán. |
| Cesty a konflikty | Zachovat stávající kontroly kořenů, zjistitelného překryvu/aliasů, relativních cest a nepodporovaných reparse/EFS/cloud položek. Obnova mimo oba stromy. Konflikt soubor/složka není pokyn k rekurzivnímu odstranění. |
| Změny po skenu | Zachovat současný read-only Revalidate a před jednotlivou budoucí akcí ověřit dostupnou identitu/metadata a očekávanou přítomnost cíle. Zjištěný rozpor znamená zastavení/přeskočení a nový náhled podle rozsahu změny. Checkpoint není atomický zámek. |
| Obnova D4 | Před přepisem i mazáním uložit nezávislou kompletní kopii původních podporovaných dat/ADS do kolizně bezpečného jména. Zaznamenat původní cestu. Chyba tvorby či ověření obnovy nesmí pokračovat destruktivním krokem. Žádné hardlinky jako záloha. |
| Kopie a metadata D5 | Běžné kopírování s průběhem, zastavením a ošetřením chyb; vybraná metadata a cílová pravidla oprávnění podle D5. Původní cílové ADS zůstávají v obnově a nesmějí se náhodně sloučit do nového obsahu. Nepodporované zachování se nesmí vykázat jako úspěch. |
| Nový cíl | Nepřepsat nově vzniklou kolizi jen proto, že při skenu cíl chyběl; vytvoření vlastních souborů přes create-new. |
| Chyba a Verify | Nehlásit úspěch po neúplném přenosu nebo chybě ověření. Zachovat obnovu; žádný slepý destruktivní retry nebo automatický rollback přes mezitím změněný cíl. |
| Stop a dokončení | Rozlišit dokončené, přeskočené, zastavené a chybové položky. Běh může být částečně hotový; neslibuje se rollback celého stromu. Zavřít handly a bezpečně řešit vlastní rozpracované soubory. |
| Mazání | Až po ověřené obnově a kontrole aktuálního objektu. Adresáře jen skutečně prázdné; žádné rekurzivní mazání odvozené z filtrovaného náhledu. |
| Evidence a ruční obnova | Jednoduchý verzovaný protokol běhu: operace, původní/obnovovací cesta, výsledek, chyba a nedokončené položky. Obnova a úklid ruční podle D4; log není historie synchronizace podle D2 ani oprávnění k automatickému přehrávání zápisů. |

D4 je vědomý doplněk synchronizačního workflow. Srovnání s jádrem neznamená zrušení této ochrany ani přijatých pravidel D5. Současné read-only kontroly se neodstraňují; současně se z nich nestává požadavek na stále přísnější infrastrukturu pro zápisy.

## Přiměřená posloupnost budoucí operace

1. Zkontrolovat položku potvrzeného plánu, aktuální objekt a běžné chyby přístupu/cesty.
2. Při přepisu nebo mazání připravit a ověřit nezávislou obnovu; bez ní nepokračovat.
3. Kopii připravit do vlastního nového dočasného souboru, pokud to zvolený běžný postup vyžaduje pro ochranu původního cíle a přesné ADS. Ošetřit chyby, Stop a metadata podle D5.
4. Před dokončením znovu zkontrolovat předpoklady a použít konkrétní běžný mechanismus vybraný podle jádra a SDK. Přesné API pro přepis zatím není vybráno. Dvojí native rename ani úplné držení řetězce rodičů nejsou závaznou architekturou. ReplaceFileW také není automaticky vhodné kvůli již zjištěnému slučování některých ADS.
5. Zkontrolovat požadovaný výsledek, zaznamenat stav a ponechat obnovu. Při selhání nebo nejasném výsledku uvést chybu, nikoli slib transakčního návratu.

Vlastní staging či kontrola výsledku může být účelná pro D4/D5; není důvod zavádět vlastní transakční systém jen kvůli silnějším zárukám, než má referenční jádro. Konkrétní postup přepisu se vybere až po úzkém technickém mapování níže, bez tichého oslabení D4/D5.

## Odložené záruky a ověřování

Pro první verzi nejsou podmínkou: atomické zmrazení úplné sady ADS a stromu, ochrana před všemi úmyslnými reparse/ACL závody, kontinuální uzamčení všech předků, oplock/USN infrastruktura, nativní relativní create/rename backend, celá matice file-ID reuse a privilegovaných zásahů ani nový transakční journal s kryptografickými kontrolními součty a automatickým crash replay. Odkládají se samostatné kampaně s fyzickými I/O poruchami, výpadky napájení a dokazováním durability filesystemu.

Běžné scénáře chyby přístupu, obsazeného souboru, nedostatku místa, Stop, neúspěšného přenosu/Verify a změny souboru po skenu zůstávají v integračních testech relevantní implementace. Není nutné předem vybudovat zvláštní diskovou laboratoř jako podmínku zahájení práce. SMB zůstává v produktovém D1, ale jeho testy nyní nejsou povoleny a místní výsledky se za ně nevydávají.

## Dosavadní evidence a další krok

[Publikovaná NTFS sonda](directory-sync-plugin-ntfs-protocol-probe.md) a její výsledky 347 assertions / 53 procesů v každém Debug/Release běhu zůstávají zachovány jako technická evidence. Její native rename/journal nejsou doporučenou implementací pluginu. Navazující rozpracované concurrency testy nebyly sestaveny ani spuštěny; jejich necommitnuté zapojení a zdroj byly odstraněny při tomto zúžení rozsahu. Žádný nový runtime výsledek se netvrdí.

**Mapování jádra/SDK je dokončeno:** [konkrétní tabulka, minimální adaptér a běžné testy](directory-sync-plugin-core-adapter.md). SDK není obecná služba copy/delete checklistu; SafeFile má známé limity cest. Doporučený privátní adaptér používá běžná wide Win32 API, nikoli interní worker či native transakční infrastrukturu. CopyFileExW / MoveFileExW jsou kandidáti k integračnímu ověření D5; dosavadní native sonda je netestovala jako tento adaptér.

Po samostatném pokynu doporučeno začít úzkou implementací kopie pouze do nového cíle, se stagingem, kontrolou výsledku, Stop a metadaty. Přepis a mazání přijdou později s ověřenou společnou obnovou D4. Nyní jde pouze o dokumentaci, bez zápisového executoru, SMB testů a změn SDK.
