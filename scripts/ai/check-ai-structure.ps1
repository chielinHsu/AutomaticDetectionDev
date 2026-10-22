<#
	Validate the shared Codex / Claude Code structure of this repository (template 2.0.0).

	  check-ai-structure.ps1            FAIL on structural errors, WARN on unfilled TODO(project)
	  check-ai-structure.ps1 -Strict    unfilled TODO(project) also FAILs

	Exit code 0 = no FAIL, 1 = at least one FAIL.
	This checks structure only. It does not judge whether the content is good.

	What is checked (a file is only checked when it exists, unless it is in the required list):
	  - required files of template 2.0.0 and TEMPLATE_VERSION (major version 2)
	  - git repository (WARN only: a new project has no git yet)
	  - strict UTF-8 decoding of every scanned file, U+FFFD (replacement character) scan
	  - AGENTS.md: required reading, the four mandatory rules (keywords), size, links, no "@" import lines
	  - CLAUDE.md: required imports (and @MODEL_ROUTING.md when MODEL_ROUTING.md exists)
	  - AI_CONTEXT.md has the required sections
	  - docs/current-status.md has the requirements table (needs the Chinese heading "需求進度表")
	  - docs/decisions.md is an index (no embedded ADR) and docs/adr/ exists, linked ADR files exist
	  - length limits: .ai/handoff.md 60, AGENTS.md 80, CLAUDE.md 40 lines (FAIL, same as harness 1.0.0);
	    docs/current-status.md 150 lines (WARN)
	  - unreplaced {{PLACEHOLDER}} and fill-in markers, unfilled TODO(project): (FAIL with -Strict)
	  - .claude/settings.json: valid JSON, no bypassPermissions, deny rules for "git add ." and "git add -A",
	    hook command uses ${CLAUDE_PROJECT_DIR} (a bare $CLAUDE_PROJECT_DIR is empty under PowerShell)
	  - skill-sync.json fields, agent frontmatter name = file name, skill frontmatter name = folder name
	  - .gitignore ignores .claude/settings.local.json, .claude/worktrees/ and the bench output folder
	  - paths written in backticks inside the main docs exist (WARN; only once AI_CONTEXT.md has no TODO(project):)
	  - skill mirror (.agents/skills vs .claude/skills) through sync-skills.ps1 -Check
	  - model routing, when MODEL_ROUTING.md exists: five agents, hook, settings wiring, and
	    node scripts/ai/check-model-routing.mjs
	This file contains Chinese keywords: keep it saved as UTF-8 with BOM (Windows PowerShell 5.1).
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
	# Split-Path instead of Join-Path '..\..': a backslash is an ordinary character on Linux (CI runs this under pwsh there).
	$RepositoryRoot = (Resolve-Path -LiteralPath (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))).Path
}
else
{
	$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
}

$fails = New-Object System.Collections.Generic.List[string]
$warns = New-Object System.Collections.Generic.List[string]

function Add-Fail { param([string]$Message) $script:fails.Add($Message) }
function Add-Warn { param([string]$Message) $script:warns.Add($Message) }

# Strict UTF-8: an invalid byte sequence is a FAIL (reported once per file), not silent mojibake.
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$textCache = @{}

function Read-Text
{
	param([string]$RelativePath)

	if ($script:textCache.ContainsKey($RelativePath))
	{
		return $script:textCache[$RelativePath]
	}
	$bytes = [IO.File]::ReadAllBytes((Join-Path $RepositoryRoot $RelativePath))
	$offset = 0
	if (($bytes.Length -ge 3) -and ($bytes[0] -eq 0xEF) -and ($bytes[1] -eq 0xBB) -and ($bytes[2] -eq 0xBF))
	{
		$offset = 3
	}
	try
	{
		$text = $strictUtf8.GetString($bytes, $offset, $bytes.Length - $offset)
	}
	catch
	{
		Add-Fail "$RelativePath is not valid UTF-8"
		$text = ''
	}
	$script:textCache[$RelativePath] = $text
	return $text
}

function Test-RepoFile
{
	param([string]$RelativePath)

	return Test-Path -LiteralPath (Join-Path $RepositoryRoot $RelativePath) -PathType Leaf
}

