# Skutečný NTFS VHD: nedostatek místa a oprava pro Modify

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


15. října 2026. Dokončen skutečný místní test podle [VHD postupu](directory-sync-plugin-vhd-runbook.md), včetně GUI souhrnů a nového pokusu po uvolnění místa. Nejde o [dřívější vstřikování chyb](directory-sync-plugin-copy-space.md). Bez přepisů či mazání uživatelských položek, SMB testů a změn SDK nebo hostitele.

## Prostředí a ověření identity

Uživatel ve zvýšeném Windows PowerShellu připojil a naformátoval nový vlastní pevný VHD o 256 MiB. Původní zákaz spouštění místních skriptů vyřešil pouze pro svou aktuální relaci pomocí Process/RemoteSigned; nejde o změnu zásad počítače. Testovací Samandarin a souborové sondy následně běžely pod běžným účtem.

Obraz: `C:\Samandarin\builds\dirsync\vhd-space-20261015-real\space-test.vhd`, soubor 268 435 968 B včetně VHD footeru. Nový NTFS svazek `DIRSYNC_SPACE` měl kapacitu **251 588 608 B**, v této relaci E:, disk 1 / oddíl 2. Identita svazku byla `\\?\Volume{5452e253-0cbe-4ec4-ba3a-7ed1297ae789}\`. Písmeno ani číslo disku se nepovažovalo za trvalou identitu.

Před zápisy ověřen Attached, přesná cesta obrazu a DevicePath proti číslu disku oddílu, jeho číslo a UniqueId/NTFS/štítek/kapacita svazku proti `volume-ready.json`. Řetězec Get-DiskImage | Get-Disk v běžném účtu vracel null Number; funkční byly samostatné Get-DiskImage, Get-Partition -DriveLetter a Get-Volume. To je odlišnost dostupných informací v běžném tokenu, nikoli důvod měnit oprávnění nebo zasáhnout jiný disk.

Použit soukromý Release x64 Samandarin 0.16 s vlastní RegFile konfigurací v `C:\Samandarin\builds\dirsync\run`. Po opravě níže byl hostitel znovu spuštěn s produkční SPL/SLG bez fault injectorů a bez testovacích hooků; obě binárky odpovídaly build výstupu:

- SPL SHA-256: `D63F86909406EC81CD8D0266CB7567C65655ACB6BA91162E94D63E4DAC23F224`.
- SLG SHA-256: `B7DFD04FAAF9D86AAE9F3B02CD134ED28AB3208DE4C463D2B96395E24B590D8A`.

## Nalezená a opravená chyba oprávnění

První běh staré soukromé binárky skončil u malého `0-done.txt`: **0 dokončených, 1 chyba, 2 nezahájené, error 5**. Na novém svazku dostal běžný účet na fixture právo Modify. Nativní sonda s tehdejším skutečným adaptérem potvrdila přenos 4 B main + 8 B ADS, ale žádné Verify; otevření stagingu s WRITE_DAC skončilo ACCESS_DENIED. Nešlo o test plného disku.

Adaptér po CopyFileExW vždy znovu zapisoval původní zděděnou DACL, i když už přesně souhlasila. Modify zahrnuje běžné souborové operace, ale nemusí zahrnovat WRITE_DAC; vlastník nových souborů na tomto svazku navíc nezajišťoval takový přístup běžnému tokenu.

Oprava v `copy.cpp`: nejprve porovnat DACL se zachycenou zděděnou hodnotou a **Apply zavolat pouze při odlišnosti**. Identita stagingu, následné kontroly DACL/metadat před i po publikaci a odmítnutí změněného zabezpečení zůstávají zachované. Pokud je obnovení skutečně potřeba a chybí právo, operace nadále selže. Na VHD nebyla rozšířena práva uživatele na Full Control a aplikace nebyla povýšena.

Nový regresní případ používá Modify pro potomky a OWNER RIGHTS ACE, aby se test neopíral o implicitní WRITE_DAC vlastníka. Před opravou selhal na očekávání úspěšné kopie; po opravě explicitně potvrzuje odepřené WRITE_DAC, úspěšnou kopii main/ADS a zachovanou zděděnou DACL cíle.

## Skutečné GUI běhy

Zdroje ležely na C: mimo VHD; každý měl `0-done.txt` (DONE, ADS `:note` = DONE-ADS), velký soubor a `z-next.txt` = NEXT. Pokaždé se začínalo v novém prázdném cíli a potvrdily se jen tři nové kopie. Pro omezení prostoru sloužil vlastní rezervní soubor mimo synchronizované kořeny, s předem omezenou délkou; pracovní disk se nezaplňoval.

| Případ | Velký soubor | Rezerva na VHD | Dostupné místo před dávkou | Výsledek GUI |
| --- | --- | ---: | ---: | --- |
| Hlavní data | 64 MiB main + malý ADS | 202 375 168 B | 34 013 184 B | Error **112**, 1 dokončený, 1 chybný, 1 nezahájený. |
| Velký Unicode ADS | 16 MiB main + 64 MiB `:漢字` | 135 266 304 B | 34 013 184 B | Error **112**, 1 dokončený, 1 chybný, 1 nezahájený. |

V obou souhrnech byla uvedena správná cílová cesta `a-large.bin` a požadavek Compare again. Po zavření souhrnu status vyžadoval nové porovnání. Na disku zůstal pouze správný `0-done.txt` s ADS; velký finální cíl, následující cíl ani `.dirsync-*` staging nebyly nalezeny. Dostupné místo po chybě bylo opět 34 013 184 B.

Po skončení každé dávky byl zkrácen na nulu jen vlastní `fixture-control\space-reserve.bin`. To je řízení syntetické fixture mimo synchronizované složky, nikoli přepis či mazání implementované v pluginu. Nové Compare ukázalo první pár Equal/Skip a dvě zbývající nové kopie. Potvrzený nový pokus v obou případech skončil **2 dokončené, 0 chyb, 0 nezahájených**. Dříve dokončený soubor zachoval čas vytvoření a nekopíroval se znovu.

## Přesné fáze selhání

Samotný GUI souhrn neukazuje poslední datový callback. Proto následovaly dvě další skutečné kopie do samostatných nových `probe-main-right` / `probe-ads-right` při stejném omezení volného místa. Nativní sonda přímo kompiluje aktuální engine a adaptér; její hook pouze zapisuje fázi a počty. Nepoužívá náhrady WinAPI, umělé chyby ani přerušení přenosu. Není součástí distribuovaného pluginu.

- Hlavní data: po dokončených 12 B první položky začala fáze Copying druhé, ale nedostal se další datový callback. CopyFileExW vrátilo 112. **Tento běh dokládá chybu před přenosem hlavních dat velkého souboru**, nikoli chybu po jeho částečném zápisu.
- ADS případ: z 12 B po první položce stouplo copied na **16 777 228 B**, tedy o celých 16 MiB main druhé položky. Pak CopyFileExW vrátilo 112; Verify zůstalo jen na 24 B první položky. To dokládá dokončení přenosu hlavního proudu a následné selhání při zpracování ADS; **nedokládá přenos části samotného ADS před chybou**.

Oba reporty: první Copied/published=true, druhý Failed/error=112/published=false, třetí NotStarted, remainingStage prázdné. Nezávislá enumerace potvrdila jen dokončený malý soubor. Po sondách byla vlastní rezerva opět uvolněna na nulu.

## Integrita, regrese a meze

Po obou GUI opakováních nezávisle ověřeno všech **20 zdrojových/cílových hlavních i pojmenovaných proudů** proti výchozím délkám a SHA-256. Sady názvů/délek ADS souhlasí, žádný proud navíc. UTC mtime a vybrané atributy main souborů odpovídají původnímu zdroji. Výhradní otevření všech těchto proudů prošlo. Časy vytvoření obou dříve dokončených malých cílů zůstaly stejné.

Lokální sestavení a regrese po opravě:

- Copy suite **766 assertions Debug i Release x64**, včetně dřívějších řízených chyb plného disku, dlouhých/Unicode cest a nové regrese Modify.
- Lifecycle **547 kontrol Debug i Release x64**, včetně Stop/Close a force Release při kopírování a Verify.
- Produkční plugin a jazykový modul Debug Win32/x64/ARM64 a Release x64 s warnings as errors.
- git diff --check prošel. Vzdálené CI těchto dosud nepublikovaných změn zatím neběželo. Podpis testovací Release probe DLL byl stávajícím pravidlem přeskočen.

Přípravná ad hoc sonda při prvních sestaveních vypsala upozornění MSBuild na přebrané cesty/intermediate projekt; její projekt a výstup byly následně odděleny od repozitáře. Nejde o compiler warning v produkčních sestaveních ani o výsledek CI. Funkční důkazy vycházejí z úspěšně spuštěné sondy a finálních GUI běhů popsaných výše.

Soukromé hostitele PID 13180 a 14412 jsme normálně ukončili a původní RegFile konfiguraci obnovili. Instalované uživatelské instance zůstaly spuštěné. Uživatel následně ve správcovské relaci provedl Dismount-DiskImage. Nezávislá kontrola Get-DiskImage v 19:38:38 UTC potvrdila **Attached=false a prázdnou DevicePath**; stav je uložen v `detach-status.json`. Obraz zůstal zachovaný v délce 268 435 968 B, včetně testovacích dat uvnitř. Odpojení je tím dokončené; nic se nemaže.

Evidence mimo repozitář: `C:\Samandarin\builds\dirsync\vhd-space-20261015-real\`: `image-manifest.json`, `volume-ready.json`, `source-before.json`, `main-before.json`, `main-failed.json`, `main-retry.json`, `ads-before.json`, `ads-failed.json`, `final-verification.json`, `native-phase-results.json`, `probe-main.log`, `probe-ads.log`, zdroj sondy a záloha konfigurace.

Ověřen je skutečný návrat 112 na omezeném místním NTFS VHD, GUI reakce, úklid stagingu, zachování dokončených dat a nový pokus. Skutečný kód 39, úplné vyčerpání i posledních metadatových bloků, selhání po publikaci z důvodu prostoru, jiné filesystémy/zařízení, SMB a crash recovery tím ověřeny nejsou. Jejich případné řízené testy zůstávají samostatným druhem důkazu. Rozsah kopírování se nerozšiřuje na přepisy či mazání a SDK se nemění.
