# DemoDisc WebGPU — execução e evidências

**Aceite concluído: 37/37 cenas desktop e 89/89 testes de navegador passaram, sem falhas, skips ou flaky.** Auditoria independente do recibo em `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\acceptance-audit.json`.

## Escopo e integração

O builder em `C:\dev\helworks\helengine-webgpu` exporta as 37 cenas desktop de `C:\dev\helprojs\demodisc`, com os módulos originais DemoDisc, Raytracing, TiltPlay e Zombislayer. As duas variantes handheld ficam fora desta seleção.

Scripts C# seguem pelo gerador C++ oficial e Emscripten/WASM. Shaders HLSL seguem por shaderc/SPIR-V e Tint fixado em `31e25af254ab572c77054edec4946d2244e184dd`, produzindo WGSP/WGPM. Não foram editados C++ ou WGSL gerados. A física continua sendo a HelPhysics selecionada pelo projeto, com o solver e o passo fixo existentes.

A pedido de Helena, somente o repositório WebGPU foi integrado à branch local `main`. Dependências usadas na exportação:

| Repositório | Branch | Commit |
| --- | --- | --- |
| `C:\dev\helworks\helengine-webgpu` | `main` | `cfd0ba5` — base; as correções de preload, testes e este relatório estão no commit final da main |
| `C:\dev\helworks\helengine` | `feat/webgpu-shared-shaders` | `080efe29` |
| `C:\dev\helprojs\demodisc` | `feat/webgpu-demodisc` | `4db9dde` |

As alterações alheias à tarefa foram preservadas. As branches da engine e do projeto não foram integradas à `main` por esta operação.

## Implementação

- Luz ambiente, direcional, spot e pontual; seleção de até 32 luzes visíveis e quatro slots diretos.
- Atlas de profundidade direcional/spot e até quatro cubemaps de profundidade radial, usando os shaders originais declarados no cook.
- Render targets da engine, múltiplas câmeras, materiais por submesh, TriangleList/LineList, culling, transparência e profundidade.
- Crescimento do buffer de uniforms entre frames e renovação dos bind groups.
- Shader original de material ausente para meshes sem material atribuído.
- Gamepad padrão do navegador, teclado/mouse, resize/DPI, liberação de controles ao perder foco e Web Audio.
- Preparo idempotente de 77 materiais WebGPU, incluindo as 25 combinações originais PBR; assets HD e 37 cenas.
- Trajetórias cinemáticas originais na HelPhysics, com transformações de hierarquia, velocidade de deslocamento e descarte dos recursos nativos.
- Quatro cenas do ray tracer por software original em 320×240, com amostras progressivas e atualizações reais da textura.

## Exportação oficial

A exportação final concluiu com status `succeeded`, código zero e os quatro artefatos exigidos novos e não vazios:

- Build ID: `1c9e0b54-3c23-4430-a158-f73c2d1d0ba0`.
- Início: `2026-10-15T17:30:47.7587395Z`; conclusão: `2026-10-15T17:37:59.0488295Z`.
- Perfil solicitado: `release`; configuração registrada pelo pipeline oficial: `Debug`.
- Manifest: 37 cenas desktop e os quatro módulos originais.
- WASM: 2.898.299 bytes; data: 63.581.942 bytes.
- O WGPM Standard dentro do pacote contém os flags originais true/true. O SHA256 do data mudou em relação ao pacote anterior, que continha false/false.
- SHA256 do data: `C9434C2230EB4C2899C5E37BB7EE3CB60C731CBA780F8CC527C2BAA7B4FCC837`.
- Log: `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\full-export-eleventh.log`.
- Estado oficial: `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\player\.helengine-build-state.json`.
- Pacote: `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\player`.
- Preview local: <http://127.0.0.1:8200/>.

## Validações

Todas as evidências abaixo ficam em `C:\dev\helworks\builds\helengine-webgpu\renderer-parity`.