function Test-RepoDirectory
{
	param([string]$RelativePath)

	return Test-Path -LiteralPath (Join-Path $RepositoryRoot $RelativePath) -PathType Container
}

# Property access that does not throw under Set-StrictMode when the property is missing.
function Get-Prop
{
	param($Object, [string]$Name)

	if (($null -ne $Object) -and ($null -ne $Object.PSObject.Properties[$Name]))
	{
		return $Object.PSObject.Properties[$Name].Value
	}
	return $null
}

function Get-LineCount
{
	param([string]$Text)

	if ($Text.Length -eq 0)
	{
		return 0
	}
	$count = ($Text -split "`n").Count
	if ($Text.EndsWith("`n"))
	{
		$count -= 1
	}
	return $count
}

# ---- 1. git ----------------------------------------------------------------------
# A new project has no git yet (New-AiProject.ps1 does not run git init), so this is only a warning.
$gitCommand = Get-Command git -ErrorAction SilentlyContinue
if ($null -eq $gitCommand)
{
	Add-Warn 'git was not found in PATH; the version control check was skipped'
}
else
{
	$previousPreference = $ErrorActionPreference
	$ErrorActionPreference = 'Continue'
	try
	{
		$gitResult = (& git -C $RepositoryRoot rev-parse --is-inside-work-tree 2>&1 | Out-String).Trim()
		$gitExit = $LASTEXITCODE
	}
	finally
	{
		$ErrorActionPreference = $previousPreference
	}
	if (($gitExit -ne 0) -or ($gitResult -ne 'true'))
	{
		Add-Warn 'this folder is not inside a git repository; run git init (engineering-workflow.md section 0: version control first)'
	}
}

# ---- 2. required files (template 2.0.0) --------------------------------------
$requiredFiles = @(
	'AGENTS.md',
	'CLAUDE.md',
	'AI_CONTEXT.md',
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/project-rules.md',
	'.agents/harness/skill-sync.json',
	'.agents/harness/TEMPLATE_VERSION',
	'.agents/skills/ai-handoff/SKILL.md',
	'.ai/handoff.md',
	'docs/00-master-plan.md',
	'docs/architecture.md',
	'docs/decisions.md',
	'docs/current-status.md',
	'docs/changelog-ai.md',
	'docs/plan/_template.md',
	'docs/adr/_template.md',
	'docs/steps/_template/spec.md',
	'docs/steps/_template/diagram.md',
	'docs/steps/_template/explainer.html',
	'docs/steps/_template/perf.md',
	'docs/steps/_template/storyboard.md',
	'tools/ste-lint.mjs',
	'scripts/ai/sync-skills.ps1',
	'scripts/ai/check-ai-structure.ps1'
)
foreach ($file in $requiredFiles)
{
	if (-not (Test-RepoFile $file))
	{
		Add-Fail "missing required file: $file"
	}
}
if (-not (Test-RepoDirectory 'docs/adr'))
{
	Add-Fail 'missing required folder: docs/adr (one file per decision: docs/adr/NNN-slug.md)'
}

# ---- 3. TEMPLATE_VERSION ------------------------------------------------------
$templateVersion = ''
if (Test-RepoFile '.agents/harness/TEMPLATE_VERSION')
{
	$templateVersion = (Read-Text '.agents/harness/TEMPLATE_VERSION').Trim()
	if ($templateVersion -notmatch '^2\.\d+\.\d+$')
	{
		Add-Fail "TEMPLATE_VERSION is '$templateVersion'; this checker is for template 2.x (run New-AiProject.ps1 -Refresh from the 2.0.0 template)"
	}
}

# ---- 4. AGENTS.md (Codex entry, shared by Claude through the import) --------
$readFirst = @(
	'AI_CONTEXT.md',
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/project-rules.md',
	'.ai/handoff.md',
	'docs/current-status.md',
	'ai-handoff'
)

