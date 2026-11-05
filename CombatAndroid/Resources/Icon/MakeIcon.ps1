# exe アイコン（App.ico）を作り直すスクリプト。
#   powershell -NoProfile -ExecutionPolicy Bypass -File CombatAndroid\Resources\Icon\MakeIcon.ps1
#
# 元画像は Source.png（オープニングのカットシーン終わりの1枚。0号機の後ろ姿と、繋がったハンマー）。
# そこから頭〜肩を正方形に切り抜き、コントラストを上げ、周囲をタイトル画面の暗い紺へ落として
# 人物を浮かせ、角を丸める。
# 16/32/48 は BMP（32bit BGRA + ANDマスク）、256 は PNG で格納する。
# ここは Assets の外に置いてある（Release では Assets がそのまま配布物へコピーされるため）。

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = 'Stop'
$srcPath = Join-Path $PSScriptRoot 'Source.png'
$outPath = Join-Path $PSScriptRoot 'App.ico'
$sizes   = 16, 32, 48, 256

# Source.png 上の切り抜き範囲（正方形）。小さいサイズほど人物に寄せる
function Get-Crop([int]$s) {
    if ($s -le 16) { return New-Object System.Drawing.Rectangle 772, 492, 200, 200 }
    if ($s -le 32) { return New-Object System.Drawing.Rectangle 758, 472, 230, 230 }
    return New-Object System.Drawing.Rectangle 738, 446, 270, 270
}

$src = [System.Drawing.Image]::FromFile($srcPath)

function New-IconBitmap([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode      = 'AntiAlias'
    $g.InterpolationMode  = 'HighQualityBicubic'
    $g.PixelOffsetMode    = 'HighQuality'
    $g.CompositingQuality = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)

    # 角丸の正方形で切り抜く（16px は角丸を控えめに）
    $r = [single]($s * $(if ($s -le 16) { 0.12 } else { 0.18 }))
    $d = 2 * $r
    $w = [single]$s
    $clip = New-Object System.Drawing.Drawing2D.GraphicsPath
    $clip.AddArc(0, 0, $d, $d, 180, 90)
    $clip.AddArc($w - $d, 0, $d, $d, 270, 90)
    $clip.AddArc($w - $d, $w - $d, $d, $d, 0, 90)
    $clip.AddArc(0, $w - $d, $d, $d, 90, 90)
    $clip.CloseFigure()
    $g.SetClip($clip)

    # コントラストと彩度を上げて描く（明るい空・草が人物と同化しないように）
    $c = 1.25; $t = (1 - $c) / 2 - 0.06; $sat = 1.3
    $lr = 0.3086 * (1 - $sat); $lg = 0.6094 * (1 - $sat); $lb = 0.0820 * (1 - $sat)
    $m = New-Object System.Drawing.Imaging.ColorMatrix
    $m.Matrix00 = ($lr + $sat) * $c; $m.Matrix01 = $lr * $c;          $m.Matrix02 = $lr * $c
    $m.Matrix10 = $lg * $c;          $m.Matrix11 = ($lg + $sat) * $c; $m.Matrix12 = $lg * $c
    $m.Matrix20 = $lb * $c;          $m.Matrix21 = $lb * $c;          $m.Matrix22 = ($lb + $sat) * $c
    $m.Matrix33 = 1; $m.Matrix40 = $t; $m.Matrix41 = $t; $m.Matrix42 = $t; $m.Matrix44 = 1
    $ia = New-Object System.Drawing.Imaging.ImageAttributes
    $ia.SetColorMatrix($m)
    $crop = Get-Crop $s
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $s, $s), $crop.X, $crop.Y, $crop.Width, $crop.Height, 'Pixel', $ia)

    # 周囲を暗い紺へ落とす（中心は人物の頭〜胸の光）
    $vig = New-Object System.Drawing.Drawing2D.GraphicsPath
    $pad = $s * 0.08
    $vig.AddEllipse([single](-$pad), [single](-$pad * 0.8), [single]($s + 2 * $pad), [single]($s + 2 * $pad))
    $pb = New-Object System.Drawing.Drawing2D.PathGradientBrush $vig
    $pb.CenterPoint = New-Object System.Drawing.PointF ([single]($s * 0.5)), ([single]($s * 0.55))
    $pb.CenterColor = [System.Drawing.Color]::FromArgb(0, 5, 8, 13)
    $pb.SurroundColors = @([System.Drawing.Color]::FromArgb(250, 5, 8, 13))
    $blend = New-Object System.Drawing.Drawing2D.Blend
    $blend.Positions = [single[]](0, 0.3, 0.6, 1)
    $blend.Factors   = [single[]](0, 0.35, 0.85, 1)
    $pb.Blend = $blend
    $g.FillRectangle($pb, 0, 0, $s, $s)
    # 楕円の外（四隅）はグラデーションが届かないので同じ色で塗る
    $outside = New-Object System.Drawing.Region (New-Object System.Drawing.RectangleF 0, 0, $s, $s)
    $outside.Exclude($vig)
    $g.FillRegion((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(250, 5, 8, 13))), $outside)

    # 下端にタイトルの帯と同じ赤を細く敷く（大きいサイズだけ）
    if ($s -ge 48) {
        $band = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 199, 15, 26))
        $g.FillRectangle($band, 0, [single]($s * 0.93), $s, [single]($s * 0.07))
    }

    $g.ResetClip()
    if ($s -ge 32) {
        $edge = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(110, 140, 150, 180)), ([single]([Math]::Max(1, $s * 0.01)))
        $g.DrawPath($edge, $clip)
    }
    $g.Dispose()
    return $bmp
}

