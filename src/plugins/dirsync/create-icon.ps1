# SPDX-FileCopyrightText: 2026 Open Salamander Authors
# SPDX-License-Identifier: GPL-2.0-or-later
# Original pixel artwork. Regenerate the ICO and transparent PNG with this script.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$pixels = @(
    '................',
    '..........G.....',
    '..........GG....',
    '..GGGGGGGGGGG...',
    '..GGGGGGGGGGGG..',
    '..GGGGGGGGGGG...',
    '..........GG....',
    '..........G.....',
    '.....B..........',
    '....BB..........',
    '...BBBBBBBBBBB..',
    '..BBBBBBBBBBBB..',
    '...BBBBBBBBBBB..',
    '....BB..........',
    '.....B..........',
    '................'
)
$bitmap = [Drawing.Bitmap]::new(16, 16)
for ($y = 0; $y -lt 16; $y++) {
    for ($x = 0; $x -lt 16; $x++) {
        $pixel = $pixels[$y][$x]
        if ($pixel -eq '.') { continue }
        $edge = $x -eq 0 -or $x -eq 15 -or $y -eq 0 -or $y -eq 15
        if (-not $edge) {
            $edge = $pixels[$y-1][$x] -ne $pixel -or $pixels[$y+1][$x] -ne $pixel -or
                    $pixels[$y][$x-1] -ne $pixel -or $pixels[$y][$x+1] -ne $pixel
        }
        $color = if ($pixel -eq 'G') {
            if ($edge) { '#237447' } else { '#65D888' }
        } else {
            if ($edge) { '#265B9D' } else { '#78BBFF' }
        }
        $bitmap.SetPixel($x, $y, [Drawing.ColorTranslator]::FromHtml($color))
    }
}
$bitmap.Save((Join-Path $PSScriptRoot 'dirsync.png'), [Drawing.Imaging.ImageFormat]::Png)
# Uncompressed BGRA ICO frames plus an AND mask support native Win32 loaders.
$frames = foreach ($scale in 1, 2, 3) {
    $size = 16 * $scale
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    $writer.Write([uint32]40); $writer.Write([int]$size); $writer.Write([int]($size*2))
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]0); $writer.Write([uint32]($size*$size*4))
    1..4 | ForEach-Object { $writer.Write([uint32]0) }
    for ($y = $size-1; $y -ge 0; $y--) {
        for ($x = 0; $x -lt $size; $x++) {
            $color = $bitmap.GetPixel([int][Math]::Floor($x/$scale), [int][Math]::Floor($y/$scale))
            $writer.Write([byte]$color.B); $writer.Write([byte]$color.G)
            $writer.Write([byte]$color.R); $writer.Write([byte]$color.A)
        }
    }
    $stride = [int]([Math]::Ceiling($size/32)*4)
    for ($y = $size-1; $y -ge 0; $y--) {
        $mask = [byte[]]::new($stride)
        for ($x = 0; $x -lt $size; $x++) {
            if ($bitmap.GetPixel([int][Math]::Floor($x/$scale), [int][Math]::Floor($y/$scale)).A -eq 0) {
                $index = [int][Math]::Floor($x/8)
                $mask[$index] = $mask[$index] -bor (128 -shr ($x%8))
            }
        }
        $writer.Write($mask)
    }
    $writer.Flush()
    ,$stream.ToArray()
    $writer.Dispose(); $stream.Dispose()
}
$output = [IO.File]::Create((Join-Path $PSScriptRoot 'dirsync.ico'))
$ico = [IO.BinaryWriter]::new($output)
$ico.Write([uint16]0); $ico.Write([uint16]1); $ico.Write([uint16]$frames.Count)
$offset = 6 + 16*$frames.Count
for ($i = 0; $i -lt $frames.Count; $i++) {
    $size = 16*($i+1)
    $ico.Write([byte]$size); $ico.Write([byte]$size)
    $ico.Write([uint16]0); $ico.Write([uint16]1); $ico.Write([uint16]32)
    $ico.Write([uint32]$frames[$i].Length); $ico.Write([uint32]$offset)
    $offset += $frames[$i].Length
}
foreach ($frame in $frames) { $ico.Write([byte[]]$frame) }
$ico.Dispose(); $output.Dispose(); $bitmap.Dispose()
