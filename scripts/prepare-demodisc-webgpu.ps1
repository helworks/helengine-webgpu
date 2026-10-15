param(
    [string]$ProjectRoot='C:\dev\helprojs\demodisc',
    [string]$OutputRoot='C:\dev\helworks\builds\helengine-webgpu\renderer-parity',
    [switch]$Apply
)
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent $PSScriptRoot
$arguments=@('run','--project',(Join-Path $repository 'tests/content/helengine.webgpu.content.csproj'),'--disable-build-servers','--artifacts-path',(Join-Path $OutputRoot 'content-tool'),'--','--project-root',[IO.Path]::GetFullPath($ProjectRoot),'--report',(Join-Path $OutputRoot 'content-audit.json'))
if($Apply) {$arguments+='--apply'}
& dotnet @arguments
if($LASTEXITCODE -ne 0) {throw 'DemoDisc WebGPU preparation failed.'}