# The four mandatory rules: [rule description, keywords that must all appear in AGENTS.md].
$mandatoryRules = @(
	@('rule 1 (reply in Traditional Chinese zh-TW, summarize on completion)', @('繁體中文', 'zh-TW', '總結')),
	@('rule 2 (no fabricated verification, no hidden failures)', @('不虛構驗證', '不隱藏失敗')),
	@('rule 3 (no unrequested commit/push, explicit staging)', @('不擅自 commit', 'git add -A')),
	@('rule 4 (expand requests into the requirements table before implementing)', @('需求進度表', 'docs/current-status.md'))
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

	foreach ($rule in $mandatoryRules)
	{
		foreach ($keyword in $rule[1])
		{
			if ($agentsText.IndexOf($keyword, [StringComparison]::Ordinal) -lt 0)
			{
				Add-Fail "AGENTS.md is missing the mandatory $($rule[0]): keyword not found: $keyword"
			}
		}
	}

	# Codex stops adding AGENTS.md content once the combined size reaches 32 KiB.
	if ($agentsBytes -gt 32768)
	{
		Add-Fail "AGENTS.md is $agentsBytes bytes; Codex truncates project docs above 32768 bytes"
	}
	if ($agentsText -match '(?m)^@')
	{
		Add-Warn 'AGENTS.md has an "@" import line; Codex does not parse imports, import from CLAUDE.md instead'
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

# ---- 5. CLAUDE.md (Claude entry) ---------------------------------------------
$requiredImports = @(
	'AGENTS.md',
	'AI_CONTEXT.md',
	'.agents/harness/engineering-workflow.md',
	'.agents/harness/project-rules.md'
)
$hasModelRouting = Test-RepoFile 'MODEL_ROUTING.md'
if ($hasModelRouting)
{
	$requiredImports += 'MODEL_ROUTING.md'
}

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
			Add-Fail "CLAUDE.md does not import: @$path (one import per line)"
		}
	}
	foreach ($path in $imports)
	{
		if (-not (Test-RepoFile $path))
		{
			Add-Fail "CLAUDE.md imports a file that does not exist: @$path"
		}
	}
}

# ---- 6. AI_CONTEXT.md sections ------------------------------------------------
if (Test-RepoFile 'AI_CONTEXT.md')
{
	$contextText = Read-Text 'AI_CONTEXT.md'
	# Heading text may carry a prefix (for example "專案用途"); the key word must be inside an H2 heading.
	$sections = @('用途', '技術棧', '重要目錄', '建置與測試', '限制與不變條件', '詞彙', 'Context Routing')
	foreach ($section in $sections)
	{
		if (-not [regex]::IsMatch($contextText, '(?m)^##\s+.*' + [regex]::Escape($section)))
		{
			Add-Fail "AI_CONTEXT.md has no '## ...$section' section"
		}
	}
}

# ---- 7. requirements table in docs/current-status.md --------------------------
if (Test-RepoFile 'docs/current-status.md')
{
	$statusText = Read-Text 'docs/current-status.md'
	if (-not [regex]::IsMatch($statusText, '(?m)^##\s+需求進度表'))
	{
		Add-Fail 'docs/current-status.md has no "## 需求進度表" section (the requirements table, see engineering-workflow.md)'
	}
	else
	{
		$firstHeading = [regex]::Match($statusText, '(?m)^##\s+(.+?)\s*$').Groups[1].Value
		if ($firstHeading -notlike '需求進度表*')
		{
			Add-Warn "docs/current-status.md: the requirements table should be the first section (first '##' heading is '$firstHeading')"
		}
	}
}

# ---- 8. decisions.md is an index, docs/adr/ holds the decisions ---------------
if (Test-RepoFile 'docs/decisions.md')
{
	$decisionsText = Read-Text 'docs/decisions.md'
	if ([regex]::IsMatch($decisionsText, '(?m)^##\s+(ADR|D)-?\d+'))
	{
		Add-Fail 'docs/decisions.md still embeds decisions (## ADR-nnn / ## D-nnnn headings); move them to docs/adr/NNN-slug.md and keep only an index here'
	}
	$adrLinks = [regex]::Matches($decisionsText, '\]\((adr/[^)#\s]+)(#[^)]*)?\)')
	if ($adrLinks.Count -eq 0)
	{
		Add-Fail 'docs/decisions.md has no links to docs/adr/ files; it must be an index (one row per ADR)'
	}
	foreach ($match in $adrLinks)
	{
		$target = 'docs/' + $match.Groups[1].Value
		if (-not (Test-RepoFile $target))
		{
			Add-Fail "docs/decisions.md links to an ADR file that does not exist: $target"
		}
	}
}

