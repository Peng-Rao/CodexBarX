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

function Set-InstallerConfigValue {
    param(
        [Parameter(Mandatory=$true)]
        [System.Xml.XmlDocument]$Xml,

        [Parameter(Mandatory=$true)]
        [string]$Name,

        [Parameter(Mandatory=$true)]
        [string]$Value
    )

    $node = $Xml.Installer.SelectSingleNode($Name)
    if (-not $node) {
        $node = $Xml.CreateElement($Name)
        [void]$Xml.Installer.AppendChild($node)
    }

    $node.InnerText = $Value
}

function Save-Utf8Xml {
    param(
        [Parameter(Mandatory=$true)]
        [System.Xml.XmlDocument]$Xml,

        [Parameter(Mandatory=$true)]
        [string]$Path
    )

    $settings = New-Object System.Xml.XmlWriterSettings
    $settings.Encoding = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $settings.Indent = $true
    $settings.NewLineChars = "`r`n"

    $writer = [System.Xml.XmlWriter]::Create($Path, $settings)
    try {
        $Xml.Save($writer)
    } finally {
        $writer.Close()
    }
}

Write-Host "=== CodexBarX Installer Builder ===" -ForegroundColor Cyan
Write-Host "Version: $Version"
Write-Host "Build Type: $BuildType"
Write-Host "Project Root: $projectRoot"

# Find Qt Installer Framework
if ([string]::IsNullOrEmpty($QtIFWPath)) {
    $qtToolsRoots = @()
    if (-not [string]::IsNullOrEmpty($env:Qt6_DIR)) {
        $qtToolsRoots += Join-Path $env:Qt6_DIR "..\..\Tools\QtInstallerFramework"
    }

    foreach ($root in $qtToolsRoots) {
        if (Test-Path $root) {
            $ifwBin = Get-ChildItem -Path $root -Filter "binarycreator.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($ifwBin) {
                $QtIFWPath = Split-Path -Parent (Split-Path -Parent $ifwBin.FullName)
                Write-Host "Found Qt IFW at: $QtIFWPath" -ForegroundColor Green
                break
            }
        }
    }

    if ([string]::IsNullOrEmpty($QtIFWPath)) {
        $possiblePaths = @(
            "D:\Qt\Tools\QtInstallerFramework\4.11\bin",
            "D:\Qt\Tools\QtInstallerFramework\4.10\bin",
            "D:\Qt\Tools\QtInstallerFramework\4.7\bin",
            "D:\Qt\Tools\QtInstallerFramework\4.6\bin",
            "D:\Qt\Tools\QtInstallerFramework\4.5\bin",
            "C:\Qt\Tools\QtInstallerFramework\4.11\bin",
            "C:\Qt\Tools\QtInstallerFramework\4.10\bin",
            "C:\Qt\Tools\QtInstallerFramework\4.7\bin",
            "C:\Qt\Tools\QtInstallerFramework\4.6\bin",
            "C:\Qt\Tools\QtInstallerFramework\4.5\bin",
            "${env:ProgramFiles}\Qt\Tools\QtInstallerFramework\4.11\bin",
            "${env:ProgramFiles}\Qt\Tools\QtInstallerFramework\4.10\bin",
            "${env:ProgramFiles}\Qt\Tools\QtInstallerFramework\4.7\bin",
            "${env:ProgramFiles(x86)}\Qt\Tools\QtInstallerFramework\4.11\bin",
            "${env:ProgramFiles(x86)}\Qt\Tools\QtInstallerFramework\4.10\bin",
            "${env:ProgramFiles(x86)}\Qt\Tools\QtInstallerFramework\4.7\bin"
        )

        foreach ($path in $possiblePaths) {
            if (Test-Path $path) {
                $QtIFWPath = Split-Path -Parent $path
                Write-Host "Found Qt IFW at: $QtIFWPath" -ForegroundColor Green
                break
            }
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
$dataDir = Join-Path $installerDir "packages\com.codexbarx.app\data"
$metaDir = Join-Path $installerDir "packages\com.codexbarx.app\meta"

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

# Keep Qt IFW visual resources as file references relative to config.xml.
$requiredInstallerResources = @(
    "stylesheet.qss",
    "controller.qs",
    "watermark.png"
)

foreach ($resource in $requiredInstallerResources) {
    $resourcePath = Join-Path $installerDir "config\$resource"
    if (-not (Test-Path $resourcePath -PathType Leaf)) {
        Write-Error "Installer resource not found: $resourcePath"
        exit 1
    }
}

[xml]$configContent = Get-Content $configXml -Raw
Set-InstallerConfigValue -Xml $configContent -Name "Version" -Value $Version
Set-InstallerConfigValue -Xml $configContent -Name "WizardStyle" -Value "Classic"
Set-InstallerConfigValue -Xml $configContent -Name "StyleSheet" -Value "stylesheet.qss"
Set-InstallerConfigValue -Xml $configContent -Name "TitleColor" -Value "#ffffff"
Set-InstallerConfigValue -Xml $configContent -Name "WizardShowPageList" -Value "false"
Set-InstallerConfigValue -Xml $configContent -Name "Watermark" -Value "watermark.png"
Set-InstallerConfigValue -Xml $configContent -Name "ControlScript" -Value "controller.qs"
Save-Utf8Xml -Xml $configContent -Path $configXml

$configValidationScript = Join-Path $scriptDir "Test-InstallerConfig.ps1"
if (-not (Test-Path $configValidationScript -PathType Leaf)) {
    Write-Error "Installer config validation script not found: $configValidationScript"
    exit 1
}

& $configValidationScript -ConfigPath $configXml

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
