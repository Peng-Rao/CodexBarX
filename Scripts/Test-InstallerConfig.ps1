[CmdletBinding()]
param(
    [string]$ConfigPath
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    $ConfigPath = Join-Path $projectRoot "installer/config/config.xml"
}

function Assert-InstallerValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Actual,

        [Parameter(Mandatory = $true)]
        [string]$Expected
    )

    if ($Actual -ne $Expected) {
        throw "Installer.$Name expected '$Expected' but found '$Actual'."
    }
}

function Get-InstallerValue {
    param(
        [Parameter(Mandatory = $true)]
        [xml]$Xml,

        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $value = [string]$Xml.Installer.$Name
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Missing required Installer.$Name node."
    }

    return $value.Trim()
}

$resolvedConfigPath = Resolve-Path -LiteralPath $ConfigPath
$configDir = Split-Path -Parent $resolvedConfigPath
[xml]$config = Get-Content -LiteralPath $resolvedConfigPath -Raw

$expectedValues = [ordered]@{
    Title                            = "CodexBarX"
    StyleSheet                       = "stylesheet.qss"
    ControlScript                    = "controller.qs"
    Watermark                        = "watermark.png"
    WizardShowPageList               = "false"
    WizardStyle                      = "Classic"
    TitleColor                       = "#ffffff"
}

foreach ($entry in $expectedValues.GetEnumerator()) {
    $actual = Get-InstallerValue -Xml $config -Name $entry.Key
    Assert-InstallerValue -Name $entry.Key -Actual $actual -Expected $entry.Value
}

foreach ($resourceNode in @("StyleSheet", "ControlScript", "Watermark")) {
    $relativePath = Get-InstallerValue -Xml $config -Name $resourceNode
    $resourcePath = Join-Path $configDir $relativePath

    if (-not (Test-Path -LiteralPath $resourcePath -PathType Leaf)) {
        throw "Installer.$resourceNode references missing file '$relativePath'."
    }
}

$unsupportedGlobalNodes = @(
    "DisableLicenseAutoAcceptCheckBox"
)

foreach ($nodeName in $unsupportedGlobalNodes) {
    if ($config.Installer.SelectSingleNode($nodeName)) {
        throw "Installer.$nodeName is not supported by Qt IFW 4.11 config.xml."
    }
}

$controlScriptPath = Join-Path $configDir (Get-InstallerValue -Xml $config -Name "ControlScript")
$controlScriptContent = Get-Content -LiteralPath $controlScriptPath -Raw
if ($controlScriptContent -match "setAutoAcceptLicenses") {
    throw "ControlScript must not call setAutoAcceptLicenses; license acceptance must remain user-controlled."
}

Write-Host "Installer config validation passed."
