param(
    [string]$BuildDirectory = "build",
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildPath = Join-Path $ProjectRoot $BuildDirectory
$TripletPath = Join-Path $BuildPath "vcpkg_installed\x64-mingw-dynamic"

if ($Configuration -eq "debug") {
    $VcpkgBin = Join-Path $TripletPath "debug\bin"
} else {
    $VcpkgBin = Join-Path $TripletPath "bin"
}

if (-not (Test-Path $BuildPath)) {
    throw "Build directory not found: $BuildPath"
}

if (-not (Test-Path $VcpkgBin)) {
    throw "vcpkg runtime directory not found: $VcpkgBin"
}

Copy-Item (Join-Path $VcpkgBin "*.dll") $BuildPath -Force

$Compiler = Get-Command "x86_64-w64-mingw32-g++.exe" -ErrorAction SilentlyContinue

if ($null -eq $Compiler) {
    $Compiler = Get-Command "g++.exe" -ErrorAction SilentlyContinue
}

if ($null -eq $Compiler) {
    Write-Warning "MinGW compiler was not found on PATH. vcpkg DLLs were copied, but compiler runtime DLLs were not."
    exit 0
}

$CompilerBin = Split-Path -Parent $Compiler.Source
$RuntimeDlls = @(
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)

foreach ($Dll in $RuntimeDlls) {
    $Source = Join-Path $CompilerBin $Dll

    if (Test-Path $Source) {
        Copy-Item $Source $BuildPath -Force
    }
}

Write-Host "Runtime DLLs copied to $BuildPath"