# ---- 9. document length -------------------------------------------------------
# Harness 1.0.0 failed on these limits, so they stay FAIL: handoff is overwritten each time (never accumulates),
# and the two entry files are bootloaders. current-status.md grows with the requirements table: WARN only.
$failLimits = [ordered]@{
	'.ai/handoff.md' = 60
	'AGENTS.md'      = 80
	'CLAUDE.md'      = 40
}
foreach ($file in $failLimits.Keys)
{
	if (Test-RepoFile $file)
	{
		$count = Get-LineCount (Read-Text $file)
		if ($count -gt $failLimits[$file])
		{
			Add-Fail "$file has $count lines (limit $($failLimits[$file])); trim it (handoff is overwritten, entry files only point to shared docs)"
		}
	}
}
if (Test-RepoFile 'docs/current-status.md')
{
	$count = Get-LineCount (Read-Text 'docs/current-status.md')
	if ($count -gt 150)
	{
		Add-Warn "docs/current-status.md has $count lines (limit 150); move history to docs/changelog-ai.md"
	}
}

# ---- 10. scanned documents: UTF-8, U+FFFD, placeholders, TODO(project) --------
$scanFiles = New-Object System.Collections.Generic.List[string]
foreach ($file in @('AGENTS.md', 'CLAUDE.md', 'AI_CONTEXT.md', 'MODEL_ROUTING.md', '.ai/handoff.md'))
{
	if (Test-RepoFile $file) { $scanFiles.Add($file) }
}
$scanFolders = @(
	@{ Path = 'docs';              Filters = @('*.md', '*.html') },
	@{ Path = '.agents';           Filters = @('*.md', '*.json') },
	@{ Path = '.claude/agents';    Filters = @('*.md') },
	@{ Path = '.claude/skills';    Filters = @('*.md') },
	@{ Path = '.claude/hooks';     Filters = @('*.mjs') },
	@{ Path = 'scripts/ai';        Filters = @('*.mjs') },
	@{ Path = 'tools';             Filters = @('*.mjs') }
)
foreach ($folder in $scanFolders)
{
	if (-not (Test-RepoDirectory $folder.Path))
	{
		continue
	}
	$base = (Resolve-Path -LiteralPath (Join-Path $RepositoryRoot $folder.Path)).Path
	foreach ($filter in $folder.Filters)
	{
		foreach ($item in (Get-ChildItem -LiteralPath $base -Recurse -File -Filter $filter -ErrorAction SilentlyContinue))
		{
			$relative = $folder.Path + '/' + $item.FullName.Substring($base.Length + 1).Replace('\', '/')
			if (-not $scanFiles.Contains($relative)) { $scanFiles.Add($relative) }
		}
	}
}
foreach ($file in $scanFiles)
{
	$text = Read-Text $file
	if ($text.IndexOf([char]0xFFFD) -ge 0)
	{
		Add-Fail "$file contains the replacement character U+FFFD (garbled text)"
	}
}

# Placeholder scan: only the entry and top-level docs (the _template files legitimately use markers).
$placeholderScan = @('AGENTS.md', 'CLAUDE.md', 'AI_CONTEXT.md', 'MODEL_ROUTING.md', '.ai/handoff.md') +
	@(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'docs') -Filter '*.md' -File -ErrorAction SilentlyContinue |
		ForEach-Object { 'docs/' + $_.Name })
foreach ($file in $placeholderScan)
{
	if (Test-RepoFile $file)
	{
		$text = Read-Text $file
		if ([regex]::IsMatch($text, '\{\{[A-Z_]+\}\}'))
		{
			Add-Fail "$file still contains an unreplaced template placeholder"
		}
		if ([regex]::IsMatch($text, '〈填入|<填入|待填入'))
		{
			Add-Fail "$file still contains a fill-in marker (〈填入 / <填入 / 待填入)"
		}
	}
}

