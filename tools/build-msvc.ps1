param(
    [string]$BuildDir = "build-msvc",
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [ValidateSet("2026", "2022")]
    [string]$VisualStudio = "2026"
)

# Optional Windows-native build. Install VS 2026 Community's Desktop
# development with C++ workload (including its Windows SDK), and CMake 4.2+.
# VS 2022 remains available with -VisualStudio 2022.
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (![IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path (Get-Location) $BuildDir
}
$buildRoot = [IO.Path]::GetFullPath($BuildDir)
$dependencies = Join-Path $buildRoot "_deps"
$version = "3.2.10"
$archiveName = "SDL3-devel-$version-VC.zip"
$archive = Join-Path $dependencies $archiveName
$expectedHash = "afcfafc7e389e048c3693e11fe645ea84392c35673e4f201fe21ac513719935d"
$sdlRoot = Join-Path $dependencies "SDL3-$version"
$package = Join-Path $buildRoot "package"

function Invoke-CMake {
    & cmake @args
    if ($LASTEXITCODE -ne 0) { throw "CMake failed with exit code $LASTEXITCODE" }
}

Get-Command cmake -ErrorAction Stop | Out-Null
$generator = if ($VisualStudio -eq "2026") { "Visual Studio 18 2026" } else { "Visual Studio 17 2022" }
$minimumCMake = if ($VisualStudio -eq "2026") { [version]"4.2" } else { [version]"3.21" }
$cmakeVersionOutput = & cmake --version
if ($LASTEXITCODE -ne 0) { throw "Could not read the CMake version" }
$versionMatch = [regex]::Match(($cmakeVersionOutput -join "`n"), 'cmake version (\d+\.\d+\.\d+)')
if (!$versionMatch.Success -or [version]$versionMatch.Groups[1].Value -lt $minimumCMake) {
    throw "Visual Studio $VisualStudio requires CMake $minimumCMake or newer on PATH."
}
# A build folder cannot switch between Visual Studio generators (or Ninja).
$cache = Join-Path $buildRoot "CMakeCache.txt"
if (Test-Path $cache) {
    $cachedGenerator = Get-Content $cache | Where-Object { $_ -like "CMAKE_GENERATOR:INTERNAL=*" }
    if ($cachedGenerator -and $cachedGenerator -ne "CMAKE_GENERATOR:INTERNAL=$generator") {
        throw "This build folder uses another generator. Choose a fresh folder with -BuildDir."
    }
}
New-Item -ItemType Directory -Force $dependencies | Out-Null
if (!(Test-Path $archive)) {
    $temporaryArchive = "$archive.part"
    Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/libsdl-org/SDL/releases/download/release-$version/$archiveName" -OutFile $temporaryArchive
    Move-Item -Force $temporaryArchive $archive
}
if ((Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw "SDL archive SHA-256 mismatch: $archive"
}
if (!(Test-Path $sdlRoot)) {
    Expand-Archive -Path $archive -DestinationPath $dependencies
}

Invoke-CMake -S $projectRoot -B $buildRoot -G $generator -A x64 `
    -DRECIV_MSVC=ON "-DSDL3_DIR=$sdlRoot/cmake"
Invoke-CMake --build $buildRoot --config $Configuration --parallel
Invoke-CMake --install $buildRoot --config $Configuration --prefix $package
Copy-Item (Join-Path $sdlRoot "LICENSE.txt") (Join-Path $package "licenses/SDL3.txt")
$zip = Join-Path $buildRoot "reciv-windows-msvc-x86_64.zip"
Compress-Archive -Force -Path (Join-Path $package "*") -DestinationPath $zip
Write-Host "Windows MSVC build: $buildRoot/$Configuration/reciv.exe"
Write-Host "Portable folder: $package"
Write-Host "Archive: $zip"
