# HelEngine WebGPU

Builder de navegador integrado ao fluxo de plataformas da HelEngine. Os scripts C# originais passam pelo gerador C++ e Emscripten/WASM. Os shaders HLSL passam por shaderc/SPIR-V e Tint para gerar artefatos WGSP; materiais usam WGPM.

## DemoDisc desktop

O destino WebGPU seleciona 37 cenas, os módulos DemoDisc, Raytracing, TiltPlay e Zombislayer e os assets HD. As duas variantes handheld ficam fora desta seleção. A validação final está registrada em `C:\dev\helworks\helengine-webgpu\docs\superpowers\reports\2026-10-15-demodisc-renderer-parity-execution.md`.

Aceite local concluído em 2026-10-15: 37/37 cenas e 89/89 testes de navegador passaram no pacote oficial, além dos testes gerenciados, de conteúdo e de builds incrementais.

Recursos implementados:

- Malhas indexadas, UVs, submeshes, TriangleList e LineList; estados de culling, transparência e profundidade dos materiais.
- Materiais sólidos e texturizados, shaders customizados, 25 combinações originais da galeria PBR e 77 overrides WebGPU.
- Luz ambiente, direcional, pontual e spot; seleção de até 32 luzes visíveis e quatro slots diretos.
- Sombras direcionais/spot em atlas de profundidade e até quatro cubemaps de profundidade radial para point lights.
- Render targets da engine, múltiplas câmeras, viewports, clear/load, sprites, atlas de fontes, painéis arredondados e scissor.
- Teclado, mouse, gamepad padrão do navegador, resize/DPI e liberação de entrada ao perder foco.
- HelPhysics selecionada pelo projeto, incluindo trajetórias cinemáticas, contatos, controladores e triggers.
- Quatro cenas com o ray tracer por software original em 320×240.
- PCM16 mono/stereo por Web Audio, com desbloqueio por gesto, loop, parada, ganho e pausa de buses.

Alguns cenários de física originais são layouts de validação com meshes e marcadores, sem corpos/controladores/triggers. A exportação preserva esse conteúdo. Os meshes sem material do Zombislayer usam o shader magenta original de material ausente do desktop.

WebGPU exige navegador/device compatíveis e localhost ou HTTPS. O runtime usa uma thread, sem exigir SharedArrayBuffer. O formato ModelAsset atual oferece posições, normais e UVs; materiais que exigem um stream de cor inexistente falham explicitamente.

## Preparação

Dependências: .NET 9, CMake 3.26+, Ninja, Python, Git e Node para as verificações. A SDK Emscripten fica em `C:\dev\helworks\tools\emsdk`; a ferramenta shader usa a revisão Tint fixada pelo lockfile.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\setup-toolchain.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\setup-shader-toolchain.ps1
dotnet build C:\dev\helworks\helengine-webgpu\builder\helengine.webgpu.builder.csproj
```

O registro local é `C:\dev\helworks\helengine\user_settings\platforms.json`, apontando para `C:\dev\helworks\helengine-webgpu\builder\bin\Debug\net9.0\helengine.webgpu.builder.dll` e `C:\dev\helworks\helengine-webgpu\platform-plugin.json`.

## Preparar o conteúdo

O comando abaixo relata as mudanças propostas. Acrescente `-Apply` para gravar os overrides e a seleção desktop pelos serviços oficiais da engine. Executar novamente deve relatar zero alterações.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\prepare-demodisc-webgpu.ps1 -ProjectRoot C:\dev\helprojs\demodisc
```

## Exportar

```powershell
dotnet run --project C:\dev\helworks\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helworks\builds\helengine-webgpu\renderer-parity\player --require index.html --require helengine_webgpu.js --require helengine_webgpu.wasm --require helengine_webgpu.data -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform webgpu -Output C:\dev\helworks\builds\helengine-webgpu\renderer-parity\player -BuildProfile release -CacheRoot C:\dev\helworks\builds\helengine-webgpu\cache
```

O pacote inclui HTML, bootstrap/audio JavaScript, JavaScript/WASM/data do Emscripten e o manifest de diagnóstico. Hospede todos os arquivos juntos. As URLs relativas permitem publicar em subdiretórios.

## Executar

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\serve.ps1 -Root C:\dev\helworks\builds\helengine-webgpu\renderer-parity\player -Port 8200
```

Abra <http://127.0.0.1:8200/>. Use setas e Enter no menu, ou o gamepad padrão. Os controles de cada demo continuam sendo os scripts originais. Clique no canvas para capturar a entrada; o botão de áudio desbloqueia o Web Audio.

## Verificar

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-webgpu\scripts\verify-demodisc-webgpu.ps1
```

`-SkipBuild` verifica uma exportação oficial já concluída, exigindo estado de sucesso e artefatos frescos. O script executa auditoria de conteúdo, testes do builder, configuração do servidor e toda a suíte de navegador, incluindo as 37 cenas e ciclos de carregamento/descarte. As fixtures GPU devem ter sido compiladas pelos scripts `C:\dev\helworks\helengine-webgpu\scripts\build-*-tests.ps1` com os caminhos de saída usados pela suíte.

Os testes usam Chrome com WebGPU e leem pixels numericamente. Screenshots, vídeos e traces ficam desativados. Falta de adapter falha explicitamente. Evidências e resultados ficam em `C:\dev\helworks\builds\helengine-webgpu\renderer-parity`.

A verificação também cobre builds incrementais do preload: editar, restaurar, adicionar ou remover assets deve atualizar o `.data`, mesmo quando o cache preserva timestamps antigos. O CMake usa as dependências dos arquivos e um inventário por SHA256; conteúdo inalterado pode reaproveitar o pacote.