$todoFiles = @('AI_CONTEXT.md', '.agents/harness/project-rules.md', 'MODEL_ROUTING.md', 'docs/00-master-plan.md', 'docs/architecture.md')
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

# ---- 11. Claude settings ---------------------------------------------------------
$settingsJson = $null
if (Test-RepoFile '.claude/settings.json')
{
	try
	{
		$settingsJson = Read-Text '.claude/settings.json' | ConvertFrom-Json
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

if ($null -ne $settingsJson)
{
	$permissions = Get-Prop $settingsJson 'permissions'
	if ((Get-Prop $permissions 'defaultMode') -eq 'bypassPermissions')
	{
		Add-Fail '.claude/settings.json sets permissions.defaultMode to bypassPermissions'
	}
	$deny = @(Get-Prop $permissions 'deny')
	# "git add -A" may be denied exactly or as a prefix rule ("git add -A*"); "git add ." must be denied exactly.
	foreach ($rule in @('Bash(git add .)', 'Bash(git add -A)'))
	{
		$prefixRule = $rule.Substring(0, $rule.Length - 1) + '*)'
		$accepted = ($deny -contains $rule)
		if ((-not $accepted) -and ($rule -ne 'Bash(git add .)'))
		{
			$accepted = ($deny -contains $prefixRule)
		}
		if (-not $accepted)
		{
			Add-Fail ".claude/settings.json permissions.deny is missing $rule"
		}
	}
	$hooksText = ''
	$hooksValue = Get-Prop $settingsJson 'hooks'
	if ($null -ne $hooksValue)
	{
		$hooksText = ($hooksValue | ConvertTo-Json -Depth 20)
	}
	if ($hooksText -match '\$CLAUDE_PROJECT_DIR')
	{
		Add-Fail '.claude/settings.json hook uses $CLAUDE_PROJECT_DIR; under PowerShell it expands to an empty string, write ${CLAUDE_PROJECT_DIR}'
	}
}

if (Test-RepoFile '.agents/harness/skill-sync.json')
{
	$syncManifest = $null
	try
	{
		$syncManifest = Read-Text '.agents/harness/skill-sync.json' | ConvertFrom-Json
	}
	catch
	{
		Add-Fail ".agents/harness/skill-sync.json is not valid JSON: $($_.Exception.Message)"
	}
	if ($null -ne $syncManifest)
	{
		# Two manifest layouts exist: sourceDirectory/targetDirectory (template) and source/targets (older projects).
		$flat = ($null -ne (Get-Prop $syncManifest 'sourceDirectory')) -and ($null -ne (Get-Prop $syncManifest 'targetDirectory'))
		$nested = ($null -ne (Get-Prop $syncManifest 'source')) -and ($null -ne (Get-Prop $syncManifest 'targets'))
		if ((-not $flat) -and (-not $nested))
		{
			Add-Fail 'skill-sync.json needs sourceDirectory + targetDirectory (or source + targets)'
		}
		foreach ($key in @('claudeOnly', 'codexOnly'))
		{
			if ($null -eq $syncManifest.PSObject.Properties[$key])
			{
				Add-Fail "skill-sync.json is missing the field $key"
			}
		}
	}
}

# Agent frontmatter: name must equal the file name (Claude Code loads the agent by name).
if (Test-RepoDirectory '.claude/agents')
{
	foreach ($item in (Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot '.claude/agents') -File -Filter '*.md'))
	{
		$agentName = [IO.Path]::GetFileNameWithoutExtension($item.Name)
		$relative = ".claude/agents/$($item.Name)"
		$pattern = '(?s)^---\r?\n(?:.*?\r?\n)?name:\s*' + [regex]::Escape($agentName) + '\s*\r?\n.*?\r?\n---'
		if (-not [regex]::IsMatch((Read-Text $relative), $pattern))
		{
			Add-Fail "$relative frontmatter is missing or its name does not equal '$agentName'"
		}
	}
}

# Skill frontmatter: name must equal the folder name and description must not be empty.
if (Test-RepoDirectory '.agents/skills')
{
	foreach ($skill in (Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot '.agents/skills') -Directory))
	{
		$relative = ".agents/skills/$($skill.Name)/SKILL.md"
		if (-not (Test-RepoFile $relative))
		{
			continue
		}
		$pattern = '(?s)^---\r?\nname:\s*' + [regex]::Escape($skill.Name) + '\s*\r?\ndescription:\s*\S.*?\r?\n---'
		if (-not [regex]::IsMatch((Read-Text $relative), $pattern))
		{
			Add-Fail "$relative frontmatter needs name (equal to the folder name) followed by a non-empty description"
		}
	}
}

