param([string]$SdkRoot = 'C:\dev\helworks\tools\emsdk')
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath (Join-Path $SdkRoot 'emsdk.bat'))) {
    & git clone --depth 1 https://github.com/emscripten-core/emsdk.git $SdkRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not download the official Emscripten SDK.' }
}
& (Join-Path $SdkRoot 'emsdk.bat') install 6.0.12
if ($LASTEXITCODE -ne 0) { throw 'Emscripten 6.0.12 installation failed.' }
& (Join-Path $SdkRoot 'emsdk.bat') activate 6.0.12
if ($LASTEXITCODE -ne 0) { throw 'Emscripten activation failed.' }
& (Join-Path $SdkRoot 'upstream\emscripten\emcc.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'Emscripten compiler verification failed.' }
