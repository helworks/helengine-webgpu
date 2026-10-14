# HelEngine WebGPU

Builder `webgpu` e player WebAssembly da HelEngine. Compila o core e os módulos C# convertidos pelo codegen oficial para C++, com Emscripten 6.0.12 e o port Emdawnwebgpu. O player usa o conteúdo preparado pelo editor, incluindo a cena, componentes e UI; a simulação roda no core WASM.

## Primeiro marco

Projeto de referência: `C:\dev\helprojs\demodisc\project.heproj`. Cena authored: `C:\dev\helprojs\demodisc\assets\scenes\rendering\cube_test.helen`; identidade de runtime: `cube_test`.

- Malhas triangulares com posições/normais, material sólido, depth/culling e uma luz direcional sem sombras.
- UI da engine: sprites, atlas de fonte, painéis arredondados e scissor.
- Teclado/mouse no canvas, resize com DPI, liberação de teclas ao perder foco e relógio reiniciado após ocultar a aba.
- PCM16 little endian mono/stereo com Web Audio: gesto de desbloqueio, loop, parada, ganho e pausa de buses. O Cube Test é silencioso; áudio é validado separadamente.
- Perfis `debug` e `release`; release otimiza WASM/JavaScript com Closure. Uma thread, sem exigência de SharedArrayBuffer.

Texturas em materiais 3D, shaders customizados, sombras, iluminação avançada, render targets, touch, gamepad, vídeo, física 3D e persistência não fazem parte deste marco. WebGPU requer navegador/device compatíveis e localhost ou HTTPS. Não há fallback automático para WebGL.

## Preparação

Dependências: .NET 9, CMake 3.26+, Ninja, Python, Git e Node para testes. Checkout da engine: `C:\dev\helworks\helengine`; plataforma: `C:\dev\helworks\helengine-webgpu`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\setup-toolchain.ps1
dotnet build C:\dev\helworks\helengine-webgpu\builder\helengine.webgpu.builder.csproj
```

A SDK fica em `C:\dev\helworks\tools\emsdk`. `HELENGINE_EMSDK_ROOT` permite indicar outra instalação da mesma versão. `HelEngineRoot` é uma propriedade MSBuild para indicar outro checkout da engine. `HELENGINE_WEBGPU_REPOSITORY_ROOT` permite localizar explicitamente os fontes da plataforma.

O registro local está em `C:\dev\helworks\helengine\user_settings\platforms.json`. A entrada `webgpu` aponta para `C:\dev\helworks\helengine-webgpu\builder\bin\Debug\net9.0\helengine.webgpu.builder.dll`, os fontes desta plataforma e `C:\dev\helworks\helengine-webgpu\platform-plugin.json`. A versão deve satisfazer a versão da engine exigida pelo projeto. A plataforma também precisa constar nas configurações do projeto; o DemoDisc já está configurado com apenas o Cube Test para esse destino.

## Exportar

```powershell
dotnet run --project C:\dev\helworks\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helworks\builds\helengine-webgpu\cube-test\player --require index.html --require helengine_webgpu.js --require helengine_webgpu.wasm --require helengine_webgpu.data -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform webgpu -Output C:\dev\helworks\builds\helengine-webgpu\cube-test\player -BuildProfile release -CacheRoot C:\dev\helworks\builds\helengine-webgpu\cache
```

O pacote inclui `index.html`, `bootstrap.js`, `audio.js`, `helengine_webgpu.js`, `helengine_webgpu.wasm`, `helengine_webgpu.data` e o manifest de diagnóstico. Hospede todos juntos. URLs relativas permitem publicar em subdiretório. O builder só publica após verificar os artefatos, mantém logs de configure/build no cache nativo e limpa apenas sua árvore de conteúdo preparado entre execuções.

## Executar

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\serve.ps1 -Root C:\dev\helworks\builds\helengine-webgpu\cube-test\player -Port 8198
```

Abra <http://127.0.0.1:8198/>. Clique no canvas para controlar a câmera: A/D, W/S, Q/E. O botão de áudio desbloqueia o contexto Web Audio. Encerre o servidor com Ctrl+C.

## Verificação

```powershell
dotnet test C:\dev\helworks\helengine-webgpu\builder.tests\helengine.webgpu.builder.tests.csproj
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\build-device-tests.ps1
Set-Location C:\dev\helworks\helengine-webgpu\tests\browser
npm ci
npm test
```

Os testes de navegador usam Chrome com WebGPU real e o player exportado. Falta de adapter compatível falha explicitamente. A prova de desenho lê pixels numericamente pela GPU, sem screenshots. O teste de aba oculta simula `visibilitychange` e a suspensão de RAF, pois Chrome headless mantém abas visíveis. Os arquivos de evidência ficam em `C:\dev\helworks\builds\helengine-webgpu\cube-test`.

Aceite em 2026-10-14: 20 testes gerenciados e 27 de navegador passaram, com adapter NVIDIA Lovelace sem fallback. Os perfis debug/release compilaram. A repetição pelo wrapper/Build Waiter confirmou publicação fresca e reutilização de 11/13 objetos nativos; dois foram recompilados para o ajuste de DPI. O fluxo compartilhado ainda executa geração e verificações do editor. Relatório completo: `C:\dev\helworks\builds\helengine-webgpu\cube-test\validation.md`.