function Get-BmpEntry([System.Drawing.Bitmap]$bmp) {
    $s = $bmp.Width
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms
    # BITMAPINFOHEADER（高さは XOR + AND の2枚分）
    $bw.Write([int]40); $bw.Write([int]$s); $bw.Write([int]($s * 2))
    $bw.Write([int16]1); $bw.Write([int16]32); $bw.Write([int]0)
    $maskStride = [int]([Math]::Ceiling($s / 32.0) * 4)
    $bw.Write([int]($s * $s * 4 + $maskStride * $s))
    $bw.Write([int]0); $bw.Write([int]0); $bw.Write([int]0); $bw.Write([int]0)
    # XOR（下から上、BGRA）
    for ($y = $s - 1; $y -ge 0; $y--) {
        for ($x = 0; $x -lt $s; $x++) {
            $c = $bmp.GetPixel($x, $y)
            $bw.Write([byte]$c.B); $bw.Write([byte]$c.G); $bw.Write([byte]$c.R); $bw.Write([byte]$c.A)
        }
    }
    # AND マスク（完全透明の画素だけ 1）
    for ($y = $s - 1; $y -ge 0; $y--) {
        $row = New-Object byte[] $maskStride
        for ($x = 0; $x -lt $s; $x++) {
            if ($bmp.GetPixel($x, $y).A -eq 0) { $row[[int][Math]::Floor($x / 8)] = $row[[int][Math]::Floor($x / 8)] -bor (0x80 -shr ($x % 8)) }
        }
        $bw.Write($row)
    }
    $bw.Flush()
    return ,$ms.ToArray()
}

function Get-PngEntry([System.Drawing.Bitmap]$bmp) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    return ,$ms.ToArray()
}

$entries = foreach ($s in $sizes) {
    $bmp = New-IconBitmap $s
    $data = if ($s -ge 256) { Get-PngEntry $bmp } else { Get-BmpEntry $bmp }
    $bmp.Dispose()
    [pscustomobject]@{ Size = $s; Data = $data }
}
$src.Dispose()

$fs = [System.IO.File]::Create($outPath)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([int16]0); $bw.Write([int16]1); $bw.Write([int16]$entries.Count)
$offset = 6 + 16 * $entries.Count
foreach ($e in $entries) {
    $dim = if ($e.Size -ge 256) { 0 } else { $e.Size }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([int16]1); $bw.Write([int16]32)
    $bw.Write([int]$e.Data.Length); $bw.Write([int]$offset)
    $offset += $e.Data.Length
}
foreach ($e in $entries) { $bw.Write($e.Data) }
$bw.Close()
Write-Host "wrote $outPath"
