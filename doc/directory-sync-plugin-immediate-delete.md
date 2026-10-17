# Okamžité odstranění z kontextového menu — 0.1.3

17. října 2026. Bez SMB testů a bez změn SDK.

## Rozhraní a rozsah

Návrhové příkazy jsou přejmenované na označení ke kopírování doprava/doleva, k přeskočení a ke smazání při synchronizaci. Klávesové zkratky zůstaly. Nová oddělená nabídka Odstranit vybrané obsahuje vlevo/vpravo/obě strany. Její rozsah je pouze explicitní výběr modelových řádků. Příkaz ignoruje návrh v řádku; platí i pro Skip nebo Shoda, nikoli automaticky pro jiné viditelné řádky. Chybějící protějšek, neidentifikovaná/nepodporovaná/nejednoznačná či read-only položka se nezařadí. Žádný fallback na celý plán.

Shift se čte při aktivaci konkrétního příkazu strany, ještě před asynchronním ověřením a potvrzením. Potvrzení OK/Cancel má bezpečný výchozí Cancel a obsahuje způsob, stranu, skutečný počet a všechny cesty. Je nezávislé na potvrzení viditelného synchronizačního plánu a nepřebírá jeho zavádějící poznámky. Uchování originálů ani journal synchronizace se nepoužijí.

## Provedení

Stávající SDK neposkytuje přenosovou frontu jádra; plugin používá Windows IFileOperation ve vlastním STA workeru. Recyklace nastavuje FOFX_RECYCLEONDELETE/FOFX_ADDUNDORECORD, vlastní callback odmítne PreDeleteItem bez TSF_DELETE_RECYCLE_IF_POSSIBLE. Nepoužívá se alternativní DeleteFile/RemoveDirectory fallback. FOF_NOERRORUI/FOFX_EARLYFAILURE zastaví zbytek při chybě; připojené soubory Shell nesmí přidat. Trvalá varianta nemá recyklační ani undo příznaky.

Před potvrzením i po něm je read-only revalidace. Výběr používá kořeny snapshotu; relativní cesty nesmějí uniknout, duplicity se odstraní podle ordinal case-insensitive cest v O(n log n). Před každou položkou se ověří identita/typ a souborová velikost/čas, reparse/read-only stav. Adresář s vlastními ADS se odmítne. Soubory první, adresáře od nejhlubších a pouze prázdné; žádné rekurzivní odstranění skrytých nebo nevybraných potomků. Nejde o rozšíření dříve odloženého plně transakčního NTFS modelu.

Stop/Close používají stávající cancel/join a Stop vedle Synchronizovat; callback a hranice položek kontrolují cancel. Jednotlivý právě probíhající Shell krok nemusí reagovat okamžitě. První chyba/Stop zastaví zbytek, hotové položky se nevracejí. Výsledek uvádí počty, HRESULT a provedené položky; náhled se vždy zneplatní po pokusu o provedení. Ostatní souborové příkazy vyžadují nové Compare. Cancel v potvrzení žádný worker nespustí a výsledek neinvaliduje.

API podklady: [IFileOperation flags](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileoperation-setoperationflags), [PreDeleteItem](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileoperationprogresssink-predeleteitem), [TRANSFER_SOURCE_FLAGS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-_transfer_source_flags). Neprovedený případ plného či vypnutého Koše není vydáván za fyzický test; guard nerecyklačního callbacku je pokryt přímo testem.

## Ověření

Release x64 a Debug Win32/x64/ARM64 sestavení s warnings-as-errors prošla. ARM64 pouze sestaveno, runtime zde x64.

- Skutečné izolované NTFS Shell testy v lifecycle probe: **57 kontrol**, včetně levé/pravé/obou stran, dvojího výběru, prázdného výběru, Skip, zachování nevybraných souborů, Hidden/System, dat + ADS v Koši, trvalého odstranění bez recyklační cesty, prázdného adresáře do Koše a soubor+rodič od nejhlubšího. Nevybraný potomek blokuje rodiče. Read-only položka je nezpůsobilá, adresářové ADS chráněné. Zamčený soubor přežije chybu, změna po skenu nic nesmaže, Stop před začátkem nic nesmaže a Stop po první položce zachová dokončenou změnu a vynechá druhou. Recyklace Unicode souboru na cestě 307–308 UTF-16 znaků prošla včetně ADS. Test bez recyklačního callback příznaku odmítnut ještě před operací.
- Celkem **2 366 lifecycle kontrol**, včetně stávajících unload/Stop/Close regresí a rozšířeného mapování Stop pro tento nový worker/ověření. To není nový fyzický GUI test nuceného uvolnění právě uprostřed Shell operace.
- **2 389 anglických / 2 421 českých** resource/layout kontrol při 96/120/144/192/240 DPI metrikách; potvrzení je stávající kompaktní tabulkový dialog, bez nového stylu.
- Computer Use, české tmavé GUI Samandarinu 0.16 x64: izolovaná sada `C:\Samandarin\builds\dirsync-immediate-20261017`, tři dvojice 10 B + ADS 3 B. Ověřené přejmenování návrhů, oddělovače, podmenu stran, potvrzení se skutečnou jednou pravou položkou, bez Shiftu přesun `right\recycle.txt` do Koše; přečten skutečný odpovídající recyklační záznam, obsah i ADS. Shift+Enter na pravé volbě ukázal „Trvale odstranit bez Koše“ a po samostatném schválení byl odstraněn jen `right\permanent.txt`. Pro něj nevznikl recyklační záznam. Oba levé protějšky i oba keep.txt včetně ADS zůstaly. Po obou operacích náhled vyžadoval nové porovnání. Koš nebyl vyprazdňován a uživatelské instance nebyly změněny.

GUI ověření nebylo rozšířeno na velké rekurzivní mazání, jiné souborové systémy ani SMB. Ruční obnova přes GUI Koše v této etapě nebyla nově provedena; ověřená data/ADS zůstávají v Koši. Historické záznamy o tom, že menu nic neprovádí, se týkají pouze návrhové sekce.
