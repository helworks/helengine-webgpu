[CmdletBinding()]
param([string]$OutputRoot='C:\dev\helworks\builds\helengine-webgpu\renderer-parity\script-tests')
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tokens=$null;$parseErrors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'scripts\verify-demodisc-webgpu.ps1'),[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'The verifier script must parse.'}
$function=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-Checked'},$true)
Invoke-Expression $function.Extent.Text
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$successLog=Join-Path $OutputRoot 'stderr-success.log'
Invoke-Checked $successLog {cmd.exe /d /c 'echo expected warning 1>&2 & exit /b 0'}
if((Get-Content -LiteralPath $successLog -Raw) -notmatch 'expected warning'){throw 'Successful native stderr must be retained.'}
$failureLog=Join-Path $OutputRoot 'stderr-failure.log'
$rejected=$false
try {Invoke-Checked $failureLog {cmd.exe /d /c 'echo expected failure 1>&2 & exit /b 7'}} catch {if($_.Exception.Message -notmatch '\(7\)'){throw};$rejected=$true}
if(-not $rejected){throw 'Nonzero native exit codes must fail validation.'}
if((Get-Content -LiteralPath $failureLog -Raw) -notmatch 'expected failure'){throw 'Failed native stderr must be retained.'}
$rejected=$false
try {Invoke-Checked (Join-Path $OutputRoot 'powershell-failure.log') {throw 'expected PowerShell failure'}} catch {if($_.Exception.Message -notmatch 'expected PowerShell failure'){throw};$rejected=$true}
if(-not $rejected){throw 'PowerShell exceptions must propagate.'}
$rejected=$false
try {Invoke-Checked (Join-Path $OutputRoot 'missing-command.log') {& 'helengine-nonexistent-validation-command'}} catch {$rejected=$true}
if(-not $rejected){throw 'A missing native command must not reuse an earlier successful exit code.'}
Write-Output 'Verifier command checks passed: native stderr success, nonzero exit, PowerShell exception, and missing command.'
