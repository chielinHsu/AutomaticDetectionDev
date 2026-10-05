<#
	Validate the shared Codex / Claude Code structure of this repository.

	  check-ai-structure.ps1            FAIL on structural errors, WARN on unfilled TODO(project)
	  check-ai-structure.ps1 -Strict    unfilled TODO(project) also FAILs

	Exit code 0 = no FAIL, 1 = at least one FAIL.
	This checks structure only. It does not judge whether the content is good.
#>

[CmdletBinding()]
param(
	[switch]$Strict,
	[string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot))
{
	$RepositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

$fails = New-Object System.Collections.Generic.List[string]
$warns = New-Object System.Collections.Generic.List[string]

function Add-Fail { param([string]$Message) $script:fails.Add($Message) }
function Add-Warn { param([string]$Message) $script:warns.Add($Message) }

function Read-Text
{
	param([string]$RelativePath)

	return [IO.File]::ReadAllText((Join-Path $RepositoryRoot $RelativePath), [Text.Encoding]::UTF8)
}

function Test-RepoFile
{
	param([string]$RelativePath)

	return Test-Path -LiteralPath (Join-Path $RepositoryRoot $RelativePath) -PathType Leaf
}

# ---- 1. required files -------------------------------------------------------
$requiredFiles = @(
	'AGENTS.md',
	'CLAUDE.md',
	'AI_CONTEXT.md',
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/project-rules.md',
	'.agents/harness/skill-sync.json',
	'.ai/handoff.md',
	'docs/architecture.md',
	'docs/decisions.md',
	'docs/current-status.md',
	'docs/changelog-ai.md',
	'scripts/ai/sync-skills.ps1'
)
foreach ($file in $requiredFiles)
{
	if (-not (Test-RepoFile $file))
	{
		Add-Fail "missing required file: $file"
	}
}

# ---- 2. AGENTS.md (Codex entry, shared by Claude through the import) --------
$readFirst = @(
	'AI_CONTEXT.md',
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/project-rules.md',
	'.ai/handoff.md',
	'docs/current-status.md'
)

if (Test-RepoFile 'AGENTS.md')
{
	$agentsText = Read-Text 'AGENTS.md'
	$agentsBytes = (Get-Item -LiteralPath (Join-Path $RepositoryRoot 'AGENTS.md')).Length

	foreach ($path in $readFirst)
	{
		if ($agentsText.IndexOf($path, [StringComparison]::Ordinal) -lt 0)
		{
			Add-Fail "AGENTS.md does not list required reading: $path"
		}
	}

	# Codex stops adding AGENTS.md content once the combined size reaches 32 KiB.
	if ($agentsBytes -gt 32768)
	{
		Add-Fail "AGENTS.md is $agentsBytes bytes; Codex truncates project docs above 32768 bytes"
	}
	$agentsLines = ($agentsText -split "`n").Count
	if ($agentsLines -gt 80)
	{
		Add-Warn "AGENTS.md has $agentsLines lines; it is an entry file, move content into the shared docs"
	}

	foreach ($match in [regex]::Matches($agentsText, '\]\(([^)#\s]+)(#[^)]*)?\)'))
	{
		$target = $match.Groups[1].Value
		if (($target -notmatch '^[a-zA-Z][a-zA-Z0-9+.-]*:') -and (-not (Test-RepoFile $target)))
		{
			Add-Fail "AGENTS.md links to a file that does not exist: $target"
		}
	}
}

# ---- 3. CLAUDE.md (Claude entry) ---------------------------------------------
$requiredImports = @(
	'AGENTS.md',
	'AI_CONTEXT.md',
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/project-rules.md'
)

if (Test-RepoFile 'CLAUDE.md')
{
	$claudeText = Read-Text 'CLAUDE.md'
	$imports = @()
	foreach ($match in [regex]::Matches($claudeText, '(?m)^@(\S+)\s*$'))
	{
		$imports += $match.Groups[1].Value
	}

	foreach ($path in $requiredImports)
	{
		if ($imports -notcontains $path)
		{
			Add-Fail "CLAUDE.md does not import: @$path"
		}
	}
	foreach ($path in $imports)
	{
		if (-not (Test-RepoFile $path))
		{
			Add-Fail "CLAUDE.md imports a file that does not exist: @$path"
		}
	}

	$claudeLines = ($claudeText -split "`n").Count
	if ($claudeLines -gt 80)
	{
		Add-Warn "CLAUDE.md has $claudeLines lines; keep only Claude-specific rules here"
	}
}

# ---- 4. state files stay small ----------------------------------------------
$lineLimits = @{
	'.ai/handoff.md'          = 60
	'docs/current-status.md'  = 80
}
foreach ($file in $lineLimits.Keys)
{
	if (Test-RepoFile $file)
	{
		$count = ((Read-Text $file) -split "`n").Count
		if ($count -gt $lineLimits[$file])
		{
			Add-Warn "$file has $count lines (limit $($lineLimits[$file])); trim it or move history to docs/changelog-ai.md"
		}
	}
}

# ---- 5. unreplaced template placeholders -------------------------------------
$placeholderScan = @('AGENTS.md', 'CLAUDE.md', 'AI_CONTEXT.md', '.ai/handoff.md') +
	@(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'docs') -Filter '*.md' -File -ErrorAction SilentlyContinue |
		ForEach-Object { 'docs/' + $_.Name })
