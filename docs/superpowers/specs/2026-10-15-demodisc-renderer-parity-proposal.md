# DemoDisc WebGPU completo — proposta de continuidade

Estado: aprovada por Helena em 2026-10-15 ("vai"). Escolha aprovada: depth textures e samplers de comparação nativos do WebGPU. A implementação desta etapa ainda não começou.

## Resultado já concreto

O shader builder compartilhado e o consumo WGSP/WGPM estão implementados. A revisão independente teve suas correções verificadas: 77 testes gerenciados, 31 de navegador, leitores/packing nativos e export oficial release. Preview desta etapa: http://127.0.0.1:8199/. A seleção WebGPU continua no Cube.

Relatório: `C:\dev\helworks\helengine-webgpu\docs\superpowers\reports\2026-10-14-demodisc-shared-shaders-execution.md`.

## Conteúdo a entregar

Manter o alvo desktop proposto: 37 das 39 cenas, excluindo apenas `DemoDiscMainMenuHandheld` e `games/tilt/tilt_trial_ds`. O inventário se divide em 3 cenas de inicialização/menu/carregamento, 7 de jogos, 12 de física, 14 de rendering e 1 benchmark. A seleção Windows atual, com 23 cenas, não substitui esse inventário.

Inventário completo: `C:\dev\helworks\builds\helengine-webgpu\full-demodisc\scene-inventory.json`. Projeto: `C:\dev\helprojs\demodisc\project.heproj`.

## Integração proposta

1. Preparar os overrides WebGPU com os valores desktop efetivamente autorados e incluir o destino no grupo HD. Não substituir os parâmetros PBR por defaults; os 25 materiais da galeria já demonstraram a necessidade dessa migração. Preservar Windows e as demais plataformas.
2. Completar os recursos do renderer exigidos pelo conteúdo, aproveitando os contratos e algoritmos existentes da engine: seleção das luzes, buffers ForwardLight/Shadow, materiais/texturas, culling, transparência, targets e passes de sombras. Manter os shaders originais no mesmo builder compartilhado.
3. Usar o registro de física e os módulos C#→C++ selecionados pelo projeto/manifest. A etapa não autoriza trocar o backend de física nem alterar o modelo de execução. Qualquer incompatibilidade que force essa troca volta para feedback arquitetural.
4. Empacotar os módulos DemoDisc, TiltPlay, Zombislayer e Raytracing e exercitar splash→menu→loading→cada demo/jogo→retorno. Preservar o ray tracer por software e os uploads de textura que ele produz.
5. Só ampliar a publicidade das capacidades do backend conforme os respectivos recursos forem implementados e validados. Um requisito não suportado permanece erro identificável durante a integração.

## Escolha arquitetural aprovada: sombras

O HLSL atual lê os mapas como texturas float comuns e faz a comparação de profundidade no shader. No DirectX11, o recurso é R32 typeless, com view de escrita D32_Float e view de leitura R32_Float. Essa combinação não deve ser copiada literalmente como um par de views WebGPU sem reconciliar o tipo do shader e do binding.

Fontes locais:

- `C:\dev\helworks\helengine\engine\helengine.directx11\rendering\DirectX11ShadowAtlasResources.cs`
- `C:\dev\helworks\helengine\engine\helengine.directx11\rendering\DirectX11PointShadowCubeResources.cs`
- `C:\dev\helworks\helengine\engine\helengine.editor\shaders\builtin\ForwardStandardShader.hlsl`

### Opção proposta: depth textures e comparação nativas

Usar atlas/cubemaps de profundidade e samplers de comparação WebGPU. Adaptar os pontos necessários no HLSL por `HEL_API_WEBGPU`, preservando os contratos e fontes das outras plataformas. O builder continua produzindo WGSL pelo caminho shaderc/SPIR-V/Tint; a reflexão e o runtime passam a aceitar os recursos de profundidade/comparação necessários. Bias, PCF, projeção e seleção das luzes precisam ser conferidos contra o comportamento desktop.

O WGSL define depth textures 2D/cube e samplers de comparação; o tipo do recurso ligado deve corresponder ao shader. Referência oficial: [WGSL, tipos de textura de profundidade](https://gpuweb.github.io/gpuweb/wgsl/#depth-texture-type).

### Alternativa: profundidade em textura float de cor

Manter as leituras float e a comparação no HLSL, produzindo um mapa de profundidade em um attachment de cor R32Float com o passe correspondente. Essa opção exige definir a escrita da profundidade explicitamente e validar a compatibilidade de amostragem/layout; não basta reaproveitar o resultado de cor de `ShadowDepthPS` como se fosse a profundidade do depth attachment.

A primeira opção foi aprovada por Helena em 2026-10-15. A alternativa de textura float de cor não foi escolhida.

### Precisão obtida no mapeamento para implementação

O atlas DirectX11 usa depth storage com view float. O cubemap DirectX11 tem um attachment de cor separado que recebe distância radial normalizada, além do depth attachment usado na rasterização (`EditorPointShadowDepth.hlsl` e `DirectX11PointShadowCubeResources.cs`). A descrição acima do recurso typeless se aplica ao atlas; não descreve a codificação radial do cubemap. No caminho WebGPU aprovado, o fragment shader do passe pontual escreve essa mesma distância radial em `SV_Depth`, para que a leitura com comparação do cubemap preserve a semântica existente. Atlas direcional/spot continua usando profundidade projetada. Os testes devem conferir ambas as codificações e as seis faces, sem tratar os dois passes como equivalentes.

## Critérios para concluir o DemoDisc

- Manifest declara as 37 cenas e todos os assets/módulos exigidos após expansão de blueprints.
- Splash, loading, menu, carregamentos aditivos, descarregamento e retorno funcionam pelo fluxo real.
- Cada demo/jogo mantém controles desktop e comportamento esperado; cenas físicas usam o backend selecionado.
- Galerias PBR/texturizadas e cenas de sombras têm seus recursos ativos, não apenas shaders compilados.
- Demos de ray tracing por software executam os scripts originais e exibem seus uploads reais.
- Leitura numérica de pixels, estado do runtime e testes de fluxo comprovam os resultados; screenshots permanecem desautorizados.
- Falhas de conversão são corrigidas no produtor, nunca nos arquivos gerados. O build oficial e o Build Waiter comprovam publicação fresca do pacote completo.

Plano detalhado: `C:\dev\helworks\helengine-webgpu\docs\superpowers\plans\2026-10-15-demodisc-renderer-parity.md`. Esta proposta não muda o escopo para apenas as cenas que já rodam.
