# Kopie do nových cílů: nedostatek místa

15. října 2026. Pouze syntetická místní data. Bez přepisů existujících cílů, mazání uživatelských položek, SMB testů a změn SDK nebo hostitele. Navazuje na [první kopírovací etapu](directory-sync-plugin-copy-new.md) a [ověření uvolnění pluginu](directory-sync-plugin-copy-unload.md).

## Rozsah a omezení skutečného svazku

Připraven vlastní dynamický VHD s kapacitou 128 MiB mimo repozitář. `CreateVirtualDisk` uspělo, ale `AttachVirtualDisk` v aktuálním tokenu skončilo **ERROR_PRIVILEGE_NOT_HELD (1314)**. Následné `Get-DiskImage` potvrdilo Attached=false a prázdnou DevicePath. Nebyl formátován žádný disk, měněny kvóty ani zaplňován pracovní svazek. Malý nepřipojený obraz byl po kontrole uklizen.

**Test na skutečně zaplněném NTFS svazku ani GUI běh nad takovým svazkem neproběhl.** Níže uvedené výsledky jsou řízené regresní testy reakcí adaptéru na chyby WinAPI. Neprokazují konkrétní chování NTFS alokátoru, ovladače nebo CopyFileExW na fyzicky plném disku.

Evidence pokusu: `C:\Samandarin\builds\dirsync\copy-space-20261015\vhd-probe.cs` a `vhd-probe.json` (createError=0, attachError=1314, kapacita 134 217 728 B, bez permanentního připojení).

## Řízené regresní ověření

Stávající `dirsync_copy_tests` rozšířen na **743 assertions Debug i Release x64**, sestavení s warnings as errors. Počet zahrnuje přípravu a kontrolní čtení; nejde o 743 samostatné scénáře. Všechny dosavadní případy včetně dlouhých/Unicode cest, ADS, kolizí, oprávnění a Stop zůstávají součástí běhu. Vzdálené CI těchto nepublikovaných změn zatím neběželo.

Testovací projekt nyní kompiluje skutečný `copy.cpp` přes `copy_space_faults.cpp`. Pouze v tomto testovacím překladu jsou obaleny CreateFileW, CopyFileExW a MoveFileExW. Nezasažené operace volají původní WinAPI; produkční projekt i lifecycle probe nadále kompilují původní adaptér přímo. Žádná obálka ani nový přepínač se nepřidává do distribuovaného SPL nebo SDK.

Každý z pěti případů se provedl pro **ERROR_DISK_FULL (112)** i **ERROR_HANDLE_DISK_FULL (39)**:

| Místo chyby | Příprava / zásah | Ověřený výsledek |
| --- | --- | --- |
| Rezervace stagingu | Obalené CreateFileW vrátí chybu před vznikem rezervace. | První položka Failed, následující NotStarted; nevznikl cíl ani staging. |
| Hlavní data | Obálka pomocí ReadFile/WriteFile skutečně zapíše 1 MiB z 8 MiB do existující vlastní rezervace a vrátí chybu. | Částečná kopie se neověřuje/nepublikuje; staging uklizen. |
| Unicode ADS | Obálka přenese 4 B hlavních dat a 1 MiB z 8 MiB ADS `:漢字`, potom vrátí chybu. | Neúplná sada proudů se nepublikuje; hlavní proud i ADS stagingu uklizeny společně. |
| Přenos a blokovaný úklid | Stejný částečný zápis; skutečný otevřený handle bez sdílení pro smazání blokuje úklid stagingu. | Zachován původní kód plného disku a Failed; remainingStage obsahuje přesnou cestu vlastní částečné kopie. |
| Publikace po Verify | Skutečná úplná CopyFileExW a Verify, poté obalené MoveFileExW vrátí chybu bez přejmenování. | Failed, published=false; žádný finální cíl, staging uklizen. |

U přenosových variant obálka před vrácením chyby zavolá produkční progress callback s částečnými počty. **Ve zvoleném chybovém přenosu nenutí skutečnou CopyFileExW selhat za běhu:** modeluje její návrat a zanechané částečné soubory explicitními WinAPI zápisy. Zbytek adaptéru, kontrola identity stagingu, reakce na chybu a úklid jsou skutečné. Tato hranice je důležitá při interpretaci výsledků.

Ve všech variantách kromě prvotní rezervace již před chybou existuje jedna dokončená nová kopie (DONE a ADS `:note` = DONE-ADS). Její hlavní data i ADS zůstaly beze změny, completed zůstal 1. Chybující položka je Failed, následující NotStarted; report není Cancelled ani Stale a zachovává přesný vložený kód 112/39. Finální názvy chybující a následující položky neexistují. U částečných přenosů průběh obsahuje přenesené bajty, ale Verify jen 24 bajtů z již dokončené malé položky.

Zdrojová hlavní data i Unicode ADS byla po chybě nezávisle přečtena a porovnána s původními daty. Výhradní otevření zdroje prošlo. U blokovaného úklidu byl zámek uvolněn až po návratu adaptéru; report stále uváděl zbylý staging. Test ověřil jeho nenulovou neúplnou délku a uklidil pouze tento vlastní soubor v kontrolované fixture.

Po vypnutí chyby a novém Compare se obě zbývající kopie podařily, včetně přesných hlavních dat a ADS. Dříve dokončený pár byl při novém skenu shodný a nekopíroval se znovu. Nejde o automatický Retry nebo obnovu starého plánu.

## Produkční závěr a další skutečný test

Chyba vyžadující změnu produkčního adaptéru se v uvedeném rozsahu neprokázala. Stávající politika odpovídá schválené úrovni jádra: první chyba zastaví dávku, dokončené nové soubory zůstanou a vlastní nepublikovaný staging se uklidí, pokud to systém dovolí. Audit `CopySummary` v `dirsync.cpp` potvrzuje, že čte statusy, původní číselný kód chyby a případný remainingStage; po skončení UI zneplatní plán a vyžaduje Compare. **GUI zobrazení chyby 112/39 se tímto auditem samostatně netestovalo.**

Pro skutečný lokální test zbývá připojit malý vlastní VHD v prostředí s potřebným oprávněním, ověřit identitu nového svazku před formátováním a teprve na něm omezit dostupný prostor. Ověřit skutečnou chybu během hlavních dat a ADS, zachování předchozí kopie, hlášený/uklizený staging a GUI souhrn; po uvolnění místa provést nové Compare a nový pokus. Pracovní ani systémový disk se pro tento test nezaplňuje. Připojení, formátování ani zvýšení oprávnění nebylo v této etapě provedeno.

Vzácné alokační chyby při konkrétním zápisu atributů/bezpečnosti a po úspěšné publikaci tím nejsou pokryté; dřívější [testy konečných chyb po publikaci](directory-sync-plugin-copy-cancellation.md) nadále dokládají zachování publikovaného souboru při chybě kontroly, nikoli fyzické zaplnění disku. SMB, přepisy, mazání, trvalý journal a crash recovery zůstávají mimo tuto etapu.
