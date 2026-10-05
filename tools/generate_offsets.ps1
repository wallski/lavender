param(
    [string]$A2xFolder = "",
    [string]$OutputFile = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-Upward {
    param([string]$Start, [string[]]$Relatives)
    $dir = $Start
    for ($i = 0; $i -lt 8; $i++) {
        foreach ($rel in $Relatives) {
            $candidate = Join-Path $dir $rel
            if (Test-Path $candidate) { return $candidate }
        }
        $parent = Split-Path -Parent $dir
        if ([string]::IsNullOrEmpty($parent) -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return $null
}

if ([string]::IsNullOrEmpty($A2xFolder)) {
    $anchor = Find-Upward $scriptDir @("a2x\output\offsets.json", "..\a2x\output\offsets.json")
    if (-not $anchor) {
        Write-Host "[gen-offsets] ERROR: could not find a2x\output\offsets.json by walking up from $scriptDir" -ForegroundColor Red
        exit 1
    }
    $A2xFolder = Split-Path -Parent $anchor
}

if ([string]::IsNullOrEmpty($OutputFile)) {
    $configAnchor = Find-Upward $scriptDir @("config\OffsetConfig.h", "src\config\OffsetConfig.h")
    if (-not $configAnchor) {
        Write-Host "[gen-offsets] ERROR: could not find OffsetConfig.h by walking up from $scriptDir" -ForegroundColor Red
        exit 1
    }
    $configDir = Split-Path -Parent $configAnchor
    $OutputFile = Join-Path $configDir "GeneratedOffsets.h"
}

$jsonPath = Join-Path $A2xFolder "offsets.json"
$hppPath = Join-Path $A2xFolder "client_dll.hpp"

if (-not (Test-Path $jsonPath)) {
    Write-Host "[gen-offsets] ERROR: $jsonPath not found" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $hppPath)) {
    Write-Host "[gen-offsets] ERROR: $hppPath not found" -ForegroundColor Red
    exit 1
}

Write-Host "[gen-offsets] a2x    : $A2xFolder"
Write-Host "[gen-offsets] output : $OutputFile"

$json = Get-Content $jsonPath -Raw
$hpp = Get-Content $hppPath -Raw
$jsonData = $json | ConvertFrom-Json

function Get-NamespacedConstant {
    param([string]$Hpp, [string]$Namespace, [string]$Name)
    $nsPattern = 'namespace\s+' + [regex]::Escape($Namespace) + '\s*\{'
    $nsMatch = [regex]::Match($Hpp, $nsPattern)
    if (-not $nsMatch.Success) { return $null }

    $start = $nsMatch.Index + $nsMatch.Length
    $end = [Math]::Min($start + 16384, $Hpp.Length)
    $block = $Hpp.Substring($start, $end - $start)

    $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($Name) + '\s*=\s*(0x[0-9A-Fa-f]+)'
    $m = [regex]::Match($block, $pattern)
    if ($m.Success) { return $m.Groups[1].Value }
    return $null
}

$globalNames = @(
    "dwEntityList",
    "dwLocalPlayerController",
    "dwLocalPlayerPawn",
    "dwViewMatrix",
    "dwGlobalVars",
    "dwGameRules",
    "dwInputSystem",
    "dwCSGOInput",
    "dwViewAngles"
)

$fieldNames = @(
    "m_iHealth",
    "m_iTeamNum",
    "m_lifeState",
    "m_vecOrigin",
    "m_hPlayerPawn",
    "m_bDormant",
    "m_pGameSceneNode",
    "m_vecAbsOrigin",
    "m_angAbsRotation",
    "m_vecViewOffset",
    "m_bGunGameImmunity",
    "m_bPawnIsAlive",
    "m_fFlags",
    "m_vecAbsVelocity",
    "m_angEyeAngles",
    "m_pAimPunchServices",
    "m_pWeaponServices",
    "m_hActiveWeapon",
    "m_modelState",
    "m_pCollision",
    "m_vecMins",
    "m_vecMaxs",
    "m_sSanitizedPlayerName",
    "m_iClip1",
    "m_iMaxClip1",
    "m_szName",
    "m_nSubclassID",
    "m_flFlashBangTime",
    "m_flFlashScreenshotAlpha",
    "m_flFlashOverlayAlpha",
    "m_flFlashMaxAlpha",
    "m_flFlashDuration"
)

$specialFields = @{
    "m_hPlayerPawn" = @{ Namespace = "CCSPlayerController"; Name = "m_hPlayerPawn" }
    "m_bPawnIsAlive" = @{ Namespace = "CCSPlayerController"; Name = "m_bPawnIsAlive" }
    "m_sSanitizedPlayerName" = @{ Namespace = "CCSPlayerController"; Name = "m_sSanitizedPlayerName" }
}

$c4Fields = @(
    "m_bBombTicking",
    "m_nBombSite",
    "m_flC4Blow",
    "m_bHasExploded",
    "m_bBeingDefused",
    "m_flDefuseLength",
    "m_flDefuseCountDown",
    "m_bBombDefused"
)

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("#pragma once")
$lines.Add("#include <cstdint>")
$lines.Add("")
$lines.Add("namespace GeneratedOffsets {")
$lines.Add("")
$lines.Add("    namespace Globals {")

$missingGlobals = 0
foreach ($name in $globalNames) {
    $found = $null
    foreach ($module in $jsonData.PSObject.Properties) {
        $prop = $module.Value.PSObject.Properties | Where-Object { $_.Name -eq $name }
        if ($prop) {
            $found = [int64]$prop.Value
            break
        }
    }
    if ($null -eq $found) {
        $lines.Add("        constexpr uintptr_t $name = 0;")
        Write-Host "[gen-offsets] WARN: global '$name' not found" -ForegroundColor Yellow
        $missingGlobals++
    }
    else {
        $hex = "0x{0:X}" -f $found
        $lines.Add("        constexpr uintptr_t $name = $hex;")
    }
}

$lines.Add("    }")
$lines.Add("")
$lines.Add("    namespace Fields {")

$missingFields = 0
foreach ($name in $fieldNames) {
    $hexVal = $null

    if ($specialFields.ContainsKey($name)) {
        $spec = $specialFields[$name]
        $hexVal = Get-NamespacedConstant -Hpp $hpp -Namespace $spec.Namespace -Name $spec.Name
        if (-not $hexVal) {
            Write-Host "[gen-offsets] WARN: namespaced field '$($spec.Namespace)::$($spec.Name)' not found" -ForegroundColor Yellow
        }
    }
    else {
        $pattern = '(?<![A-Za-z0-9_])' + [regex]::Escape($name) + '\s*=\s*(0x[0-9A-Fa-f]+)'
        $match = [regex]::Match($hpp, $pattern)
        if ($match.Success) {
            $hexVal = $match.Groups[1].Value
        }
    }

    if ($hexVal) {
        $lines.Add("        constexpr uint32_t $name = $hexVal;")
    }
    else {
        $lines.Add("        constexpr uint32_t $name = 0;")
        Write-Host "[gen-offsets] WARN: field '$name' not found" -ForegroundColor Yellow
        $missingFields++
    }
}

$lines.Add("    }")
$lines.Add("")
$lines.Add("    namespace C4 {")

$missingC4 = 0
foreach ($name in $c4Fields) {
    $hexVal = Get-NamespacedConstant -Hpp $hpp -Namespace "C_PlantedC4" -Name $name
    if ($hexVal) {
        $lines.Add("        constexpr uint32_t $name = $hexVal;")
    }
    else {
        $lines.Add("        constexpr uint32_t $name = 0;")
        Write-Host "[gen-offsets] WARN: C4 field '$name' not found" -ForegroundColor Yellow
        $missingC4++
    }
}

$lines.Add("    }")
$lines.Add("")
$lines.Add("}")

$content = $lines -join "`r`n"
Set-Content -Path $OutputFile -Value $content -Encoding ASCII

Write-Host "[gen-offsets] Wrote $OutputFile (globals: $($globalNames.Count - $missingGlobals)/$($globalNames.Count), fields: $($fieldNames.Count - $missingFields)/$($fieldNames.Count), C4: $($c4Fields.Count - $missingC4)/$($c4Fields.Count))" -ForegroundColor Green

if ($missingGlobals -gt 0 -or $missingFields -gt 0) {
    exit 1
}