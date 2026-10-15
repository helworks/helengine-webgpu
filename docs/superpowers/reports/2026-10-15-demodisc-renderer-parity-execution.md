# DemoDisc WebGPU — estado da implementação

## Escopo

Integração do builder WebGPU em `C:\dev\helworks\helengine-webgpu`, com as 37 cenas desktop do projeto `C:\dev\helprojs\demodisc`. Scripts C# seguem pelo gerador C++ oficial e WASM; HLSL segue por shaderc/SPIR-V/Tint e artefatos WGSP/WGPM.

A pedido de Helena, somente este repositório será colocado na `main` nesta integração. As mudanças da engine e do projeto continuam em seus respectivos repositórios e branches.

## Implementado neste repositório

- Luz ambiente, direcional, spot e pontual; quatro slots diretos e seleção de até 32 luzes visíveis.
- Atlas de sombras e cubemaps de profundidade radial, com shaders originais declarados no cook.
- Render targets, múltiplas câmeras, materiais por submesh e shader original de material ausente.
- Crescimento do buffer de uniforms entre frames e renovação dos bind groups.
- Gamepad padrão do navegador e observação dos módulos, cenas e física selecionada.
- Preparo idempotente dos 77 materiais WebGPU, galeria PBR com 25 combinações e seleção HD/37 cenas.
- Navegação automatizada pelas cenas originais e script de verificação da exportação oficial.

## Evidências disponíveis

Resultados em `C:\dev\helworks\builds\helengine-webgpu\renderer-parity`:

| Verificação | Resultado | Arquivo |
| --- | --- | --- |
| Suíte gerenciada do builder | 93 passaram | `managed-isolated-final.log` |
| Configuração do servidor e seleção do shader corrompido | 5 passaram | `corruption-selection-green.log` |
| Planejamento nativo de luzes, sombras e materiais | Passou | `light-submesh-final.log` |
| Auditoria de conteúdo | 37 cenas, 77 materiais, zero mudanças propostas | `content-idempotence-final.log` |
| Regressão prévia de Cube, materiais, UI e entrada | 15 passaram, 1 falhou | `browser-preflight.log` |

A falha do teste negativo de WGSL ocorria porque ele corrompia o primeiro shader do pacote, que podia pertencer a uma cena ainda não carregada. O helper agora seleciona o shader Textured de SpriteBatch usado na inicialização; as duas regressões Node passaram. A repetição desse caso no navegador está pendente.

## Aceite pendente

A exportação oficial mais recente concluída antes das últimas correções foi a quinta (`full-export-fifth.log`). Ela contém as 37 cenas e quatro módulos. As correções posteriores de materiais ausentes, referências Tilt e trajetórias HelPhysics ainda exigem a conclusão de uma nova exportação e o aceite conjunto no navegador.

Não há declaração de 37/37 cenas aprovadas nesta integração. A suíte completa, os ciclos de descarte e a geração de `verification-summary.json` continuam pendentes.

Os layouts originais character steps, mesh ground stability, render-only slope e trigger volume não contêm os componentes físicos sugeridos pelos nomes. Os meshes de Zombislayer foram salvos sem materiais e usam o shader magenta de material ausente, como no desktop.

## Dependências em outros repositórios

- `C:\dev\helworks\helengine`: branch `feat/webgpu-shared-shaders`, incluindo os contratos de shaders e o suporte cinemático HelPhysics.
- `C:\dev\helprojs\demodisc`: branch `feat/webgpu-demodisc`, incluindo os assets, menu, ray tracer e referências das cinco fases Tilt.

Esses repositórios não são integrados à `main` por esta operação. Screenshots, vídeos e traces permanecem desativados.
