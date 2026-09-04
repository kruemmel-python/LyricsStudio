[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Version = "2.1.1"
)

$ErrorActionPreference = "Stop"
$projectDir = $PSScriptRoot
$nativeCmake = Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"
$wix = Join-Path $env:USERPROFILE ".dotnet\tools\wix.exe"

if (-not (Test-Path -LiteralPath $nativeCmake)) {
    throw "CMake für Windows wurde nicht gefunden: $nativeCmake"
}
if (-not (Test-Path -LiteralPath $wix)) {
    throw "WiX Toolset wurde nicht gefunden: $wix"
}

$buildDir = Join-Path $projectDir "build"
$releaseDir = Join-Path $buildDir $Configuration
$distDir = Join-Path $projectDir "dist"
$intermediateDir = Join-Path $buildDir "installer"
$msiPath = Join-Path $distDir "KlanggeistLyricsStudio-$Version-x64.msi"

& $nativeCmake -S $projectDir -B $buildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake-Konfiguration fehlgeschlagen." }

& $nativeCmake --build $buildDir --config $Configuration --target KlanggeistLyricsStudio --parallel
if ($LASTEXITCODE -ne 0) { throw "Programm-Build fehlgeschlagen." }

# Dokumentation und Laufzeitdateien können sich ändern, ohne dass der Linker die
# EXE neu erzeugt. Vor jedem MSI-Build deshalb den Paketinhalt explizit auffrischen.
foreach ($file in @("requirements.txt", "INSTALL_BACKEND.bat", "README.md", "LICENSE", "THIRD_PARTY_NOTICES.md")) {
    Copy-Item -LiteralPath (Join-Path $projectDir $file) -Destination $releaseDir -Force
}
foreach ($directory in @("assets", "backend", "docs", "presets", "tools")) {
    Copy-Item -LiteralPath (Join-Path $projectDir $directory) -Destination $releaseDir -Recurse -Force
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
New-Item -ItemType Directory -Path $intermediateDir -Force | Out-Null

& $wix build `
    -arch x64 `
    -d "ProjectDir=$projectDir" `
    -d "ReleaseDir=$releaseDir" `
    -d "ProductVersion=$Version" `
    -intermediatefolder $intermediateDir `
    -pdbtype full `
    -out $msiPath `
    (Join-Path $projectDir "installer\Product.wxs")
if ($LASTEXITCODE -ne 0) { throw "MSI-Build fehlgeschlagen." }

& $wix msi validate -sice ICE91 -pdb ([IO.Path]::ChangeExtension($msiPath, ".wixpdb")) $msiPath
if ($LASTEXITCODE -ne 0) { throw "MSI-Validierung fehlgeschlagen." }

Write-Host "MSI erstellt: $msiPath"
