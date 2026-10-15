[CmdletBinding()]
param(
    [string]$OutputRoot='C:\dev\helworks\builds\helengine-webgpu\renderer-parity\preload-rebuild',
    [string]$SdkRoot='C:\dev\helworks\tools\emsdk'
)
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tokens=$null;$parseErrors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'scripts\verify-demodisc-webgpu.ps1'),[ref]$tokens,[ref]$parseErrors)
$function=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-Checked'},$true)
Invoke-Expression $function.Extent.Text
$fixture=Join-Path $OutputRoot 'fixture';$content=Join-Path $fixture 'content';$native=Join-Path $OutputRoot 'native'
New-Item -ItemType Directory -Path $content -Force | Out-Null
$payload=Join-Path $content 'payload.txt';$added=Join-Path $content 'added.txt'
if(Test-Path -LiteralPath $added){Remove-Item -LiteralPath $added}
[IO.File]::WriteAllText($payload,'base')
[IO.File]::WriteAllText((Join-Path $fixture 'main.cpp'),'int main() { return 0; }')
$cmake=@'
cmake_minimum_required(VERSION 3.26)
project(preload_rebuild LANGUAGES CXX)
include("@HELPER@")
add_executable(preload main.cpp)
set_target_properties(preload PROPERTIES SUFFIX ".js")
target_link_options(preload PRIVATE --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/content@/content")
helengine_track_preload_content(preload "${CMAKE_CURRENT_SOURCE_DIR}/content")
'@
$cmake=$cmake.Replace('@HELPER@',(Join-Path $repository 'cmake\WebGpuPreloadContent.cmake').Replace('\','/'))
[IO.File]::WriteAllText((Join-Path $fixture 'CMakeLists.txt'),$cmake)
$env:EM_CONFIG=Join-Path $SdkRoot '.emscripten'
$env:PATH=(Join-Path $SdkRoot 'upstream\emscripten')+[IO.Path]::PathSeparator+$env:PATH
$toolchain=Join-Path $SdkRoot 'upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake'
Invoke-Checked (Join-Path $OutputRoot 'configure.log') {cmake -S $fixture -B $native -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain"}
Invoke-Checked (Join-Path $OutputRoot 'initial.log') {cmake --build $native}
$data=Join-Path $native 'preload.data'
if([IO.File]::ReadAllText($data) -ne 'base'){throw 'Initial preload must contain the fixture payload.'}
[IO.File]::WriteAllText($payload,'edit')
Invoke-Checked (Join-Path $OutputRoot 'modified.log') {cmake --build $native}
if([IO.File]::ReadAllText($data) -ne 'edit'){throw 'Editing only content must relink the preload package.'}
[IO.File]::WriteAllText($payload,'back')
[IO.File]::SetLastWriteTimeUtc($payload,[DateTime]::Parse('2000-01-01T00:00:00Z').ToUniversalTime())
Invoke-Checked (Join-Path $OutputRoot 'restored-configure.log') {cmake -S $fixture -B $native -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain"}
Invoke-Checked (Join-Path $OutputRoot 'restored.log') {cmake --build $native}
if([IO.File]::ReadAllText($data) -ne 'back'){throw 'Restoring changed bytes with an old timestamp must update the preload package after official configure.'}
[IO.File]::WriteAllText($added,'added')
[IO.File]::SetLastWriteTimeUtc($added,[DateTime]::Parse('2000-01-01T00:00:00Z').ToUniversalTime())
Invoke-Checked (Join-Path $OutputRoot 'added.log') {cmake --build $native}
$bytes=[IO.File]::ReadAllText($data)
if(-not $bytes.Contains('added') -or -not $bytes.Contains('back')){throw 'Adding even an old-dated asset must update the preload package.'}
Remove-Item -LiteralPath $added
Invoke-Checked (Join-Path $OutputRoot 'removed.log') {cmake --build $native}
if([IO.File]::ReadAllText($data) -ne 'back'){throw 'Removing content must remove it from the preload package.'}
$timestamp=(Get-Item -LiteralPath $data).LastWriteTimeUtc
Invoke-Checked (Join-Path $OutputRoot 'unchanged-configure.log') {cmake -S $fixture -B $native -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain"}
Invoke-Checked (Join-Path $OutputRoot 'unchanged.log') {cmake --build $native}
if((Get-Item -LiteralPath $data).LastWriteTimeUtc -ne $timestamp){throw 'An unchanged package must remain cached.'}
Write-Output 'Preload rebuild checks passed: edit, old-dated restoration, old-dated addition, removal, and unchanged cache.'
