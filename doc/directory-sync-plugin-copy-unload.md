# Kopírování: nucené uvolnění a ukončení hostitele

15. října 2026. Navazuje na [Stop/Close a chyby po publikaci](directory-sync-plugin-copy-cancellation.md) a [dokončené GUI ověření](directory-sync-plugin-ui-automation.md). Pouze vlastní syntetická místní NTFS data, kopie do nových cílů. Bez přepisů, mazání uživatelských položek, SMB testů a změn SDK nebo hostitele.

## Nucené Release během přenosu a Verify

Rozšířen stávající `dirsync_lifecycle_probe.dll`, který zahrnuje skutečný `dirsync.cpp` a kopírovací adaptér. Testovací exporty a hooky se nedistribuují v produkčním SPL. Případy používají skutečný `Plugin::Release`, nikoli jeho napodobeninu.

| Přerušená fáze | Předchozí dokončené položky | Výsledek |
| --- | --- | --- |
| CopyFileEx callback po nenulovém přenosu | 0 | Force Release zastavil a joinoval worker; aktuální Cancelled, další NotStarted. |
| Verify po nenulovém čtení | 0 | Stejný výsledek; žádný publikovaný cíl. |
| CopyFileEx callback druhého souboru | 1, včetně ADS | Dokončený soubor zachován; aktuální Cancelled, poslední NotStarted. |
| Verify druhého souboru | 1, včetně ADS | Stejný výsledek; dokončená hlavní data i ADS zachovány. |

V každém případě nejprve `Release(FALSE)` odmítl uvolnění a worker zůstal aktivní. `Release(TRUE)` doběhl pod místním testovacím limitem 3 s. Následoval úspěšný opakovaný force Release a **okamžité FreeLibrary**, po němž DLL nebyla v mapě modulů. Nenalezen nepublikovaný cíl ani staging, všechny zdrojové soubory šly výhradně otevřít. U dokončené položky byla nezávisle zkontrolována délka a každý bajt hlavního proudu i ADS `:note` (v obou případech 1 MiB testovacích dat).

Probe zadrží dosažený callback nejvýše 5 s, aby testovací vlákno spolehlivě zasáhlo zvolenou fázi u malých souborů; cancel jej ihned uvolní. Má malé pomocné modeless okno, takže Release prochází také větví s neprázdným `Preview::window`. Následné zrušení tohoto pomocného okna je úklid testovacího harnessu. **Nejde o test reentrantního odmapování DLL uvnitř živého modalního callbacku hostitele.**

`dirsync_lifecycle_tests`: **547 kontrol Debug i Release x64** s warnings as errors, včetně původních 429 kontrol skenu, porovnání a Stop/Close. Počet zahrnuje přípravu a opakované kontroly, nikoli 547 nezávislých scénářů. Nová varianta nevyžadovala změnu produkčního kódu; opravovaná chyba se neprokázala. Vzdálené CI těchto dosud nepublikovaných změn neběželo.

## Běžné ukončení v GUI

Computer Use ověřilo vlastní Release x64 Samandarin 0.16 v `C:\Samandarin\builds\dirsync\run`, s vlastní RegFile konfigurací. Produkční SPL bez hooků měl SHA-256 `F27B18C7D89820EDB38655939D654B4B799A9DEE3DC84499D997E5789E3411AE`, shodný s build výstupem; SLG `701CD3E2BC87FEFD64C6A4E62CA7264BFBDCD5B3B14BFFD16F8100CFD3EE1EC4` také odpovídal build výstupu. Testovací DLL nebyla v hostiteli načtena.

Čerstvá sada měla tři řádky: `0-done.txt` (DONE, ADS `:note` = DONE-ADS), `a-large.bin` (4 GiB skutečných dat) a `z-next.txt`. Cílová složka byla prázdná. Po potvrzení kopírování průběh ukázal **položku 2 ze 3, 591 396 876 přenesených bajtů, Verify 24 bajtů a 1 dokončení**. Velký soubor byl tedy ještě v přenosu.

První Alt+F4 během této fáze zrušilo práci a zavřelo náhled do panelů hostitele. Odeslání klávesy a návrat následného snímku trvaly **555 ms**; nejde o samostatné měření interního Join ani garantovanou mez. Druhé Alt+F4 běžně ukončilo hostitele. Nezůstal proces testovací instance PID 10484; nebyl použit hard kill. Návratový kód procesu nebyl měřen.

Po ukončení obsahoval cíl pouze správný `0-done.txt` a jeho ADS. Velký cíl, následující cíl ani staging neexistovaly. Celý velký zdroj šel výhradně otevřít a byl nezávisle přečten: všech 1024 bloků po 4 MiB odpovídalo původnímu deterministickému bloku (SHA-256 `D92621500ED84CA76E1F997884E892E9AB4D16831BED00D3CF96FF229AC6AEDD`). Konfigurace soukromé instance byla obnovena ze zálohy; velký vlastní zdroj po ověření uklizen. Původní tři uživatelské instance zůstaly spuštěné.

Evidence mimo repozitář: `C:\Samandarin\builds\dirsync\copy-unload-20261015\results.json`, `config-before.reg` a malá zdrojová/cílová data.

## Co tím je a není ověřeno

Tento dvoukrokový postup odpovídá současnému jádru: při aktivním modalním příkazu jde Alt+F4 nejprve dialogu pluginu. Ochrana `AlreadyInPlugin > 0` v `src/mainwnd3.cpp:11221` brání běžnému zavření hlavního okna během pluginového callbacku. `CPluginData::Unload` v `src/plugins1.cpp:3157` kontroluje `CanUnloadPlugin`; kolem řádku 3248 volá Release před odmapováním knihovny. Běžné ukončení volá `Plugins.UnloadAll` v `src/mainwnd3.cpp:11615`. Tyto ochrany nebyly obcházeny ani měněny.

Ověřen je tedy kontrakt force Release s aktivním kopírovacím workerem a skutečné běžné ukončování zahájené při kopírování, při němž nejprve skončí dialog/worker. **Není tvrzeno, že se hlavní proces běžně ukončil se stále aktivním workerem.** GUI případ zasáhl přenos; řízené přerušení Verify pokrývají nativní testy.

Nebylo provedeno odhlášení/restart/vypnutí Windows, kritický WM_ENDSESSION ani násilné ukončení procesu během kopie. Force Release není hard kill a neposkytuje crash recovery: při náhlé ztrátě procesu se cleanup kód nemusí vykonat a vlastní staging může zůstat. SMB, zaseknuté I/O, plný disk a jiná zařízení nejsou pokryté. Místní časy nejsou zárukou maximální doby ukončení pro libovolné zařízení. Tyto meze nevyžadují rozšiřování SDK ani předběžné zavádění transakční obnovy nad úroveň současného jádra.