# ---- 12. .gitignore -------------------------------------------------------------
if (Test-RepoFile '.gitignore')
{
	$ignoreText = Read-Text '.gitignore'
	$ignorePatterns = New-Object System.Collections.Generic.List[string]
	$ignorePatterns.Add('.claude/settings.local.json')
	$ignorePatterns.Add('.claude/worktrees/')
	# The bench output folder is whatever bench-model-routing.mjs writes to (.ai/bench/ or perf/results/).
	if (Test-RepoFile 'scripts/ai/bench-model-routing.mjs')
	{
		$benchText = Read-Text 'scripts/ai/bench-model-routing.mjs'
		if ([regex]::IsMatch($benchText, '\.ai[''"/\\, ]+bench'))
		{
			$ignorePatterns.Add('.ai/bench/')
		}
		elseif ([regex]::IsMatch($benchText, 'perf[''"/\\, ]+results'))
		{
			$ignorePatterns.Add('perf/results/')
		}
	}
	foreach ($pattern in $ignorePatterns)
	{
		if ($ignoreText.IndexOf($pattern, [StringComparison]::Ordinal) -lt 0)
		{
			Add-Fail ".gitignore does not contain $pattern"
		}
	}
}
else
{
	Add-Fail '.gitignore is missing (it must ignore .claude/settings.local.json, .claude/worktrees/ and the bench output folder)'
}

# ---- 13. paths written in backticks exist ---------------------------------------
# Only once the project is filled in: a fresh template legitimately names folders (src/, tests/) that do not exist yet.
$contextFilled = (Test-RepoFile 'AI_CONTEXT.md') -and ([regex]::Matches((Read-Text 'AI_CONTEXT.md'), 'TODO\(project\):').Count -eq 0)
if ($contextFilled)
{
	# handoff.md and current-status.md are not scanned: they name branches (docs/..., feat/...) and commits, not only files.
	$refFiles = @('AGENTS.md', 'CLAUDE.md', 'AI_CONTEXT.md', 'MODEL_ROUTING.md',
		'docs/architecture.md', 'docs/decisions.md', 'docs/00-master-plan.md')
	foreach ($item in @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot '.agents/harness') -File -Filter '*.md' -ErrorAction SilentlyContinue))
	{
		$refFiles += ('.agents/harness/' + $item.Name)
	}
	$refPattern = '`((?:src|tests|tools|scripts|docs|\.agents|\.claude|\.ai)/[^`\s*<>]+)`'
	foreach ($file in $refFiles)
	{
		if (-not (Test-RepoFile $file)) { continue }
		foreach ($match in [regex]::Matches((Read-Text $file), $refPattern))
		{
			$path = $match.Groups[1].Value.TrimEnd('/', '.', ',')
			# Skip placeholders, ellipses, and paths that are ignored by git on purpose (they exist only on some machines).
			if ($path -match 'NNN|Rnn|YYYY|-slug|\.\.\.|…|^\.claude/skills|settings\.local\.json|^\.ai/bench|^\.claude/worktrees')
			{
				continue
			}
			if (-not (Test-Path -LiteralPath (Join-Path $RepositoryRoot $path)))
			{
				# WARN, not FAIL: the backtick may be a branch name or an example, so a miss is a hint to check the doc.
				Add-Warn "$file refers to $path, which does not exist"
			}
		}
	}
}

