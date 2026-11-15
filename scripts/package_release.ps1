[CmdletBinding()]
param(
    [string]$BinaryPath,
    [string]$SlintDir = $env:SLINT_DIR,
    [string]$OutputDirectory,
    [string]$Version = 'dev'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($BinaryPath)) {
    $candidates = @(Get-ChildItem -Path (Join-Path $projectRoot 'x64\Release') -Filter '*.exe' -File -ErrorAction SilentlyContinue)
    if ($candidates.Count -ne 1) { throw 'Pass -BinaryPath explicitly when the Release output does not contain exactly one executable.' }
    $BinaryPath = $candidates[0].FullName
}
$BinaryPath = (Resolve-Path -LiteralPath $BinaryPath).Path

if ([string]::IsNullOrWhiteSpace($SlintDir)) { $SlintDir = 'C:\Program Files\Slint-cpp 1.17.1' }
$slintDll = Join-Path $SlintDir 'lib\slint_cpp.dll'
if (-not (Test-Path -LiteralPath $slintDll)) { throw "Slint runtime DLL was not found: $slintDll" }

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = Join-Path $projectRoot "dist\XPEBot-$Version" }
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'dist'))
$outputPrefix = $outputRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
if (-not $OutputDirectory.StartsWith($outputPrefix, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'OutputDirectory must be inside the repository dist directory.' }

if (Test-Path -LiteralPath $OutputDirectory) { Remove-Item -LiteralPath $OutputDirectory -Recurse -Force }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null

Copy-Item -LiteralPath $BinaryPath -Destination (Join-Path $OutputDirectory 'XPEBot.exe')

# MinGW-built binaries need the MinGW runtime DLLs shipped alongside.
$binaryBytes = [System.IO.File]::ReadAllBytes($BinaryPath)
$importText = [System.Text.Encoding]::ASCII.GetString($binaryBytes)
if ($importText.Contains('libstdc++-6.dll')) {
    $mingwBin = $env:MINGW_BIN
    if ([string]::IsNullOrWhiteSpace($mingwBin)) {
        $gpp = Get-Command g++.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
        if ($gpp) { $mingwBin = Split-Path -Parent $gpp }
    }
    if ([string]::IsNullOrWhiteSpace($mingwBin) -or -not (Test-Path -LiteralPath $mingwBin)) {
        throw 'MinGW runtime DLLs are required but MINGW_BIN is not set and g++.exe was not found on PATH.'
    }
    foreach ($runtimeDll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $mingwBin $runtimeDll) -Destination $OutputDirectory
    }
}
# MSVC-built binaries link against the Visual C++ runtime; ship it app-local so the
# package also runs on a machine without the VC++ Redistributable installed.
if ($importText.Contains('MSVCP140.dll') -or $importText.Contains('VCRUNTIME140.dll')) {
    $msvcDlls = @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')
    $msvcSources = @()
    if (-not [string]::IsNullOrWhiteSpace($env:VCToolsRedistDir)) {
        $msvcSources += (Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT')
    }
    foreach ($vsRoot in @("$env:ProgramFiles\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio")) {
        if (-not (Test-Path -LiteralPath $vsRoot)) { continue }
        $msvcSources += @(Get-ChildItem -Path (Join-Path $vsRoot '*\*\VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT') -Directory -ErrorAction SilentlyContinue |
            Sort-Object -Property FullName -Descending | Select-Object -ExpandProperty FullName)
    }
    $msvcSources += (Join-Path $env:SystemRoot 'System32')
    foreach ($runtimeDll in $msvcDlls) {
        $sourceDll = $null
        foreach ($sourceDir in $msvcSources) {
            $candidate = Join-Path $sourceDir $runtimeDll
            if (Test-Path -LiteralPath $candidate) { $sourceDll = $candidate; break }
        }
        if ($sourceDll) {
            Copy-Item -LiteralPath $sourceDll -Destination $OutputDirectory
        } else {
            Write-Warning "Visual C++ runtime DLL not found: $runtimeDll (the package may require the VC++ Redistributable)."
        }
    }
    Write-Output 'Visual C++ runtime bundled (app-local deployment).'
}

Copy-Item -LiteralPath $slintDll -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE.txt') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination (Join-Path $OutputDirectory 'README.md')
New-Item -ItemType Directory -Path (Join-Path $OutputDirectory 'config') | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\config.json') -Destination (Join-Path $OutputDirectory 'config\config.json')
Copy-Item -LiteralPath (Join-Path $projectRoot 'data') -Destination (Join-Path $OutputDirectory 'data') -Recurse

$checksum = Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'XPEBot.exe') -Algorithm SHA256
"$($checksum.Hash)  XPEBot.exe" | Set-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -NoNewline

$archive = "${OutputDirectory}.zip"
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path $OutputDirectory -DestinationPath $archive -CompressionLevel Optimal
Write-Output "Package created: $archive"