foreach ($file in $placeholderScan)
{
	if ((Test-RepoFile $file) -and ([regex]::IsMatch((Read-Text $file), '\{\{[A-Z_]+\}\}')))
	{
		Add-Fail "$file still contains an unreplaced template placeholder"
	}
}

# ---- 6. unfilled TODO(project) -----------------------------------------------
$todoFiles = @('AI_CONTEXT.md', '.agents/harness/project-rules.md', 'docs/architecture.md')
$todoTotal = 0
$todoDetail = @()
foreach ($file in $todoFiles)
{
	if (Test-RepoFile $file)
	{
		$count = [regex]::Matches((Read-Text $file), 'TODO\(project\):').Count
		if ($count -gt 0)
		{
			$todoTotal += $count
			$todoDetail += "$file ($count)"
		}
	}
}
if ($todoTotal -gt 0)
{
	$message = "unfilled TODO(project): $todoTotal in " + ($todoDetail -join ', ')
	if ($Strict) { Add-Fail $message } else { Add-Warn $message }
}

# ---- 7. Claude settings and gitignore ----------------------------------------
if (Test-RepoFile '.claude/settings.json')
{
	try
	{
		Read-Text '.claude/settings.json' | ConvertFrom-Json | Out-Null
	}
	catch
	{
		Add-Fail ".claude/settings.json is not valid JSON: $($_.Exception.Message)"
	}
}
else
{
	Add-Warn '.claude/settings.json is missing; git add -A / add . will not be blocked for Claude Code'
}

if (Test-RepoFile '.gitignore')
{
	if ((Read-Text '.gitignore') -notmatch '(?m)^\.claude/settings\.local\.json\s*$')
	{
		Add-Warn '.gitignore does not ignore .claude/settings.local.json'
	}
}
else
{
	Add-Warn '.gitignore is missing'
}

# ---- 8. skill mirror ----------------------------------------------------------
$syncScript = Join-Path $PSScriptRoot 'sync-skills.ps1'
if (Test-Path -LiteralPath $syncScript -PathType Leaf)
{
	# The child prints its own per-skill FAIL lines to the console.
	& $syncScript -Check -RepositoryRoot $RepositoryRoot
	if ($LASTEXITCODE -ne 0)
	{
		Add-Fail 'skills: .agents/skills and .claude/skills are out of sync (details above); run scripts/ai/sync-skills.ps1'
	}
}

# ---- report -------------------------------------------------------------------
$versionPath = Join-Path $RepositoryRoot '.agents/harness/TEMPLATE_VERSION'
if (Test-Path -LiteralPath $versionPath -PathType Leaf)
{
	Write-Host ("Template version: " + ([IO.File]::ReadAllText($versionPath).Trim()))
}
foreach ($warn in $warns) { Write-Host "WARN $warn" }
foreach ($fail in $fails) { Write-Host "FAIL $fail" }

if ($fails.Count -gt 0)
{
	Write-Host "RESULT: FAILED ($($fails.Count) failure(s), $($warns.Count) warning(s))"
	exit 1
}

Write-Host "RESULT: OK ($($warns.Count) warning(s))"
exit 0
