<#
Builds NF De-Esser on THIS Windows PC (VST3 + PACE-signed AAX) and packs the installer with Inno Setup.
Nothing is uploaded anywhere. Output: Installer\NF De-Esser X.Y.Z Setup.exe

Run from the repo root in PowerShell:   .\Installer\Windows\build_windows_installer.ps1

Prerequisites: Visual Studio 2022 (C++), CMake, Inno Setup 6, and for AAX: the AAX SDK + PACE wraptool.

Environment variables (all optional):
  AAX_SDK_PATH     AAX SDK folder (default: %USERPROFILE%\Documents\AAX_SDK)
  WRAPTOOL         path to PACE wraptool.exe (default: found on PATH)
  WRAP_ACCOUNT     PACE/iLok account used to sign (asked if missing)
  WRAP_PASSWORD    wraptool password (if not set, wraptool asks for it)
  WRAP_GUID        wrap GUID (default: "NF De-Esser - Signing Only")
  EXTRA_WRAP_ARGS  extra `wraptool sign` options your PACE setup needs (e.g. Windows code-signing options)
  SKIP_AAX=1       build an installer WITHOUT AAX (VST3 only)
The version is read from CMakeLists.txt, so a version bump needs no edit here.
#>
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cmakeFile = Join-Path $root 'CMakeLists.txt'
$m = [regex]::Match((Get-Content $cmakeFile -Raw), 'project\(NFDeEsser VERSION (\d+\.\d+\.\d+)')
if (-not $m.Success) { throw "Could not read the version from $cmakeFile" }
$version = $m.Groups[1].Value
$product = 'NF De-Esser'
$withAax = ($env:SKIP_AAX -ne '1')
$aaxSdk = if ($env:AAX_SDK_PATH) { $env:AAX_SDK_PATH } else { Join-Path $env:USERPROFILE 'Documents\AAX_SDK' }
$wrapGuid = if ($env:WRAP_GUID) { $env:WRAP_GUID } else { 'A3705280-BD40-11F1-B096-005056920FF7' }
$buildDir = Join-Path $root 'build-deesser'
$payload = Join-Path $root 'Installer\Windows\payload'

# ---- AAX prerequisites: fail early, before the long build
if ($withAax) {
  if (-not (Test-Path $aaxSdk)) { throw "AAX SDK not found at $aaxSdk (set AAX_SDK_PATH, or SKIP_AAX=1 for a VST3-only installer)" }
  $wraptool = $env:WRAPTOOL
  if (-not $wraptool) { $cmd = Get-Command wraptool -ErrorAction SilentlyContinue; if ($cmd) { $wraptool = $cmd.Source } }
  if (-not $wraptool -or -not (Test-Path $wraptool)) { throw "PACE wraptool not found (set WRAPTOOL=C:\path\wraptool.exe, or SKIP_AAX=1)" }
  $account = $env:WRAP_ACCOUNT
  if (-not $account) { $account = Read-Host 'PACE/iLok account for signing' }
}

$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
if (-not (Test-Path $iscc)) { throw "Inno Setup 6 not found at $iscc" }

Write-Host "==> Building $product $version"
$cfg = @('-S', $root, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64')
$targets = @('NFDeEsser_VST3')
if ($withAax) { $cfg += @('-DNFDeEsser_ENABLE_AAX=ON', "-DNFDeEsser_AAX_SDK_PATH=$aaxSdk"); $targets += 'NFDeEsser_AAX' }
else { $cfg += '-DNFDeEsser_ENABLE_AAX=OFF' }
cmake @cfg
if ($LASTEXITCODE) { throw 'cmake configure failed' }
cmake --build $buildDir --config Release --target @targets --parallel
if ($LASTEXITCODE) { throw 'build failed' }

Write-Host '==> Staging'
if (Test-Path $payload) { Remove-Item -Recurse -Force $payload }
New-Item -ItemType Directory -Force -Path $payload | Out-Null
$vst3 = Get-ChildItem -Path $buildDir -Recurse -Filter "$product.vst3" -Directory | Select-Object -First 1
if (-not $vst3) { throw 'VST3 bundle not found' }
Copy-Item -Recurse -Force $vst3.FullName (Join-Path $payload "$product.vst3")

if ($withAax) {
  $aax = Get-ChildItem -Path $buildDir -Recurse -Filter "$product.aaxplugin" -Directory | Select-Object -First 1
  if (-not $aax) { throw 'AAX bundle not found' }
  Write-Host "==> Signing AAX with PACE wraptool (wrap $wrapGuid)"
  $out = Join-Path $payload "$product.aaxplugin"
  $wrapArgs = @('sign', '--verbose', '--account', $account, '--wcguid', $wrapGuid, '--in', $aax.FullName, '--out', $out)
  if ($env:WRAP_PASSWORD) { $wrapArgs += @('--password', $env:WRAP_PASSWORD) }
  if ($env:EXTRA_WRAP_ARGS) { $wrapArgs += $env:EXTRA_WRAP_ARGS.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries) }
  & $wraptool @wrapArgs
  if ($LASTEXITCODE -or -not (Test-Path $out)) { throw 'wraptool did not produce the signed AAX' }
}

Write-Host '==> Creating installer'
$isccArgs = @("/DMyAppVersion=$version")
if ($withAax) { $isccArgs += '/DWITH_AAX' }
& $iscc @isccArgs (Join-Path $root 'Installer\Windows\NFDeEsser.iss')
if ($LASTEXITCODE) { throw 'Inno Setup failed' }
Write-Host "Done: Installer\$product $version Setup.exe" -ForegroundColor Green