| Verificação | Resultado confirmado | Evidência |
| --- | --- | --- |
| Navegador no pacote oficial final | 89 passaram; 37 IDs de cena iguais ao manifest; zero falhas/skips/flaky | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\browser-report.json` |
| Recibo oficial e hashes | 4 artefatos atuais conferidos | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\verification-summary.json` |
| Builder gerenciado | 93 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\verification-managed.log` |
| Configuração do servidor e corrupção do shader necessário no startup | 5 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\verification-server.log` |
| Auditoria de conteúdo | 37 cenas, 77 materiais, 25 combinações PBR, zero mudanças propostas | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\content-audit.json` |
| HelPhysics gerenciada | 309 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\helphysics-final.log` |
| Tilt original, incluindo inicialização após registro da cena | 51 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\tilt-lifecycle-green.log` |
| Ray tracer original | 82 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\ray-platform-green.log` |
| Authoring original | 3 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\desktop-authoring-tests-final.log` |
| Planejamento nativo de luzes/sombras e materiais por submesh | Passou | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\light-submesh-final.log` |
| Harness WASM de GPU/input/audio com os tipos atuais | Compilou e linkou | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\device-harness-final.log` |
| Cinco fases Tilt na nona exportação oficial | 5 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\browser-tilt-ninth.log` |
| Suíte anterior completa na nona exportação | 88 passaram, zero falhas/skips/flaky | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\pre-final-browser-report.json` |
| Preservação dos flags do material Standard gerado | 3 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\standard-shadow-flags-green.log` |
| Dependências reais do preload Emscripten | 5 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\preload-rebuild-final.log` |
| Ciclos de menu, descarte físico, ray uploads, wrappers GPU e triggers originais | 2 passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\browser-lifecycle-strengthened.log` |
| Praça de sombras no pacote oficial final | 1 passou | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\browser-plaza-final.log` |
| Runner PowerShell: stderr com sucesso, saída não zero, exceção e comando ausente | 4 verificações passaram | `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\verifier-command-green.log` |

A rodada fortalecida encontrou dois problemas de naturezas distintas: ausência de draws de sombra na praça e uma expectativa incorreta de identidade física entre cenas. A praça expôs a perda dos flags originais do material Standard durante o cook; a correção dos dois produtores passou em três regressões (gerado, override autoral e schema sem campos). O teste físico agora verifica identidade estável dentro da cena e liberação ao voltar ao menu, conforme o ciclo de vida original.

A décima exportação concluiu, mas expôs uma segunda falha: o material recém-cooked tinha flags true/true e o `.data` ainda continha false/false. O Ninja informou que não havia trabalho porque o preload não era dependência do link. O backend agora rastreia os arquivos e um inventário ordenado com SHA256, incluindo alterações de conteúdo com timestamps preservados pelo cache.

A regressão Emscripten passou em cinco casos: edição, restauração com timestamp antigo, adição com timestamp antigo, remoção e reaproveitamento sem mudanças. O player real refez o link e a praça original passou no pacote de diagnóstico separado. A exportação oficial final concluiu e a praça passou também nesse pacote oficial. A suíte final passou nos 89 testes em 8,5 minutos. O verificador concluiu com código zero, e a auditoria cruzou os 37 casos com o manifest real e reconferiu os quatro hashes dos artefatos.

As fixtures GPU verificam numericamente sombras de atlas e seis faces dos cubemaps, reassociação de slots, materiais texturizados, transparência, render targets e crescimento do buffer de uniforms. Os casos das cenas usam os menus/controles originais, componentes carregados, draws reais de malha/sombra e readbacks da área central. A galeria também exige pelo menos 25 MeshComponents ativos e seus 25 conjuntos de parâmetros são conferidos no cook.

A rodada anterior passou nos 88 testes, mas o runner PowerShell abortou ao interpretar um aviso de cores do Node em stderr como falha. A correção mantém stderr no log e verifica o código de saída real; erros de processo, exceções e comandos ausentes continuam falhando. A regressão reproduziu a falha antes da correção e passou depois.

Nos três ciclos finais, o runtime físico permaneceu estável dentro de cada cena e foi liberado ao voltar ao menu. Após aguardar a fila GPU e executar GC, cada ciclo reteve 3 buffers e 13 texturas. A moeda original foi coletada, o HUD chegou a Coins 1/3 e o runtime emitiu os tipos Enter, Stay e Exit. Os uploads do ray tracer pararam ao retornar ao menu.

## Conteúdo original e limites das evidências

- Physics contém oito cenas com física ativa e quatro layouts originais sem corpos/controladores/triggers: character steps, mesh ground stability, render-only slope e trigger volume. A exportação preserva esses layouts.
- Moving platform contém uma plataforma cinemática e um marcador de spawn; não contém um controlador de personagem ativo.
- Os meshes de Zombislayer foram salvos sem atribuições de material. Usam o shader magenta original de material ausente, como no desktop. Movimento, pausa e retorno continuam nos scripts originais.
- O ModelAsset atual oferece posições, normais e UVs. Nenhum dos 77 materiais auditados exige um stream de cor; materiais que o exigem falham explicitamente. Não foi criado um novo formato de mesh.
- Os eventos de trigger observados são agregados do runtime. Coletar uma moeda e observar Enter/Exit comprova o gameplay original e essas transições; não atribui ambos os eventos ao mesmo par de entidades.
- A contagem após GC mede wrappers JavaScript de buffers/texturas GPU retidos nos ciclos observados. Não mede todas as alocações CPU/GPU nem garante ausência de qualquer vazamento de longa duração.
- A execução usa Chrome com WebGPU disponível. Outros navegadores/devices não foram certificados. O runtime permanece em uma thread, sem exigir SharedArrayBuffer.
- Screenshots, vídeos e traces ficaram desativados; as observações GPU são numéricas.

## Reprodução

`C:\dev\helworks\helengine-webgpu\scripts\verify-demodisc-webgpu.ps1` exporta oficialmente e executa a auditoria e os testes. `-SkipBuild` exige uma exportação oficial concluída e artefatos frescos. As fixtures GPU devem estar compiladas nos diretórios documentados em `C:\dev\helworks\helengine-webgpu\README.md`.

Os resultados completos deste aceite ficam em `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\browser-report.json` e o recibo com hashes dos artefatos em `C:\dev\helworks\builds\helengine-webgpu\renderer-parity\verification-summary.json`.
