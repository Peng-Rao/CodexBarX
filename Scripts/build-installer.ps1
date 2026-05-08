param(
    [Parameter(Mandatory=$false)]
    [string]$Version = "0.1.0",
    
    [Parameter(Mandatory=$false)]
    [string]$BuildType = "Release",
    
    [Parameter(Mandatory=$false)]
    [string]$QtIFWPath = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$installerDir = Join-Path $projectRoot "installer"
$buildDir = Join-Path $projectRoot "build"
$releaseDir = Join-Path $buildDir $BuildType

Write-Host "=== CodexBarX Installer Builder ===" -ForegroundColor Cyan
Write-Host "Version: $Version"
Write-Host "Build Type: $BuildType"
Write-Host "Project Root: $projectRoot"

# Find Qt Installer Framework
if ([string]::IsNullOrEmpty($QtIFWPath)) {
    $possiblePaths = @(
        "C:\Qt\Tools\QtInstallerFramework\4.7\bin",
        "C:\Qt\Tools\QtInstallerFramework\4.6\bin",
        "C:\Qt\Tools\QtInstallerFramework\4.5\bin",
        "${env:ProgramFiles}\Qt\Tools\QtInstallerFramework\4.7\bin",
        "${env:ProgramFiles(x86)}\Qt\Tools\QtInstallerFramework\4.7\bin"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $QtIFWPath = Split-Path -Parent $path
            Write-Host "Found Qt IFW at: $QtIFWPath" -ForegroundColor Green
            break
        }
    }
    
    if ([string]::IsNullOrEmpty($QtIFWPath)) {
        Write-Error "Qt Installer Framework not found. Please install it or specify -QtIFWPath parameter."
        Write-Host "Download from: https://download.qt.io/official_releases/qt-installer-framework/"
        exit 1
    }
}

$binDir = Join-Path $QtIFWPath "bin"
if (-not (Test-Path $binDir)) {
    Write-Error "Qt IFW bin directory not found: $binDir"
    exit 1
}

$env:PATH = "$binDir;$env:PATH"

# Check if build exists
if (-not (Test-Path $releaseDir)) {
    Write-Error "Build directory not found: $releaseDir"
    Write-Host "Please run build first: cmake --build build --config $BuildType"
    exit 1
}

$exePath = Join-Path $releaseDir "CodexBarX.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "CodexBarX.exe not found at: $exePath"
    exit 1
}

# Prepare data directory
Write-Host "`n=== Preparing installer data ===" -ForegroundColor Cyan
$dataDir = Join-Path $installerDir "packages\com.wincodexbar.app\data"
$metaDir = Join-Path $installerDir "packages\com.wincodexbar.app\meta"

if (Test-Path $dataDir) {
    Remove-Item -Recurse -Force $dataDir
}
New-Item -ItemType Directory -Path $dataDir -Force | Out-Null

# Copy release files
Write-Host "Copying build artifacts..."
Copy-Item -Path "$releaseDir\*" -Destination $dataDir -Recurse -Force

# Copy translations
$translationsSrc = Join-Path $buildDir "translations"
if (Test-Path $translationsSrc) {
    $translationsDest = Join-Path $dataDir "translations"
    New-Item -ItemType Directory -Path $translationsDest -Force | Out-Null
    Copy-Item -Path "$translationsSrc\*.qm" -Destination $translationsDest -Force
    Write-Host "Copied translation files"
}

# Update version in config files
Write-Host "`n=== Updating version information ===" -ForegroundColor Cyan

$configXml = Join-Path $installerDir "config\config.xml"
$packageXml = Join-Path $metaDir "package.xml"
$releaseDate = Get-Date -Format "yyyy-MM-dd"

# Update config.xml
$configContent = Get-Content $configXml -Raw
$configContent = $configContent -replace '<Version>[^<]*</Version>', "<Version>$Version</Version>"
Set-Content $configXml $configContent -NoNewline

# Update package.xml
$packageContent = Get-Content $packageXml -Raw
$packageContent = $packageContent -replace '<Version>[^<]*</Version>', "<Version>$Version</Version>"
$packageContent = $packageContent -replace '<ReleaseDate>[^<]*</ReleaseDate>', "<ReleaseDate>$releaseDate</ReleaseDate>"
Set-Content $packageXml $packageContent -NoNewline

Write-Host "Version: $Version"
Write-Host "Release Date: $releaseDate"

# Create offline installer
Write-Host "`n=== Building offline installer ===" -ForegroundColor Cyan
$outputName = "CodexBarX-$Version-Installer.exe"
$outputPath = Join-Path $projectRoot $outputName

if (Test-Path $outputPath) {
    Remove-Item $outputPath -Force
}

Push-Location $installerDir

try {
    & binarycreator.exe -c "config\config.xml" -p "packages" -v $outputPath
    if ($LASTEXITCODE -ne 0) {
        throw "binarycreator failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

if (Test-Path $outputPath) {
    $sizeMB = [math]::Round((Get-Item $outputPath).Length / 1MB, 2)
    Write-Host "`n=== Success! ===" -ForegroundColor Green
    Write-Host "Installer created: $outputPath" -ForegroundColor Green
    Write-Host "Size: $sizeMB MB" -ForegroundColor Green
} else {
    Write-Error "Failed to create installer"
    exit 1
}
