# Stavový řádek — 0.1.4

17. října 2026. Úprava pouze pluginu, bez SMB testů a změn SDK.

Dosavadní dva oddíly s pravým souhrnným textem odděleným svislými čárami jsou nahrazené samostatnými nativními oddíly. Vlevo zůstává stav. V klidu následují řádky celkem, viditelné, skryté a blokované/konfliktní. Při skenu jsou samostatně prohledané a porovnané položky; při čtení navíc přečtené bajty. Souborové operace mají položku X/Y, zkopírované bajty, ověřené bajty a dokončené položky. Zastavování zachovává průběhové údaje.

Šířky vycházejí z měření skutečného textu fontem status baru. Při nedostatku místa se proporcionálně zmenší a ponechají alespoň přibližně 28 % šířky stavu. Nativní tooltip každého oddílu obsahuje celý text. Počet oddílů se mění jen podle fáze, seznam zůstává přilepený ke status baru. Nezavádí se vlastní kreslení ani samostatné nastavení barev.

## Ověření

- Release x64 sestavení pluginu i obou jazykových modulů prošlo s warnings-as-errors.
- 2 419 anglických / 2 451 českých resource/layout kontrol prošlo při 96/120/144/192/240 DPI metrikách. Nové jednotlivé formátovací texty jsou lokalizované a neobsahují oddělovací svislé čáry; staré souhrnné texty byly odstraněny.
- 2 366 lifecycle kontrol prošlo. Rozšířený nativní test používá skutečný msctls_statusbar32: ověřuje počty oddílů pro klid, sken, čtení, kopírování i Stop; při pěti velikostech fontu odpovídajících 96–240 DPI a šířkách 450/900/1600/700 px ověřuje rostoucí hrany oddílů, místo pro stav a úplný dlouhý Unicode tooltip. Zachována kontrola návaznosti seznamu na status bar.
- Jde o automatizované nativní ověření ovládacího prvku a fontových metrik; ruční GUI test přepínání DPI mezi monitory nebyl v této etapě proveden.

Verze registračních údajů, SPL, českého i anglického SLG a balíčku je 0.1.4.
