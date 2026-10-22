<#
	Apply the shared Codex / Claude Code structure to a folder.

	  New-AiProject.ps1 -Path D:\Work\MyApp
	  New-AiProject.ps1 -Path D:\Work\MyApp -Name "My App"
	  New-AiProject.ps1 -Path D:\Work\MyApp -DryRun       show what would happen
	  New-AiProject.ps1 -Path D:\Work\MyApp -Refresh      also update template-managed files

	Files are never overwritten, with one exception: -Refresh replaces the
	template-managed files (universal workflow, TEMPLATE_VERSION, ai-handoff skill,
	check-ai-structure.ps1, sync-skills.ps1, tools/ste-lint.mjs, and the _template
	files under docs/plan, docs/adr, docs/steps).
	Everything else is owned by the project once created (including the model
	routing files and scripts/ai/check-model-routing.mjs, which projects tune).
	Run it on an existing
	project and it only adds what is missing; existing AGENTS.md / CLAUDE.md are
	left alone and reported so you can merge them by hand.
	The script does not run git init and does not commit anything.
#>

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Path,
	[string]$Name,
	[switch]$Refresh,
	[switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$templateRoot = Join-Path $PSScriptRoot 'template'
if (-not (Test-Path -LiteralPath $templateRoot -PathType Container))
{
	throw "Template folder not found next to this script: $templateRoot"
}

# Files the template keeps up to date. Everything else is project-owned.
$managedPatterns = @(
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/TEMPLATE_VERSION',
	'.agents/skills/ai-handoff/*',
	'scripts/ai/check-ai-structure.ps1',
	'scripts/ai/sync-skills.ps1',
	'tools/ste-lint.mjs',
	'docs/steps/_template/*',
	'docs/plan/_template.md',
	'docs/adr/_template.md'
)
# Existing entry files signal an existing AI setup that needs a manual merge.
$entryFiles = @('AGENTS.md', 'CLAUDE.md')

$gitignoreLines = @(
	'# AI agent local state',
	'.claude/settings.local.json',
	'.claude/worktrees/',
	'CLAUDE.local.md',
	'perf/results/'
)

if (-not (Test-Path -LiteralPath $Path))
{
	if ($DryRun)
	{
		Write-Host "DRYRUN create folder $Path"
		$targetRoot = [IO.Path]::GetFullPath($Path)
	}
	else
	{
		New-Item -ItemType Directory -Path $Path -Force | Out-Null
		$targetRoot = (Resolve-Path -LiteralPath $Path).Path
	}
}
else
{
	$targetRoot = (Resolve-Path -LiteralPath $Path).Path
}
$targetRoot = $targetRoot.TrimEnd('\', '/')

if ([string]::IsNullOrWhiteSpace($Name))
{
	$Name = Split-Path -Leaf $targetRoot
}
$today = Get-Date -Format 'yyyy-MM-dd'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
# .ps1 files that contain Chinese need a BOM, or Windows PowerShell 5.1 parses them with the ANSI code page.
$utf8Bom = New-Object System.Text.UTF8Encoding($true)

function Test-Managed
{
	param([string]$RelativePath)

	foreach ($pattern in $managedPatterns)
	{
		if ($RelativePath -like $pattern)
		{
			return $true
		}
	}
	return $false
}

$counts = @{ COPY = 0; UPDATE = 0; SKIP = 0 }
$skippedEntries = @()
$templateFiles = Get-ChildItem -LiteralPath $templateRoot -Recurse -File -Force | Sort-Object FullName

foreach ($file in $templateFiles)
{
	$relative = $file.FullName.Substring($templateRoot.Length).TrimStart('\', '/').Replace('\', '/')
	$destination = Join-Path $targetRoot $relative
	$exists = Test-Path -LiteralPath $destination -PathType Leaf
	$managed = Test-Managed -RelativePath $relative

	if ($exists -and -not ($Refresh -and $managed))
	{
		Write-Host "SKIP   $relative (exists)"
		$counts.SKIP++
		if ($entryFiles -contains $relative)
		{
			$skippedEntries += $relative
		}
		continue
	}

	$action = 'COPY'
	if ($exists)
	{
		$action = 'UPDATE'
	}
	Write-Host "$($action.PadRight(6)) $relative"
	$counts[$action]++

	if ($DryRun)
	{
		continue
	}

	$directory = Split-Path -Parent $destination
	if (-not (Test-Path -LiteralPath $directory -PathType Container))
	{
		New-Item -ItemType Directory -Path $directory -Force | Out-Null
	}
	$text = [IO.File]::ReadAllText($file.FullName, [Text.Encoding]::UTF8)
	$text = $text.Replace('{{PROJECT_NAME}}', $Name).Replace('{{DATE}}', $today)
	$encoding = $utf8NoBom
	if ($destination -like '*.ps1')
	{
		$encoding = $utf8Bom
	}
	[IO.File]::WriteAllText($destination, $text, $encoding)
}

# .gitignore: append only the lines that are missing.
$gitignorePath = Join-Path $targetRoot '.gitignore'
$existingIgnore = @()
if (Test-Path -LiteralPath $gitignorePath -PathType Leaf)
{
	$existingIgnore = @([IO.File]::ReadAllLines($gitignorePath) | ForEach-Object { $_.Trim() })
}
$missingIgnore = @($gitignoreLines | Where-Object { ($_ -notlike '#*') -and ($existingIgnore -notcontains $_) })
if ($missingIgnore.Count -gt 0)
{
	Write-Host "GITIGNORE add: $($missingIgnore -join ', ')"
	if (-not $DryRun)
	{
		$existingText = ''
		if (Test-Path -LiteralPath $gitignorePath -PathType Leaf)
		{
			$existingText = [IO.File]::ReadAllText($gitignorePath)
		}
		# Follow the file's own line endings; a repo that normalizes to LF must not get CRLF lines.
		$eol = [Environment]::NewLine
		if ($existingText.Contains("`r`n"))
		{
			$eol = "`r`n"
		}
		elseif ($existingText.Contains("`n"))
		{
			$eol = "`n"
		}
		$block = @()
		if ($existingIgnore.Count -gt 0)
		{
			$block += ''
		}
		$block += $gitignoreLines[0]
		$block += $missingIgnore
		if (($existingText.Length -gt 0) -and (-not $existingText.EndsWith("`n")))
		{
			$existingText += $eol
		}
		[IO.File]::WriteAllText($gitignorePath, $existingText + (($block -join $eol) + $eol), $utf8NoBom)
	}
}

Write-Host ''
Write-Host "Done: $($counts.COPY) copied, $($counts.UPDATE) updated, $($counts.SKIP) skipped. Target: $targetRoot"

if ($DryRun)
{
	Write-Host 'Dry run: nothing was written.'
	exit 0
}

# Mirror skills into .claude/skills, then validate the result.
$syncScript = Join-Path $targetRoot 'scripts/ai/sync-skills.ps1'
$checkScript = Join-Path $targetRoot 'scripts/ai/check-ai-structure.ps1'
$exitCode = 0
if (Test-Path -LiteralPath $syncScript -PathType Leaf)
{
	Write-Host ''
	& $syncScript -RepositoryRoot $targetRoot
	if ($LASTEXITCODE -ne 0) { $exitCode = 1 }
}
if (Test-Path -LiteralPath $checkScript -PathType Leaf)
{
	Write-Host ''
	& $checkScript -RepositoryRoot $targetRoot
	if ($LASTEXITCODE -ne 0) { $exitCode = 1 }
}

Write-Host ''
if ($skippedEntries.Count -gt 0)
{
	Write-Host "NOTE: existing entry file(s) were not modified: $($skippedEntries -join ', ')."
	Write-Host '      Merge by hand: AGENTS.md must list the required reading and contain the four mandatory rules, and CLAUDE.md must import the shared docs (see check output above).'
}
Write-Host 'Next: fill every TODO(project) in AI_CONTEXT.md, .agents/harness/project-rules.md, MODEL_ROUTING.md, docs/00-master-plan.md and docs/architecture.md,'
Write-Host '      then run scripts/ai/check-ai-structure.ps1 -Strict.'
exit $exitCode
