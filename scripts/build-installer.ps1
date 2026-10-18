param(
    [string]$Configuration = "mingw-release",
    [switch]$SkipBuild,
    [switch]$FetchVCRedist
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\$Configuration"
$binDir = Join-Path $buildDir "bin"
$issPath = Join-Path $root "installer\ImRelighter.iss"
$distDir = Join-Path $root "dist"

if (-not $SkipBuild) {
    Push-Location $root
    try {
        cmake --build --preset $Configuration
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

$exePath = Join-Path $binDir "ImRelighter.exe"
if (-not (Test-Path $exePath)) {
    throw "Missing $exePath. Build the application first."
}

$cmakeLists = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match(
    $cmakeLists, 'project\s*\(\s*ImRelighter\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) {
    throw "Cannot read the project version from CMakeLists.txt."
}
$version = $versionMatch.Groups[1].Value

$isccCandidates = @(
    @(
        $env:ISCC,
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    ) | Where-Object { $_ -and (Test-Path $_) }
)

if ($isccCandidates.Count -eq 0) {
    throw "ISCC.exe not found. Install Inno Setup 6.5+ or set the ISCC environment variable."
}
$iscc = $isccCandidates[0]

$redistDir = Join-Path $root "installer\redist"
$redistPath = Join-Path $redistDir "vc_redist.x64.exe"
if ($FetchVCRedist -and -not (Test-Path $redistPath)) {
    New-Item -ItemType Directory -Force $redistDir | Out-Null
    Write-Host "Downloading the Microsoft Visual C++ redistributable..."
    Invoke-WebRequest -UseBasicParsing `
        -Uri "https://aka.ms/vs/17/release/vc_redist.x64.exe" `
        -OutFile $redistPath
}

$isccArgs = @(
    "/DMyAppVersion=$version",
    "/DMyBuildDir=$binDir"
)
if (Test-Path $redistPath) {
    $isccArgs += "/DIncludeVCRedist=1"
    Write-Host "Bundling the Microsoft Visual C++ redistributable."
}
else {
    Write-Warning "vc_redist.x64.exe is not bundled. Run with -FetchVCRedist to include it."
}

New-Item -ItemType Directory -Force $distDir | Out-Null
$outputName = "ImRelighter-$version-setup.exe"
$outputPath = Join-Path $distDir $outputName
Remove-Item $outputPath -Force -ErrorAction SilentlyContinue
Remove-Item "$outputPath.sha256" -Force -ErrorAction SilentlyContinue

& $iscc @isccArgs $issPath
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compilation failed with exit code $LASTEXITCODE."
}

$hash = (Get-FileHash $outputPath -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $outputName" | Set-Content "$outputPath.sha256" -Encoding ascii

$sizeMb = [math]::Round((Get-Item $outputPath).Length / 1MB, 2)
Write-Host "Installer: $outputPath"
Write-Host "Size:      $sizeMb MB"
Write-Host "SHA-256:   $hash"