# ---- 14. skill mirror ----------------------------------------------------------
# Prefer the project's own sync-skills.ps1 (it matches the project's skill-sync.json); fall back to the one next to this file.
$syncScript = Join-Path $RepositoryRoot 'scripts/ai/sync-skills.ps1'
if (-not (Test-Path -LiteralPath $syncScript -PathType Leaf))
{
	$syncScript = Join-Path $PSScriptRoot 'sync-skills.ps1'
}
if (Test-Path -LiteralPath $syncScript -PathType Leaf)
{
	# The child prints its own per-skill FAIL lines to the console.
	# Older project copies name the root parameter -Root instead of -RepositoryRoot; pass whichever exists.
	$syncParameters = (Get-Command -Name $syncScript).Parameters
	$syncArguments = @{ Check = $true }
	if ($syncParameters.ContainsKey('RepositoryRoot'))
	{
		$syncArguments['RepositoryRoot'] = $RepositoryRoot
	}
	elseif ($syncParameters.ContainsKey('Root'))
	{
		$syncArguments['Root'] = $RepositoryRoot
	}
	$global:LASTEXITCODE = 0
	try
	{
		& $syncScript @syncArguments
	}
	catch
	{
		Write-Host "FAIL sync-skills.ps1 raised an error: $($_.Exception.Message)"
		$global:LASTEXITCODE = 1
	}
	if ($LASTEXITCODE -ne 0)
	{
		Add-Fail 'skills: .agents/skills and .claude/skills are out of sync (details above); run scripts/ai/sync-skills.ps1'
	}
}

# ---- 15. model routing (Claude only; checked when MODEL_ROUTING.md exists) ------
if (-not $hasModelRouting)
{
	Add-Warn 'MODEL_ROUTING.md is missing; model routing for Claude Code subagents is not set up (template 2.0.0 includes it)'
}
else
{
	$agentNames = @('fast-worker', 'developer', 'developer-deep', 'architect', 'architect-deep')
	foreach ($agent in $agentNames)
	{
		if (-not (Test-RepoFile ".claude/agents/$agent.md"))
		{
			Add-Fail "model routing: missing .claude/agents/$agent.md"
		}
	}
	if (-not (Test-RepoFile '.claude/hooks/model_router.mjs'))
	{
		Add-Fail 'model routing: missing .claude/hooks/model_router.mjs'
	}
	if ($null -ne $settingsJson)
	{
		$hookText = ($settingsJson | ConvertTo-Json -Depth 20)
		if ($hookText.IndexOf('model_router.mjs', [StringComparison]::Ordinal) -lt 0)
		{
			Add-Fail 'model routing: .claude/settings.json does not register .claude/hooks/model_router.mjs (UserPromptSubmit)'
		}
	}

	$routingCheck = Join-Path $RepositoryRoot 'scripts/ai/check-model-routing.mjs'
	if (-not (Test-Path -LiteralPath $routingCheck -PathType Leaf))
	{
		Add-Fail 'model routing: missing scripts/ai/check-model-routing.mjs'
	}
	else
	{
		$node = Get-Command node -ErrorAction SilentlyContinue
		if ($null -eq $node)
		{
			Add-Warn 'node was not found in PATH; scripts/ai/check-model-routing.mjs was not run'
		}
		else
		{
			$previousPreference = $ErrorActionPreference
			$previousEncoding = [Console]::OutputEncoding
			$ErrorActionPreference = 'Continue'
			try
			{
				[Console]::OutputEncoding = [Text.Encoding]::UTF8
				$routingOutput = & $node.Source $routingCheck 2>&1 | Out-String
				$routingExit = $LASTEXITCODE
			}
			finally
			{
				$ErrorActionPreference = $previousPreference
				[Console]::OutputEncoding = $previousEncoding
			}
			foreach ($line in ($routingOutput -split "`r?`n"))
			{
				if ($line -match '^(FAIL|WARN|RESULT)')
				{
					Write-Host "model-routing: $line"
				}
			}
			if ($routingExit -ne 0)
			{
				Add-Fail "model routing: node scripts/ai/check-model-routing.mjs failed (exit code $routingExit; run it directly for details)"
			}
		}
	}
}

# ---- report -------------------------------------------------------------------
if ($templateVersion -ne '')
{
	Write-Host "Template version: $templateVersion"
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
