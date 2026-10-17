# Místní zkouška neutrálního exportu

Tyto nástroje zůstávají u původního forku pod `tools/`. **Nejsou součástí výběru exportu**: obsahují původní hledaná označení a nesmějí být kopírovány do nového projektu. Export vybírá jen `src/plugins/dirsync/` a `doc/directory-sync-plugin*`; SDK/UI snapshoty a licence přidává samostatně. Nic neposílá na remote, nezakládá GitHub repozitář, nepřepisuje zdrojové refs ani nemaže adresáře. Všechny testovací cíle musí být nové přímé podadresáře odděleného evidenčního kořene.

- `neutral_filter.py`: callbacky pro historické texty, cesty a zprávy vybraných commitů. Původní binární data a licenční hlavičky se nepřepisují. Neznámé formulace/kódování s výskytem zastaví export. Seznam vybraných blobů brání zpracování nesouvisejících souborů jádra, které filtrační nástroj načítá před výběrem cest.
- `trial_export.py`: nový klon pomocí `--no-local`, filtrování, porovnání každého vlastního commitu, doplnění 52 podporujících souborů, licence a neutrálních atribucí, audit a `git fsck`. Zprávy pluginových commitů, autoři a časy se ověřují proti originálu; u historických zdrojů smí kromě dokumentů změnit jen ověřený diagnostický popisek testu. V aktuálním tipu zůstávají všechny vlastní zdroje bajtově shodné s Git bloby vstupu.
- `audit_export.py`: samostatná kontrola všech dosažitelných blobů, commitů, tagů, refs a cest každé historické revize. Čte texty UTF-8, BOM UTF-16 a rozpoznatelné ASCII/UTF-16 řetězce v binárních datech. Samostatná zkratka je celé slovo, nikoli náhodný podřetězec identifikátoru. Nejde o OCR, dekompilaci nebo hledání ve vzdálených cílech odkazů.
- `verify_trial.py`: nový nezávislý klon výsledku, audit, integrita a kontrola všech uložených objektů, absence sdílených objektových souborů a pozitivní test detektoru na samostatném negativním kontrolním repozitáři. Testuje i záznamy, které už nejsou v posledním checkoutu, anotovaný tag a callbackové okrajové případy.

Použité rozhraní callbacků a chování filtrování popisuje [oficiální manuál git-filter-repo](https://github.com/newren/git-filter-repo/blob/main/Documentation/git-filter-repo.txt). Autor nástroje: Elijah Newren a přispěvatelé. Nástroj se nepřibaluje do exportu ani neinstaluje globálně. Ověřená verze: **2.47.0**, `git_filter_repo.py` SHA-256 `67447413e273fc76809289111748870b6f6072f08b17efe94863a92d810b7d94`.

## Opakování zkoušky

Použít nový název cíle, nikoli existující adresář. Příklad z kořene původního checkoutu; `--head` je výslovně fixovaný vstup. Před skutečnou migrací jej nahradit schváleným migračním commitem a zopakovat celý audit. `--preparation-commit` standardně ukazuje na `3552fafb...`, odkud vznikne 70 přesných pravidel podle již zkontrolované neutrální úpravy textů; nedovozuje se z rodiče libovolného novějšího HEAD.

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
python -m pip install --disable-pip-version-check --no-deps --target C:\Samandarin\builds\dirsync-export-dryrun-20261017\tool-runtime git-filter-repo==2.47.0
python tools/directory-sync-export/trial_export.py --source C:\Samandarin\directory-sync-plugin --evidence-root C:\Samandarin\builds\dirsync-export-dryrun-20261017 --name another-trial --tool-runtime C:\Samandarin\builds\dirsync-export-dryrun-20261017\tool-runtime --head 3552fafbec51af9f47044f9c33484660a0643aa2
python tools/directory-sync-export/verify_trial.py --trial C:\Samandarin\builds\dirsync-export-dryrun-20261017\another-trial --evidence-root C:\Samandarin\builds\dirsync-export-dryrun-20261017 --name another-trial-check
```

Již nainstalovaný runtime znovu neinstalovat; runner ověřuje jeho verzi. Evidenční kořen musí existovat. Každý shellový příkaz má podle pravidel forku limit 120 s; runner i auditor omezují jednotlivé Git procesy na 110 s. Na pomalém stroji oddělit klonování/filtrování od verifikace a celkový příkaz ukončit do limitu. `--resume-filtered` je pouze pro dokončení verifikace po již dokončeném filtru: vyžaduje existující commit-map, přesný očekávaný filtrovaný HEAD a žádný remote, nic nefiltruje znovu. Nepoužívat `--force`, sdílené objekty, `--mirror` nebo push.

Výsledné JSON protokoly, pravidla s původními texty, nekrácené mapy a konzolové logy zůstávají v evidenčním kořeni mimo export. Zkouška nepřestavuje MSBuild projekty ani neopravuje jejich relativní cesty; funkční samostatný build je další etapa.
