[CmdletBinding()]
param(
    [string]$Version = "2.1.1",
    [ValidateSet("Release")]
    [string]$Configuration = "Release",
    [string]$FfmpegSourceDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectDir = $PSScriptRoot
$buildDir = Join-Path $projectDir "build"
$releaseDir = Join-Path $buildDir $Configuration
$packageDir = Join-Path $buildDir "package"
$distDir = Join-Path $projectDir "dist"
$nativeCmake = Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"
$portableName = "KlanggeistLyricsStudio-$Version-portable"
$portableStage = Join-Path $packageDir $portableName
$portableZip = Join-Path $distDir "$portableName.zip"
$msiPath = Join-Path $distDir "KlanggeistLyricsStudio-$Version-x64.msi"
$expectedFfmpegCommit = "b397eba2f0d3d86daf1098d0f27daffccc74fea5"

function Reset-SafeDirectory([string]$Path, [string]$ExpectedParent) {
    $parent = [IO.Path]::GetFullPath($ExpectedParent).TrimEnd('\')
    $target = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    if (-not $target.StartsWith($parent + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsicheres Paketverzeichnis: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    New-Item -ItemType Directory -Path $target -Force | Out-Null
}

function Assert-LastExitCode([string]$Message) {
    if ($LASTEXITCODE -ne 0) { throw $Message }
}

function Get-Sha256([string]$Path) {
    $sha256 = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try {
        $bytes = $sha256.ComputeHash($stream)
        return ([BitConverter]::ToString($bytes)).Replace("-", "").ToLowerInvariant()
    } finally {
        $stream.Dispose()
        $sha256.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $nativeCmake)) {
    throw "CMake für Windows wurde nicht gefunden: $nativeCmake"
}

$cmakeText = Get-Content -LiteralPath (Join-Path $projectDir "CMakeLists.txt") -Raw
if ($cmakeText -notmatch "project\(KlanggeistLyricsStudio VERSION $([Regex]::Escape($Version)) ") {
    throw "Die gewünschte Release-Version $Version stimmt nicht mit CMakeLists.txt überein."
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

& $nativeCmake -S $projectDir -B $buildDir -G "Visual Studio 17 2022" -A x64
Assert-LastExitCode "CMake-Konfiguration fehlgeschlagen."

foreach ($target in @("KlanggeistLyricsStudio", "KlanggeistVideoSmoke", "KlanggeistExportControllerSmoke")) {
    & $nativeCmake --build $buildDir --config $Configuration --target $target --parallel
    Assert-LastExitCode "Build des Targets $target fehlgeschlagen."
}

$pythonLauncher = Get-Command py -ErrorAction SilentlyContinue
if ($null -ne $pythonLauncher) {
    & $pythonLauncher.Source -3.12 -m py_compile (Join-Path $projectDir "backend\whisper_worker.py")
    Assert-LastExitCode "Python-Syntaxprüfung fehlgeschlagen."
} else {
    $python = Get-Command python -ErrorAction Stop
    & $python.Source -m py_compile (Join-Path $projectDir "backend\whisper_worker.py")
    Assert-LastExitCode "Python-Syntaxprüfung fehlgeschlagen."
}

& (Join-Path $projectDir "BUILD_MSI.ps1") -Configuration $Configuration -Version $Version
Assert-LastExitCode "MSI-Erstellung fehlgeschlagen."

Reset-SafeDirectory -Path $portableStage -ExpectedParent $packageDir
foreach ($file in @(
    "KlanggeistLyricsStudio.exe", "INSTALL_BACKEND.bat", "requirements.txt",
    "README.md", "LICENSE", "THIRD_PARTY_NOTICES.md"
)) {
    $source = if ($file -eq "KlanggeistLyricsStudio.exe") { Join-Path $releaseDir $file } else { Join-Path $projectDir $file }
    Copy-Item -LiteralPath $source -Destination $portableStage
}
foreach ($directory in @("assets", "backend", "docs", "presets", "tools")) {
    Copy-Item -LiteralPath (Join-Path $projectDir $directory) -Destination $portableStage -Recurse
}

if (Test-Path -LiteralPath $portableZip) { Remove-Item -LiteralPath $portableZip -Force }
Compress-Archive -LiteralPath $portableStage -DestinationPath $portableZip -CompressionLevel Optimal

$releaseFiles = @($portableZip, $msiPath)

if (-not [string]::IsNullOrWhiteSpace($FfmpegSourceDir)) {
    $resolvedFfmpeg = (Resolve-Path -LiteralPath $FfmpegSourceDir).Path
    $head = (& git -C $resolvedFfmpeg rev-parse HEAD).Trim()
    Assert-LastExitCode "FFmpeg-Commit konnte nicht gelesen werden."
    if ($head -ne $expectedFfmpegCommit) {
        throw "FFmpeg steht auf $head, erwartet wird $expectedFfmpegCommit."
    }
    $changes = @(& git -C $resolvedFfmpeg status --porcelain --untracked-files=no)
    Assert-LastExitCode "FFmpeg-Arbeitsbaum konnte nicht geprüft werden."
    if ($changes.Count -ne 1 -or $changes[0].Trim() -ne "M libavfilter/vf_subtitles.c") {
        throw "Der FFmpeg-Arbeitsbaum enthält unerwartete Änderungen: $($changes -join ', ')"
    }

    $shortCommit = $expectedFfmpegCommit.Substring(0, 8)
    $sourceName = "Klanggeist-FFmpeg-8.0-git-$shortCommit-source"
    $sourceStage = Join-Path $packageDir $sourceName
    $sourceZip = Join-Path $distDir "$sourceName.zip"
    $sourceTar = Join-Path $packageDir "$sourceName.tar"
    Reset-SafeDirectory -Path $sourceStage -ExpectedParent $packageDir
    if (Test-Path -LiteralPath $sourceTar) { Remove-Item -LiteralPath $sourceTar -Force }

    & git -C $resolvedFfmpeg archive --format=tar --output=$sourceTar HEAD
    Assert-LastExitCode "FFmpeg-Basisquellcode konnte nicht archiviert werden."
    & tar -xf $sourceTar -C $sourceStage
    Assert-LastExitCode "FFmpeg-Basisquellcode konnte nicht entpackt werden."
    Remove-Item -LiteralPath $sourceTar -Force

    Copy-Item -LiteralPath (Join-Path $resolvedFfmpeg "libavfilter\vf_subtitles.c") -Destination (Join-Path $sourceStage "libavfilter\vf_subtitles.c") -Force
    Copy-Item -LiteralPath (Join-Path $projectDir "patches\ffmpeg-reuse-unchanged.patch") -Destination (Join-Path $sourceStage "KLANGGEIST-reuse-unchanged.patch")
    @"
Klanggeist FFmpeg source package

Upstream: https://github.com/FFmpeg/FFmpeg
Base commit: $expectedFfmpegCommit
Local change: libavfilter/vf_subtitles.c
Patch copy: KLANGGEIST-reuse-unchanged.patch
Build and license details: https://github.com/kruemmel-python/LyricsStudio/blob/v$Version/docs/FFMPEG.md
"@ | Set-Content -LiteralPath (Join-Path $sourceStage "KLANGGEIST_PATCH_INFO.txt") -Encoding UTF8

    if (Test-Path -LiteralPath $sourceZip) { Remove-Item -LiteralPath $sourceZip -Force }
    & tar -a -c -f $sourceZip -C $packageDir $sourceName
    Assert-LastExitCode "FFmpeg-Quellpaket konnte nicht komprimiert werden."
    $releaseFiles += $sourceZip
}

$checksumPath = Join-Path $distDir "SHA256SUMS.txt"
$checksumLines = foreach ($file in $releaseFiles) {
    if (-not (Test-Path -LiteralPath $file)) { throw "Release-Datei fehlt: $file" }
    $hash = Get-Sha256 -Path $file
    "$hash  $([IO.Path]::GetFileName($file))"
}
$checksumLines | Set-Content -LiteralPath $checksumPath -Encoding ASCII

Write-Host ""
Write-Host "Release $Version erfolgreich erstellt:"
foreach ($file in ($releaseFiles + $checksumPath)) {
    $item = Get-Item -LiteralPath $file
    Write-Host ("  {0} ({1:N1} MiB)" -f $item.FullName, ($item.Length / 1MB))
}
