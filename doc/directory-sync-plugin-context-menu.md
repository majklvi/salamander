# Kontextové menu výsledků — průzkum a návrh

Stav 16. října 2026. **Návrh k posouzení, menu v pluginu dosud není implementované.** Aktuální funkce a omezení popisuje [návod](directory-sync-plugin.md). Kontextová nabídka schránky v tabulkových souhrnech je samostatná existující funkce.

## Provedený průzkum

Skutečný Computer Use průchod v referenčním správci souborů, verze **11.58 x64**, dialog Synchronize directories. Použita pouze místní izolovaná NTFS sada `C:\Samandarin\builds\dirsync-menu-20261016\left` a `right`. Čtyři dvojice souborů, levostranný a pravostranný Unicode soubor a podadresář `sub` s jedním souborem na každé straně: osm výsledných souborových řádků, dvanáct fyzických souborů. Ověřeno obousměrné i asymetrické porovnání, Subdirs a by content; maska `*.*`.

Průzkum prováděl jen porovnání a změny návrhů v paměti. Synchronize ani položky Delete, Edit, Rename, kopírování vlastností, tisk a shellové nabídky nebyly spuštěny. Po zavření dialogu byly ověřeny všechny původní hashe a časy dvanácti souborů proti `before.json`: beze změny. Původní hlavní panely nebyly přesměrovány; aplikace zůstala spuštěná a byla vrácena do původního minimalizovaného stavu.

## Viditelné příkazy nabídky

Nabídka má následující skupiny, oddělené standardními separátory. Klávesy byly přečteny přímo z nabídky; funkčně se ověřovaly jen níže výslovně uvedené příkazy.

| Skupina | Příkazy a zobrazené klávesy |
| --- | --- |
| Návrhy kopírování | Mark for copying (default direction) — Ctrl+D; Unmark for copying — Ctrl+M; Mark for copying → (left to right) — Ctrl+L; Mark for copying ← (right to left) — Ctrl+R; Mark for copying in reverse direction — Ctrl+W |
| Vlastnosti pro přenos | Copy file properties → (left to right)… — Ctrl+Alt+L; Copy file properties ← (right to left)… — Ctrl+Alt+R |
| Tisk a porovnání | Print selected… — Ctrl+P; Compare left & right — Ctrl+F3 |
| Práce s jednotlivou stranou | View left — F3; View right — Shift+F3; Edit left — F4; Edit right — Shift+F4; Rename left — F6; Rename right — Shift+F6 |
| Shell a vlastnosti | Left context menu — Alt+Down; Right context menu — Alt+Shift+Down; Left properties — Alt+Enter; Right properties — Alt+Shift+Enter |
| Mazání | ← Delete left; → Delete right; Delete on both sides; bez zobrazené klávesové zkratky |

## Skutečně ověřené chování

| Situace | Pozorování |
| --- | --- |
| Pravé kliknutí na zvýrazněný soubor | Otevře nabídku; předchozí výběr zachová. |
| Dva řádky zvýrazněné přes Shift+Down; pravé kliknutí na jeden z nich | Oba zůstaly zvýrazněné. Volba směru doprava nastavila šipku u obou, včetně původně shodného souboru. Ostatní řádky se nezměnily. |
| Pravé kliknutí na jiný, dosud nezvýrazněný řádek | Původní dvojici nahradil jen nově kliknutý řádek. |
| Shift+F10 při zaměření seznamu | Otevřelo stejnou nabídku pro aktuální výběr; Escape ji zavřel bez změny návrhu. |
| Pravé kliknutí v prázdné části seznamu při existujícím výběru | Otevřelo nabídku pro dosavadní zvýrazněný řádek. Neoznačilo automaticky všechny výsledky. |
| Pravé kliknutí na skupinové záhlaví `sub` | Zvýraznilo oba zobrazené souborové řádky této skupiny a otevřelo nabídku. Edit/Rename, shellové kontexty/vlastnosti a párové porovnání byly neaktivní; View left/right zůstaly dostupné. Nejde o důkaz provádění operací nad adresářem. |
| Odlišný soubor na obou stranách; Mark for copying → | Jen změna značky na zelenou šipku. Žádný přenos se nespustil. |
| Soubor pouze vlevo | View/Edit/Rename/Context menu/Properties pro pravou stranu byly neaktivní, stejně jako reverse direction a kopírování vlastností. Oba explicitní příkazy směru zůstaly dostupné. Volba směru zprava doleva **zrušila původní značku**, nevytvořila přenos z neexistujícího souboru. |
| Filtrovaná sada | Po skrytí směru doprava zůstaly tři viditelné soubory. Ctrl+A a Ctrl+M odstranily značky jen těmto řádkům. Po opětovném zobrazení doprava zůstaly skryté návrhy včetně ručně označené shodné dvojice zachované. |
| Asymetrie, pravostranný Unicode soubor | Porovnání navrhlo červené ×. Nabídka měla dostupné příkazy existující pravé strany a Delete right; levostranné protějšky byly neaktivní. Explicitní směry kopírování byly stále dostupné — jejich provedení zde ověřeno nebylo. |

