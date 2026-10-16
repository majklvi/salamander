# Skutečný nedostatek místa: postup pro izolovaný NTFS VHD

> **Datovaný návrh / evidence vývoje.** Aktuální stav, instalace, ovládání a omezení jsou v [hlavním návodu](directory-sync-plugin.md). Níže uvedené tehdejší rozsahy, názvy ovládacích prvků, počty testů a neprovedená ověření popisují příslušnou etapu; nejsou souhrnem současné verze.


Připraveno 15. října 2026. Jde o postup přípravy; výsledky následně provedeného běhu jsou v [samostatném záznamu skutečných NTFS VHD testů](directory-sync-plugin-vhd-results.md). [Řízené testy chyb 112/39](directory-sync-plugin-copy-space.md) zůstávají odděleným druhem důkazu. Vytvoření tohoto postupu samo neprovedlo připojení ani formátování a neměnilo plugin, SDK nebo hostitele.

## Oprávnění a rozdělení práce

Předchozí CreateVirtualDisk uspělo, ale AttachVirtualDisk v běžném tokenu vrátilo 1314. Microsoft pro AttachVirtualDisk uvádí oprávnění **SeManageVolumePrivilege**; Mount-DiskImage vyžaduje pro VHD administrátorská oprávnění. Samotné členství účtu ve skupině Administrators nemusí při běhu s filtrovaným UAC tokenem stačit. [AttachVirtualDisk](https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-attachvirtualdisk), [Mount-DiskImage](https://learn.microsoft.com/en-us/powershell/module/storage/mount-diskimage).

Doporučený postup:

1. Uživatel otevře **Windows PowerShell → Spustit jako správce** a sám potvrdí UAC. Pokud jde o standardní účet, tento krok provede správce s oprávněným účtem; přihlašovací údaje se nepředávají do chatu.
2. Zvýšená relace slouží jen k vytvoření/připojení, inicializaci, formátování vlastního prázdného VHD a jeho následnému odpojení.
3. Příprava syntetických souborů a testovací Samandarin poběží pod obvyklým uživatelským účtem bez zvýšení oprávnění. **Není třeba restartovat Codex ani Samandarin jako správce.** Nová testovací složka na VHD musí tomuto účtu umožnit běžné vytváření souborů.
4. Pokud i zvýšená relace hlásí 1314, správce prověří efektivní oprávnění účtu a firemní zásady. Neobchází se UAC ani nemění bezpečnostní zásady. SeManageVolumePrivilege je ve výchozím nastavení určeno místním správcům; není důvod je trvale přidávat běžnému účtu. [Microsoft: Perform volume maintenance tasks](https://learn.microsoft.com/previous-versions/windows/it-pro/windows-10/security/threat-protection/security-policy-settings/perform-volume-maintenance-tasks).

V administrátorském PowerShellu lze nejprve provést pouze tyto kontroly:

```powershell
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
whoami /priv
```

První výsledek má být True. U SeManageVolumePrivilege je důležitá přítomnost v tokenu; Disabled samo o sobě není důvod měnit zásady, protože nástroj může privilegium při operaci aktivovat. Pro přípravu použít standardní Správu disků / Storage cmdlety, nikoli znovu slepě spouštět původní nízkoúrovňovou sondu.

## 1. Vytvoření čerstvého malého obrazu

- Zvolit nový jedinečný adresář, například `C:\Samandarin\builds\dirsync\vhd-space-<id>`. `<id>` se nahradí časem nebo GUID; existující fixture se nepoužije znovu.
- Obraz: `space-test.vhd`, **pevná velikost 256 MiB**. Předem ověřit alespoň 2 GiB volného místa na hostitelském disku pro obraz, malé zdroje a rezervu. Pevný obraz omezuje další růst souboru obrazu při plnění VHD; neslibuje ochranu před současným zaplněním hostitelského disku jinou aplikací.
- Ve **Správě disků spuštěné jako správce** zvolit Akce → Vytvořit virtuální pevný disk; zadat přesnou novou cestu, VHD, 256 MB a pevnou velikost. Nevytvářet z existujícího fyzického disku ani existujícího obrazu. Standardní Správa disků stačí; instalace Hyper-V ani modul New-VHD nejsou potřeba. [Microsoft: správa VHD](https://learn.microsoft.com/en-us/windows-server/storage/disk-management/manage-virtual-hard-disks).
- Dialog obecné inicializace disků případně zavřít. Před inicializací nejprve ověřit níže uvedenou vazbu konkrétního souboru obrazu na nový disk. Číslo disku ani písmeno jednotky se nepřebírá z příkladu.

## 2. Ověření identity před inicializací a formátováním

Ve zvýšené relaci doplnit skutečnou cestu a provést tento kontrolní blok. Nic v něm nepřepisuje disk:

```powershell
$ErrorActionPreference = 'Stop'
$vhdPath = (Resolve-Path -LiteralPath 'C:\Samandarin\builds\dirsync\vhd-space-<id>\space-test.vhd').Path
$image = Get-DiskImage -ImagePath $vhdPath
if (-not $image.Attached) { throw 'Testovací obraz není připojen.' }
$disks = @($image | Get-Disk)
if ($disks.Count -ne 1) { throw 'Nejednoznačná vazba obrazu na disk.' }
$testDisk = $disks[0]
if ($testDisk.IsBoot -or $testDisk.IsSystem -or
    $testDisk.PartitionStyle -ne 'RAW' -or $testDisk.NumberOfPartitions -ne 0 -or
    $testDisk.Size -lt 240MB -or $testDisk.Size -gt 272MB) {
    throw 'Disk neodpovídá čerstvému prázdnému testovacímu VHD.'
}
if ($image.DevicePath -ne "\\.\PHYSICALDRIVE$($testDisk.Number)") {
    throw 'Neshoda identity obrazu a disku.'
}
$image | Format-List ImagePath, Attached, DevicePath
$testDisk | Format-List Number, UniqueId, Size, PartitionStyle, NumberOfPartitions, IsBoot, IsSystem
```

Vazbu `Get-DiskImage → Get-Disk → Get-Partition → Get-Volume` dokumentuje [Microsoft Get-DiskImage](https://learn.microsoft.com/en-us/powershell/module/storage/get-diskimage). Ručně zkontrolovat vypsanou přesnou cestu, kapacitu a shodné číslo nového disku ve Správě disků. Pokud kontrola neprojde, zastavit přípravu; nepřidávat Clear-Disk, přepínače Force ani výjimku pro existující oddíly.

Teprve po této kontrole ve Správě disků inicializovat **jen tento čerstvý disk**, například jako GPT, vytvořit jeden jednoduchý datový oddíl a rychle jej naformátovat jako **NTFS**, běžná 4KiB alokace, štítek `DIRSYNC_SPACE`. Kompresi nezapínat. Písmeno nechat přidělit systémem a zaznamenat skutečný výsledek. Formátování je příprava nového prázdného testovacího svazku; neprovádí se na existujících uživatelských discích.

Znovu načíst vazbu ze souboru obrazu a uložit výpis:

```powershell
Get-DiskImage -ImagePath $vhdPath | Get-Disk | Get-Partition | Get-Volume |
    Format-List DriveLetter, Path, UniqueId, FileSystem, FileSystemLabel, Size, SizeRemaining
```

Před každým dalším plněním ověřit přes tento řetězec stejné UniqueId svazku, NTFS, štítek, kapacitu a aktuální písmeno. Samotné písmeno nebo štítek nestačí. Je-li potřeba obraz znovu připojit, zvýšená relace použije `Mount-DiskImage -ImagePath $vhdPath -Access ReadWrite`; znovu se provede kontrola identity, **nikoli opakované formátování**.

## 3. Příprava dat běžným uživatelem

Zdroj leží mimo VHD v novém adresáři pod stejnou fixture na místním NTFS. Na ověřeném VHD vytvořit oddělené nové cílové složky `main-right` a `ads-right`. Testovací rezervní soubor `space-reserve.bin` umístit mimo oba porovnávané kořeny, například do samostatné složky `fixture-control` na VHD. Nikdy se nestane součástí synchronizačního plánu.

Pro každý případ připravit samostatný zdroj s tímto pořadím názvů:

| Soubor | Test hlavních dat | Test ADS |
| --- | --- | --- |
| `0-done.txt` | Malý obsah DONE a ADS `:note` = DONE-ADS | Totéž |
| `a-large.bin` | 64 MiB skutečných hlavních dat, malý ADS | 16 MiB hlavních dat a 64 MiB ADS `:漢字` |
| `z-next.txt` | Malý obsah NEXT | Totéž |

Nové soubory zakládat přes CreateNew a skutečně zapisovat nenulové bloky; bez sparse, komprese, EFS a reparse položek. Před testem uložit SHA-256 hlavních dat i každého ADS, délky, UTC mtime a relevantní atributy. Ověřit přístup k novým kořenům pod běžným účtem. Žádný existující cíl pro tyto tři názvy nesmí být přítomen.

Rezervní soubor vyplnit skutečnými zápisy tak, aby na VHD zbývalo přibližně **32 MiB**. Předem spočítat horní mez `volné místo − 32 MiB`, použít například 1MiB bloky a po dosažení meze flush/close; znovu změřit dostupné místo. Celkový rezervní zápis nesmí překročit kapacitu testovacího VHD. Při každém běhu znovu ověřit identitu svazku. Nepsat neomezenou smyčkou „dokud neselže“ a nevytvářet rezervu na C:.

To dovolí dokončit první malý soubor, ale ne celou následující kopii. U ADS případu se hlavní proud vejde, zatímco celá sada proudů se nevejde. Skutečný okamžik selhání závisí na alokaci CopyFileExW/NTFS; předem se nepředpokládá nenulový průběh hlavního přenosu.

## 4. Dva běhy produkčního pluginu

Použít izolovanou distribuci Samandarin-0.16 s vlastní RegFile konfigurací a produkční SPL/SLG, jejichž hashe odpovídají build výstupu. Zálohovat konfiguraci; instalované uživatelské instance ponechat beze změn. Příprava rezervy skončí ještě před Compare.

1. Nastavit příslušný zdroj a prázdný cílový kořen VHD, provést Compare a ověřit tři návrhy Copy to right. Potvrdit jen kopie do nových cílů. Spustit nejprve hlavní data, potom samostatný ADS případ.
2. Zaznamenat průběh, skutečný Win32 kód, finální souhrn a stav vyžadující nové Compare. Neočekávat nutně konkrétní kód 112: uložit i případný 39 nebo jiný skutečný výsledek a posoudit jej podle příčiny.
3. Očekávaný běžný výsledek: **1 dokončený, 1 chybný, 1 nezahájený**; první main/ADS zachovány, finální velký a následující cíl nevznikly. Pokud se první položka nedokončí nebo se chyba objeví už při rezervaci stagingu, zaznamenat skutečnou fázi a upravit novou testovací rezervu; neoznačit to za chybu během datového přenosu.
4. Nezávisle vypsat i skryté položky a proudy cíle. Ověřit, že staging byl uklizen, případně že existující zbytek přesně odpovídá hlášené cestě. Zkontrolovat všechny zdrojové hashe/metadata a data/ADS dokončeného cíle, ověřit uvolnění handlů.
5. Bez mazání či změn synchronizovaných souborů uvolnit **jen vlastní rezervu fixture**: po dokončení workeru otevřít přesnou ověřenou cestu `fixture-control\space-reserve.bin` a zkrátit tento kontrolní soubor na nulu. Je to řízení testovacích podmínek mimo plugin, nikoli implementace přepisů nebo mazání uživatelských položek. Znovu ověřit nárůst volného místa. Případný zbylý staging zatím zachovat jako důkaz a vyřešit před dalším plánem, aby se nenavrhl opačným směrem.
6. Nové Compare: dokončený pár má být Equal/Skip, zbývající dva soubory nové kopie. Nový pokus musí dokončit hlavní data i všechny ADS a projít nezávislou kontrolou hashů a podporovaných metadat. Neprovádět starý plán ani ručně nastavovat přepis existujícího dokončeného cíle.

Výsledky oddělit: chyba před prvními daty, po části hlavního proudu, nebo při ADS; samotný procentní průběh nedokazuje konkrétní alokační fázi. Pokud skutečný běh v některé fázi nelze navodit, uvést ji jako neprovedenou, ne jako potvrzenou regresní simulací.

## 5. Evidence a ukončení

Pro oba běhy uložit mimo VHD: SHA commitu a SPL/SLG, verzi hostitele, cestu/kapacitu obrazu, identitu disku a svazku, velikosti a volné místo před/po, souhrn a původní kód chyby, hashe main/ADS, seznam zbytků a výsledek nového pokusu. Screenshoty doplňují, nenahrazují filesystem kontrolu. Případnou opravu vyhodnotit až podle skutečného výsledku; rozsah pluginu zůstává copy-new a současné SDK.

Zavřít dialog pluginu, počkat na konec workeru a normálně ukončit soukromého hostitele; obnovit jeho zálohovanou konfiguraci. Všechny nástroje přepnout mimo cílovou jednotku. Ve zvýšené relaci pak:

```powershell
Dismount-DiskImage -ImagePath $vhdPath -ErrorAction Stop
Get-DiskImage -ImagePath $vhdPath | Format-List ImagePath, Attached, DevicePath
```

Vyžadovat Attached=false. Při chybě odpojení nejprve zjistit otevřené handly; nepoužívat násilné odpojení během I/O. Obraz a evidenci ponechat uložené pro kontrolu; tento postup neobsahuje smazání obrazu ani rekurzivní úklid. Odpojení VHD samo data nemaže. [Microsoft: odpojení VHD](https://learn.microsoft.com/en-us/windows-server/storage/disk-management/manage-virtual-hard-disks#detach-a-vhd).

Přepisy/mazání uživatelských položek, SMB, změny SDK, globální kvóty, změny bezpečnostních zásad a pokročilá transakční obnova jsou mimo tento postup.
