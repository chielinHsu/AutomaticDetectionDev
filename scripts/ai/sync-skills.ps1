<#
	Mirror .agents/skills (source of truth, read by Codex) into .claude/skills
	(read by Claude Code). Neither tool reads the other's directory and symlinks
	are unreliable on Windows, so the mirror is a verified copy.

	  sync-skills.ps1           copy source -> target, then report
	  sync-skills.ps1 -Check    report only, change nothing (exit 1 on drift)

	Rules (manifest: .agents/harness/skill-sync.json):
	  - every skill in the source is mirrored, unless listed in "codexOnly"
	  - a skill that exists only in the target must be listed in "claudeOnly"
	  - mirrored skills are replaced wholesale; never edit them in the target
#>

[CmdletBinding()]
param(
	[switch]$Check,
	[string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot))
{
	$RepositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

$manifestPath = Join-Path $RepositoryRoot '.agents/harness/skill-sync.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf))
{
	Write-Host "FAIL manifest not found: $manifestPath"
	exit 1
}

$manifest = [IO.File]::ReadAllText($manifestPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
$sourceDirectory = Join-Path $RepositoryRoot $manifest.sourceDirectory
$targetDirectory = Join-Path $RepositoryRoot $manifest.targetDirectory
$claudeOnly = @($manifest.claudeOnly)
$codexOnly = @($manifest.codexOnly)
$ignoredDirectoryNames = @($manifest.ignoredDirectoryNames)
$ignoredExtensions = @($manifest.ignoredExtensions)

function Get-SkillNames
{
	param([string]$Directory)

	if (-not (Test-Path -LiteralPath $Directory -PathType Container))
	{
		return @()
	}

	return @(Get-ChildItem -LiteralPath $Directory -Directory -Force |
		Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'SKILL.md') -PathType Leaf } |
		ForEach-Object { $_.Name })
}

function Get-SkillFileMap
{
	param([string]$SkillDirectory)

	$map = @{}
	$root = (Resolve-Path -LiteralPath $SkillDirectory).Path.TrimEnd('\', '/')
	foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Force)
	{
		$relative = $file.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
		$segments = $relative.Split('/')
		$ignored = $false
		for ($i = 0; $i -lt $segments.Length - 1; $i++)
		{
			if ($ignoredDirectoryNames -contains $segments[$i])
			{
				$ignored = $true
			}
		}
		if ($ignoredExtensions -contains $file.Extension.ToLowerInvariant())
		{
			$ignored = $true
		}
		if (-not $ignored)
		{
			$map[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
		}
	}

	return $map
}

function Get-MapDifference
{
	param($SourceMap, $TargetMap)

	$differences = New-Object System.Collections.Generic.List[string]
	foreach ($key in $SourceMap.Keys)
	{
		if (-not $TargetMap.ContainsKey($key))
		{
			$differences.Add("missing $key")
		}
		elseif ($TargetMap[$key] -ne $SourceMap[$key])
		{
			$differences.Add("changed $key")
		}
	}
	foreach ($key in $TargetMap.Keys)
	{
		if (-not $SourceMap.ContainsKey($key))
		{
			$differences.Add("extra $key")
		}
	}

	return @($differences | Sort-Object)
}

function Copy-SkillMirror
{
	param([string]$Source, [string]$Target, [string]$TargetRoot)

	if (-not (Test-Path -LiteralPath $TargetRoot -PathType Container))
	{
		New-Item -ItemType Directory -Path $TargetRoot -Force | Out-Null
	}

	$resolvedRoot = (Resolve-Path -LiteralPath $TargetRoot).Path.TrimEnd('\', '/')
	if (Test-Path -LiteralPath $Target)
	{
		$resolvedTarget = (Resolve-Path -LiteralPath $Target).Path
		if (-not $resolvedTarget.StartsWith($resolvedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase))
		{
			throw "Refusing to replace a path outside the target root: $resolvedTarget"
		}
		Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
	}

	New-Item -ItemType Directory -Path $Target -Force | Out-Null
	Copy-Item -Path (Join-Path $Source '*') -Destination $Target -Recurse -Force
}

$problems = New-Object System.Collections.Generic.List[string]
$sourceNames = @(Get-SkillNames -Directory $sourceDirectory)
$targetNames = @(Get-SkillNames -Directory $targetDirectory)

foreach ($name in $codexOnly)
{
	if ($sourceNames -notcontains $name)
	{
		$problems.Add("codexOnly skill '$name' is not in $($manifest.sourceDirectory)")
	}
	if ($targetNames -contains $name)
	{
		$problems.Add("codexOnly skill '$name' must not exist in $($manifest.targetDirectory)")
	}
}

foreach ($name in $claudeOnly)
{
	if ($targetNames -notcontains $name)
	{
		$problems.Add("claudeOnly skill '$name' is not in $($manifest.targetDirectory)")
	}
	if ($sourceNames -contains $name)
	{
		$problems.Add("claudeOnly skill '$name' must not exist in $($manifest.sourceDirectory)")
	}
}

foreach ($name in $targetNames)
{
	if (($sourceNames -notcontains $name) -and ($claudeOnly -notcontains $name))
	{
		$problems.Add("ORPHAN '$name' exists only in $($manifest.targetDirectory); move it to $($manifest.sourceDirectory) or list it in claudeOnly")
	}
}

$mirrored = @($sourceNames | Where-Object { $codexOnly -notcontains $_ })
foreach ($name in $mirrored)
{
	$sourcePath = Join-Path $sourceDirectory $name
	$targetPath = Join-Path $targetDirectory $name
	$state = 'OK'
	$detail = @()

	if ($targetNames -notcontains $name)
	{
		$state = 'MISSING'
	}
	else
	{
		$detail = @(Get-MapDifference -SourceMap (Get-SkillFileMap -SkillDirectory $sourcePath) -TargetMap (Get-SkillFileMap -SkillDirectory $targetPath))
		if ($detail.Count -gt 0)
		{
			$state = 'DRIFT'
		}
	}

	if ($state -eq 'OK')
	{
		Write-Host "OK   $name"
		continue
	}

	if ($Check)
	{
		$problems.Add("$state '$name' $($detail -join '; ')")
	}
	else
	{
		Copy-SkillMirror -Source $sourcePath -Target $targetPath -TargetRoot $targetDirectory
		Write-Host "SYNC $name ($state)"
	}
}

if ($problems.Count -gt 0)
{
	foreach ($problem in $problems)
	{
		Write-Host "FAIL $problem"
	}
	exit 1
}

if ($Check)
{
	Write-Host "Skills in sync: $($mirrored.Count) mirrored, $($claudeOnly.Count) claudeOnly, $($codexOnly.Count) codexOnly."
}
exit 0