Dostupnost Compare left & right u některých jednostranných řádků sama o sobě nedokládá, jak příkaz chybějící stranu řeší. Z dostupnosti položek Delete nelze odvodit potvrzení, okamžik mazání ani jejich vztah k hlavnímu Synchronize; tyto položky se záměrně neaktivovaly. Neověřovala se klávesa Menu, stav bez jakéhokoli výběru, archivy, SMB, reparse/ACL blokování ani jiné verze aplikace. Nejde o požadavek kopírovat všechny funkce do pluginu.

## Doporučená první podoba pro plugin

Použít nativní popup menu přes současné SDK, standardní font, hostitelské barvy a separátory. Žádný nový vlastní vizuální styl. První část menu **upravuje pouze návrhy zvýrazněných viditelných souborových řádků**; přenosy, přepisy a mazání nadále spouštějí samostatná existující tlačítka se svým potvrzením.

```text
Restore default proposal                 Ctrl+D
Skip                                    Ctrl+M
Copy to right                           Ctrl+L
Copy to left                            Ctrl+R
Reverse copy direction                  Ctrl+W
Mark right-only files for deletion
─────────────────────────────────────────────
Copy paths                              ▸
    Relative paths                      Ctrl+C
    Left full paths
    Right full paths
```

Jde o navržené anglické popisky, nikoli nové existující příkazy. Zkratky by platily jen při zaměření seznamu výsledků, aby nezasahovaly do textových editací. Položka pro mazání musí zřetelně říkat **Mark**: mění pouze návrh, sama nemaže.

Pravidla rozsahu a aktivace:

- Pravé kliknutí na zvýrazněný řádek zachová výběr; na jiný řádek nejprve vybere právě tento řádek. Shift+F10 / Menu použijí aktuální výběr. Pravé kliknutí do prázdna zachová výběr; bez výběru se příkazy pro návrhy deaktivují. Nikdy se implicitně nepoužijí všechny viditelné výsledky.
- Směr se nabídne jen pro existující zdroj a podporovaný pravidelný soubor. V asymetrii bude Copy to left i Reverse neaktivní podle D3, přestože referenční nabídka explicitní směry zobrazuje dostupné. Chybějící zdroj se nebude tiše interpretovat jako Skip.
- Mark right-only files for deletion se aktivuje pouze v asymetrii pro soubory existující pouze vpravo. Žádné Delete left/both, mazání složek ani okamžitá destruktivní operace v první nabídce.
- Blokované a konfliktní řádky zůstanou nepřepisovatelné stejně jako nyní. Při smíšeném výběru se uplatní jen způsobilé řádky a stavový řádek uvede počet změněných a vynechaných. Skryté řádky zůstanou beze změny; Skip lze explicitně změnit zpět na platný návrh.
- Restore default proposal obnoví návrh z právě platného porovnání, nikoli z nové historie nebo skrytého opětovného skenu. In-memory kopie výchozích návrhů bude patřit k aktuálnímu výsledku a zneplatní se s ním.
- Kopírování cest použije Unicode schránku, jednu cestu na řádek a jen existující příslušnou stranu. Nezpřístupní neexistující cestu jako skutečný soubor. U neplatného výsledku se změny návrhů zakážou; pro neškodné zkopírování zobrazených cest není nutný nový sken.
- Náhled pluginu má ploché relativní cesty, nikoli skupinová záhlaví referenčního dialogu. Kliknutí na adresářový řádek proto nebude bez samostatného návrhu implicitně označovat celý podstrom.

## Nejmenší technické mapování a navazující možnosti

`WM_CONTEXTMENU` + hit-test nativního ListView, `LVN_KEYDOWN` pro zkratky a menu z existujícího `CSalamanderGUIAbstract` / `CGUIMenuPopupAbstract`. Cílové indexy se zachytí před otevřením menu. Změny návrhů použijí stejnou čerstvou validaci a pravidla způsobilosti jako `ValidatePreview(Intent::Apply)` / `Apply`, ne druhou nezávislou prováděcí cestu. Restore default vyžaduje jen uchovat původní `Action` u aktuálních řádků; aktuální model `Row` si po ruční úpravě původní návrh samostatně nepamatuje. Formát synchronizační historie ani SDK se kvůli tomu nemění. Pro schránku se využije stávající Unicode postup tabulkových souhrnů.

Prohlížení, porovnání obsahu, editace, přejmenování, kopírování vlastností, tisk a shellové nabídky doporučuji do první implementace menu nezahrnout. SDK má například `ExecuteAssociation` a `ViewFileInPluginViewer`, ale první neznamená interní View a druhá vyžaduje konkrétní prohlížecí plugin. Přímé univerzální volání interního porovnání/editoru tu nebylo nalezeno. Tyto příkazy proto potřebují nejprve konkrétní mapování na současný hostitel, včetně dlouhých Unicode cest a následného zneplatnění změněných výsledků. Není důvod kvůli nim nyní rozšiřovat SDK.

Po odsouhlasení podoby menu zbývá implementovat a běžně ověřit: jeden a více řádků, pravé kliknutí na jiný řádek a do prázdna, klávesnice, filtrované/Skip/blokované položky, asymetrie, Unicode schránka a změněný snapshot. Tyto budoucí testy nejsou vydávané za hotové ověření pluginového menu. Bez mazání složek, SMB testů a změn SDK.
