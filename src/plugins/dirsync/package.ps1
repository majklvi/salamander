# SPDX-FileCopyrightText: 2026 Open Salamander Authors
# SPDX-License-Identifier: GPL-2.0-or-later
# Manual-install preview package; no catalog, signing or host configuration changes.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BinaryDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$binary=(Resolve-Path -LiteralPath $BinaryDirectory).Path
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) { throw 'Output directory must be new; existing artifacts are never replaced.' }
$inputs=@('dirsync.spl','lang\english.slg','lang\czech.slg')
foreach($relative in $inputs) {
    $path=Join-Path $binary $relative
    if(!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $path" }
    $stream=[IO.File]::OpenRead($path)
    $reader=[IO.BinaryReader]::new($stream)
    try {
        if($reader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE module: $path" }
        $stream.Position=0x3c; $offset=$reader.ReadInt32(); $stream.Position=$offset
        if($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw "Expected x64 PE: $path" }
    } finally { $reader.Dispose() }
}
$payload=Join-Path $output 'payload\dirsync'
New-Item -ItemType Directory -Path (Join-Path $payload 'lang'),(Join-Path $payload 'licenses') | Out-Null
foreach($relative in $inputs) { Copy-Item -LiteralPath (Join-Path $binary $relative) -Destination (Join-Path $payload $relative) }
$guide=[IO.File]::ReadAllText((Join-Path $repo 'doc\directory-sync-plugin.md'))
$guide=[regex]::Replace($guide,'\]\((directory-sync-plugin[^)]*)\)','](https://github.com/majklvi/salamander/blob/feature/directory-sync-plugin/doc/$1)')
[IO.File]::WriteAllText((Join-Path $payload 'README.cs.md'),$guide,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $payload 'LICENSE')
Copy-Item -LiteralPath (Join-Path $repo 'src\third_party\darkmodelib\LICENSE.md') -Destination (Join-Path $payload 'licenses\darkmodelib-MPL-2.0.md')
Copy-Item -LiteralPath (Join-Path $repo 'src\third_party\darkmodelib\LICENSE-MIT.md') -Destination (Join-Path $payload 'licenses\darkmodelib-MIT.md')
Get-ChildItem -LiteralPath (Join-Path $repo 'src\third_party\darkmodelib\docs') -Filter 'LICENSE-*.md' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $payload ('licenses\'+$_.Name))
}
$notice=@'
Directory Synchronization 0.1 preview, x64, for Samandarin 0.16.
Copyright (c) 2026 Open Salamander Authors. GPL-2.0-or-later.
Source: https://github.com/majklvi/salamander/tree/feature/directory-sync-plugin/src/plugins/dirsync
Includes darkmodelib by ozone10; licenses are in licenses/.
Manual installation: extract dirsync/ into the host plugins/ directory, then
Plugins > Plugins Manager > Add > dirsync.spl if not already auto-registered.
README.cs.md is the current guide;
English and Czech language modules are included in lang/.
its technical documentation links refer to doc/ in the source repository.
This local development package is unsigned and is not a catalog release.
'@
[IO.File]::WriteAllText((Join-Path $payload 'NOTICE.txt'),$notice,[Text.UTF8Encoding]::new($false))
$manifest=@(Get-ChildItem -LiteralPath $payload -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($payload.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}
})
[IO.File]::WriteAllText((Join-Path $payload 'files.json'),($manifest|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
$archive=Join-Path $output 'plugin_5.0_dirsync_0.1_x64.zip'
Compress-Archive -LiteralPath $payload -DestinationPath $archive
$hash=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output 'package-sha256.txt'),"$hash  $([IO.Path]::GetFileName($archive))`r`n",[Text.UTF8Encoding]::new($false))
Write-Output $archive
Write-Output "SHA256 $hash"
