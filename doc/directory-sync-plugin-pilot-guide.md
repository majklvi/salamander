# Synchronizace adresářů — pilotní testování

Vývojová verze 0.1.3 pro **Samandarin 0.16 x64**, čeština i angličtina. Balíček je nepodepsaný a obsahuje pouze plugin. Pilot je určen pro **kopie místních NTFS dat**; SMB, síťové disky a změny SDK jsou mimo tento pilot.

Klikáním do záhlaví sloupce přepínáte vzestupné/sestupné řazení souborů uvnitř adresářů. Pravé tlačítko na záhlaví nebo sekce **Sloupce / Columns** v nastavení pluginu skryje či zobrazí volitelné sloupce. Názvy vlevo i vpravo, cesta adresáře a operace zůstávají. Volby sloupců a řazení se pamatují; skrytí sloupce nevynechává soubory ze synchronizace.

Výsledky jsou seskupené podle relativních adresářů (`\` je kořen). Pod záhlavím jsou názvy souborů; záhlaví samo není položka operace a nevybírá své potomky. Prázdné skupiny po filtrování zmizí. Adresář určený k odstranění má vlastní řádek v nadřazené skupině.

## Instalace

1. Zavřete instance Samandarinu používající starší plugin. Rozbalte ZIP a celý adresář `dirsync` vložte do `plugins` vedle `salamand.exe`; zachovejte podadresář `lang` s oběma SLG.
2. V **Plugins → Plugins Manager → Add…** vyberte `plugins/dirsync/dirsync.spl`, pokud již není registrovaný. Spusťte **Directory Synchronization → Directory synchronization…** (česky Synchronizace adresářů).
3. V **Settings… / Nastavení…** ponechte pro začátek **Keep originals / Uchovávat originály** zapnuté a vyberte existující adresář obnovy mimo oba porovnávané stromy, který není ani jejich předkem. Zajistěte místo pro zálohy i právě přenášený soubor. Volby se pamatují. Jazyk a světlý/tmavý režim určuje hostitel.

## Krátký pracovní postup

1. Vyberte dvě oddělené testovací kopie adresářů. Nastavte masku a volby, pak **Compare / Porovnat**. Tento krok pouze čte data.
2. Prohlédněte návrhy a důvody. Filtry lze kombinovat; pravým tlačítkem nebo Shift+F10 měňte návrhy zvýrazněných řádků. **Skip** položku vynechá. Popisky akcí lze zapnout v nastavení.
3. **Synchronize… / Synchronizovat…** nabízí zvlášť nové kopie, přepisy a mazání. Každá operace má vlastní potvrzení. **Provádějí se všechny viditelné způsobilé návrhy, nikoli jen zvýrazněné řádky.** Skryté a Skip řádky jsou vynechané; zkontrolujte počty i režim obnovy v potvrzení.
4. Mazání je dostupné v asymetrii: **levá strana je zdroj**, pravostranné položky chybějící vlevo jsou kandidáty k odstranění. Adresáře vyžadují volbu Empty folders / Prázdné adresáře. Obsah se maže před rodiči, adresář jen když je skutečně prázdný; vynechaný potomek chrání rodiče. Kořeny a společné prázdné adresáře zůstávají.
5. Po každé operaci přečtěte souhrn a spusťte nové Compare. Nové porovnání zahodí ruční změny návrhů, takže potřebný Skip znovu nastavte. Změny šesti voleb skenu se uplatní až při příštím Compare; změna cesty či masky zneplatní dosavadní plán.
6. Křížek v pravé části Compare nebo Synchronize zastaví příslušnou práci. Zavření počká na worker. **Již dokončené změny zůstávají provedené**, automatické vrácení není součástí pluginu.

Vyzkoušejte běžné kopírování, přepis a malé vlastní odstranění; poté ruční obnovu do nového kontrolního adresáře. U většího seznamu sledujte odezvu, filtry, klávesnici a změny velikosti okna. Uveďte i zkušenost s jiným DPI či více monitory. Není nutné opakovat laboratorní chybové testy.


## Přímé odstranění z kontextového menu

Horní „Označit ke kopírování…“ / „Označit k přeskočení“ pouze mění návrhy. Oddělené **Odstranit vybrané… → Vlevo… / Vpravo… / Na obou stranách…** odstraňuje právě zvýrazněné položky. Bez Shiftu do Koše, se Shiftem při aktivaci příkazu strany trvale. Režim a skutečné cesty ověřte v potvrzení OK/Storno. Uchovat originály se na tento příkaz nevztahuje; trvalé mazání nemá zálohu pluginu. Adresáře musí být prázdné, rodič nevybírá potomky. První chyba zastaví dávku a po běhu je nutné nové Porovnat. Obnovu z Koše provádějte v Koši Windows.

## Obnova a hlavní omezení

- Při ON se před přepisem/mazáním nejprve uloží a ověří zařazené originály. V cestě uvedené v souhrnu najdete `manifest.jsonl` a prováděcí protokol. U synchronizačních příkazů při OFF **nevzniká nová záloha ani trvalý protokol a nepoužívá se Koš**. Starší zálohy se automaticky neuklízejí a zabírají místo.
- Ruční obnova: v manifestu použijte `saved` u úspěšně uložené položky. Strukturu podle adresářových záznamů nejprve vytvořte v novém kontrolním adresáři; soubory přeneste běžným F5 z jejich uložených cest. Nekopírujte celý pomocný strom běhu do původního kořene. Ověřená obnova zahrnuje strukturu, souborová data, ADS a souborový čas. F7 vytvoří adresáře s novými časy; přesná obnova adresářových metadat a původních oprávnění není automatická.
- Skryté a systémové položky jsou zahrnuté podle masky a rozsahu, i když je hlavní panely nezobrazují. Atributy H/S ukazuje tooltip řádku. By content porovnává pouze hlavní data, nikoli ADS; pevná tolerance časů je 3 sekundy.
- Samostatné kopírování prázdných adresářů zatím není podporované. Reparse/junction/symlink, EFS a nepodporované cloud/offline položky se blokují; adresáře s vlastními ADS se před mazáním odmítnou. Read-only cíle se nepřepisují ani nemažou obcházením atributů.
- První chyba zastaví zbývající práci. Nejde o transakci ani úplný NTFS zálohovací nástroj; během běhu neměňte testovací stromy jiným programem. Mohou zůstat vytvoření prázdní rodiče, neúplný protokol nebo oznámený zbytek stagingu.
- Přenos zatím používá vlastní okno pluginu, bez společné fronty jádra, přenosu na pozadí a rychlostního limitu. Ověřený runtime je x64. Běh na jiném systémovém DPI a přechody mezi monitory zbývají ověřit; procházení tisíců řádků přes UI Automation dříve způsobovalo prodlevy.

Podrobnosti jsou v přiloženém [aktuálním návodu](directory-sync-plugin.md) (`README.cs.md` v ZIP).

## Jak hlásit chybu

Pošlete hlášení autorovi pluginu; souhrn výsledku lze zkopírovat přes **Ctrl+Shift+C**, cestu vybraného řádku přes **Ctrl+C**. Hlášení ideálně obsahuje:

- Identifikaci balíčku: název ZIP a SHA-256 z přiloženého `package-sha256.txt`; hash `dirsync.spl` je také v `files.json`.
- Verzi Samandarinu a Windows, jazyk, světlý/tmavý režim, DPI monitorů a potvrzení místního NTFS.
- Přesný postup, masku, volby skenu, filtry, ruční návrhy, druh operace a Keep originals ON/OFF.
- Očekávaný a skutečný výsledek; počty a přesnou chybu ze souhrnu, případně snímek okna. U prodlev počet řádků, délku čekání a zda běžel nástroj UI Automation.
- Při ON příslušné `manifest.jsonl` a `delete.jsonl` či `overwrite.jsonl`, pokud existují. Při OFF jejich nepřítomnost není chyba. Před odesláním odstraňte citlivé cesty/názvy z kopie hlášení; původní protokoly si ponechte pro obnovu. Neposílejte skutečná soukromá data; nejlepší je malá syntetická sada reprodukující problém.

Při neočekávané změně dat další zápisy zastavte, uchovejte souhrn i obnovovací běh a nejprve zkontrolujte kopie v odděleném adresáři.

Od 0.1.3 lze v Nastavení → Sloupce nezávisle zapnout relativní velikost a relativní čas. Obě volby jsou standardně vypnuté. U rozdílných hodnot dvojic souborů zobrazují rozdíl vůči protější straně; shodné hodnoty a chybějící protějšek ponechávají absolutní údaje. Řazení se nadále řídí skutečnou velikostí a časem. Podrobnosti uvádí README.cs.md.

Tabulka má symetrické pořadí Název | Velikost | Změněno | Operace | Změněno | Velikost | Název. První trojice patří levému adresáři, poslední pravému. Volba Nastavení → Sloupce → Uvádět stranu v záhlaví přidá do popisků vlevo/vpravo; standardně je vypnutá a pamatuje se.
