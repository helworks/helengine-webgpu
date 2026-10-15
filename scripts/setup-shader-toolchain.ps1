param([string]$ToolRoot = 'C:\dev\helworks\tools\webgpu-shaders')
$ErrorActionPreference = 'Stop'
$revision = '31e25af254ab572c77054edec4946d2244e184dd'
$jsonHash = 'aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63'
$ToolRoot = [IO.Path]::GetFullPath($ToolRoot)
$source = Join-Path $ToolRoot "dawn-$revision"
$archive = Join-Path $ToolRoot "dawn-$revision.tar.gz"
$repo = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path $ToolRoot,(Join-Path $ToolRoot 'include/nlohmann'),(Join-Path $ToolRoot 'licenses') | Out-Null
if (-not (Test-Path -LiteralPath (Join-Path $source 'tools/fetch_dawn_dependencies.py'))) {
    if (-not (Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri "https://codeload.github.com/google/dawn/tar.gz/$revision" -OutFile $archive
    }
    # Unix-only HermeticXcode links cannot be created on Windows and are not compiler inputs.
    & tar -xf $archive -C $ToolRoot --exclude='*/src/cmake/HermeticXcode/*'
    if ($LASTEXITCODE -ne 0) { throw 'Dawn source extraction failed.' }
}
$header = Join-Path $ToolRoot 'include/nlohmann/json.hpp'
if (-not (Test-Path -LiteralPath $header)) {
    Invoke-WebRequest -Uri 'https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp' -OutFile $header
}
if ((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash.ToLowerInvariant() -ne $jsonHash) { throw 'nlohmann/json 3.12.0 checksum mismatch.' }
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT' -OutFile (Join-Path $ToolRoot 'licenses/nlohmann-json.txt')
Copy-Item -LiteralPath (Join-Path $source 'LICENSE') -Destination (Join-Path $ToolRoot 'licenses/dawn-tint.txt')
& python (Join-Path $repo 'tools/shader-tool/fetch_dependencies.py') $source
if ($LASTEXITCODE -ne 0) { throw 'Pinned Dawn compiler dependency fetch failed.' }
$build = Join-Path $ToolRoot 'build'
& cmake -S (Join-Path $repo 'tools/shader-tool') -B $build -G 'Visual Studio 17 2022' -A x64 "-DDAWN_SOURCE_ROOT=$source" "-DSHADER_TOOL_ROOT=$ToolRoot"
if ($LASTEXITCODE -ne 0) { throw 'WebGPU shader adapter configuration failed.' }
& cmake --build $build --config Release --target helengine-webgpu-shader --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'WebGPU shader adapter compilation failed.' }
$compiler = Join-Path $ToolRoot 'bin/helengine-webgpu-shader.exe'
$dependencies = @{}
foreach ($relative in @('third_party/abseil-cpp','third_party/spirv-headers/src','third_party/spirv-tools/src','third_party/jinja2','third_party/markupsafe','third_party/webgpu-headers/src')) {
    $dependency = Join-Path $source $relative
    $identity = & git -C $dependency rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw "Cannot identify pinned dependency $relative" }
    $dependencies[$relative] = $identity.Trim()
}
$lock = [ordered]@{
    schemaVersion=1; dawnRevision=$revision; adapterVersion='1'
    compilerSha256=(Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant()
    jsonSha256=$jsonHash; dawnSourceSha256=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    dependencies=$dependencies
}
[IO.File]::WriteAllText((Join-Path $ToolRoot 'toolchain.lock.json'), ($lock | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
Write-Output "WebGPU shader tool installed: $compiler"
