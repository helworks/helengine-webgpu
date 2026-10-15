# DemoDisc no navegador: shaders pelo pipeline compartilhado

Data: 2026-10-14
Estado: proposta escrita para revisão de Helena; implementação ainda não iniciada.

## Objetivo e decisões recebidas

Helena quer o DemoDisc inteiro no navegador, executando os scripts existentes pelo caminho C# → C++ → WebAssembly. A plataforma escolhida é WebGPU. Helena aprovou um formato de material próprio do backend e pediu expressamente o mesmo esquema de build de shaders usado por Windows, PS3 e Xbox 360.

O escopo de conteúdo proposto é o menu desktop e suas demos/jogos: 37 das 39 cenas existentes. As duas variantes exclusivas de portáteis ficam fora dessa seleção. Essa seleção ainda é uma suposição comunicada a Helena. Não usar a seleção Windows atual como inventário completo: ela contém somente 23 cenas.

Este documento especifica a primeira etapa necessária: conectar materiais e shaders WebGPU aos contratos compartilhados. A entrega do DemoDisc completo também exige renderização PBR/sombras, transições, física, jogos e o ray tracer por software; concluir esta etapa não representa concluir as 37 cenas.

## Evidência no código atual

- O builder WebGPU publica materiais básicos como `PlatformMaterialAsset` e não reporta dependências de shaders. Esse asset não representa roughness, metallic, specular e os demais dados PBR necessários.
- O build WebGPU copia dois arquivos WGSL mantidos pela plataforma. Os renderizadores carregam esses nomes fixos. Esse é o caminho do Cube Test, que será substituído pelo caminho de artefatos compilados.
- O Windows informa `PlatformShaderDependency` contendo shader asset, programas de vértice/pixel e variante. Seu backend DirectX 11 implementa o contrato compartilhado `IShaderBackend`.
- PS3 e Xbox 360 implementam `IPlatformShaderArtifactBuilder`; seus cookers recebem snapshots de fontes e dependências explícitas e declaram cada artefato produzido. Seus builders também integram o registro de backends.
- A engine já oferece `CookedPlatformOwned` e `BuildMaterialFromCooked(path, source)` para payloads opacos. O empacotador escreve os bytes devolvidos pelo cooker sem exigir `PlatformMaterialAsset`.
- `ShaderCompileTarget` e `ShaderTargetDescriptors` ainda não têm WebGPU. Acrescentar um target preservando os valores existentes é necessário para o registro compartilhado.
- O índice de código da engine é de 2026-09-23; os contratos relevantes foram conferidos diretamente no código atual após o sinal de metadados alterados.

## Abordagem proposta

Usar os shaders HLSL originais pelo serviço compartilhado e acrescentar um backend WebGPU. O material seleciona a mesma identidade lógica de shader, programas e variante. O cooker publica um artefato WebGPU versionado, contendo WGSL traduzido e os metadados necessários ao runtime. O renderer cria seus módulos, layouts e pipelines a partir desse artefato.

A tradução proposta é HLSL → SPIR-V por shaderc → WGSL por Tint. O PS3 já usa shaderc para o frontend HLSL; WebGPU terá seu adaptador próprio, sem depender do builder PS3. Versões e hashes das ferramentas entram na identidade da compilação. A instalação deve fixar uma revisão concreta antes de gerar artefatos reutilizáveis.

Alternativas consideradas: manter WGSL escrito à mão duplicaria os shaders e não atenderia ao pedido; introduzir outro frontend HLSL é possível, mas acrescenta uma toolchain diferente da já usada pela engine. A proposta usa shaderc e Tint; sua compatibilidade com as fontes reais deve ser demonstrada antes de declarar o pipeline disponível.

No navegador, WGSL é entregue à API WebGPU. A compilação final para a GPU pertence ao navegador/driver; o build faz tradução, validação e preparação dos metadados. Não existe promessa de distribuir bytecode de GPU portátil para browsers.

## Integração com a engine

1. Acrescentar `ShaderCompileTarget.WebGpu` ao final da enumeração, com identificador explícito, descriptor `webgpu` e define `HEL_API_WEBGPU`. Preservar os identificadores persistidos das demais plataformas.
2. Implementar `IShaderBackend` e `IShaderBackendRegistryContributor` no plugin WebGPU. A compilação deve receber fonte, entry point, stage, variante, defines, opções e resolver de includes pelo contrato existente.
3. Implementar `IPlatformShaderArtifactBuilder`. Receber `PlatformShaderArtifactCookRequest`, compilar seleções distintas e devolver `PlatformCookedArtifactDeclaration` para todos os arquivos publicados.
4. Implementar `IPlatformRendererShaderDependencyProvider` para os shaders de UI e passes que pertencem ao renderer. As dependências de materiais vêm de `PlatformMaterialCookResult`.
5. Aproveitar snapshots e identidades fornecidos pelo editor. Reutilizar o catálogo de variantes de `ForwardStandardShader` e `SpriteBatchShader`. Seleções customizadas usam a definição real do asset; não inferir entry points ou variantes ausentes.
6. O build deve empacotar os artefatos pelo manifest. Remover a cópia de WGSL fixo do caminho de produção após migrar ambos os renderizadores.

## Frontend, reflection e bindings

Includes devem passar pelo resolver recebido, com registro das dependências efetivamente usadas e diagnósticos com caminho e linha. Defines de plataforma e variante passam pelo fluxo compartilhado. Cache deve considerar fontes, includes, opções, versões das ferramentas e versão dos formatos; desativá-lo explicitamente enquanto a identidade completa não estiver implementada.

Reflection deve partir da representação compilada e reconciliar vértice/pixel antes da publicação. Registrar entry points finais, localizações de entradas/saídas, recursos, visibilidade por stage, tamanhos de buffers e offsets de membros. O runtime não deve deduzir o layout com regex sobre WGSL.

Usar a `ShaderBindingPolicy` recebida, mantendo o esquema compartilhado: register space corresponde ao bind group e `GetSlot` fornece o binding. Na política padrão, space/grupo 0 usa buffers em 0+, texturas em 100+ e samplers em 200+. Aplicar o mapeamento durante a compilação/transformação estruturada do shader. Detectar colisões, layouts incompatíveis e recursos não representáveis antes de publicar. Na primeira etapa, aceitar apenas register space zero, como os shaders atuais; outros spaces devem falhar explicitamente. Recursos de storage não fazem parte dos shaders de vértice/pixel exigidos nesta etapa e devem receber diagnóstico se solicitados.

Preservar a convenção matemática da engine, sem transposição dupla. A tradução deve registrar o layout efetivo de matrizes e buffers e o runtime deve preencher os dados conforme esse layout. Variações de stride/alinhamento entre HLSL, SPIR-V e WGSL são responsabilidade do adaptador e de seus metadados.

## Artefatos de shader e material

O artefato de shader usa extensão `.wgsp`. Seu cabeçalho tem 28 bytes: magic ASCII `WGSP`, seguido de seis uint32 little-endian: versão (1), tamanho do cabeçalho (28), tamanho total, comprimento dos metadados, comprimento do WGSL de vértice e comprimento do WGSL de fragmento. Seguem três seções UTF-8, sem terminador ou padding, nessa ordem. Limite total: 64 MiB. Leitores devem verificar limites, overflow, UTF-8 e tamanho exato, rejeitando trailing bytes e versões desconhecidas.

Os metadados JSON têm ordem determinística de propriedades e coleções e contêm: `shaderAssetId`, `vertexProgram`, `pixelProgram`, `variant`, `target` (`webgpu`), `toolchain`, `dependencies`, `vertexEntryPoint`, `fragmentEntryPoint`, `vertexInputs`, `vertexOutputs`, `fragmentInputs` e `bindings`. `toolchain` identifica versões/hashes dos executáveis e versão do adaptador; cada dependência tem identidade/caminho e hash do conteúdo. Cada elemento de interface registra nome semântico, índice, location, tipo e interpolação. Cada binding registra nome, classe, register index/space original, group/binding final, stages, dimensão/sample type e layout de buffer quando aplicável. O layout de buffer contém tamanho e membros com nome, offset, tamanho, tipo, array/matrix stride e orientação da matriz. O writer e o reader devem publicar a mesma definição de schema no repositório. Rejeitar inconsistência entre metadados e stages.

O material WebGPU usa extensão `.wgpm` e envelope equivalente de 16 bytes: magic ASCII `WGPM`, versão uint32 (1), tamanho total uint32 e comprimento JSON uint32, todos little-endian. Segue JSON UTF-8 determinístico, limitado a 16 MiB. O JSON contém identidade do material, caminho relativo e seleção explícita do artefato de shader, estado de renderização, constant buffers por nome e valores/referências de texturas necessários aos materiais reais do DemoDisc. Dados binários de constantes usam base64 no JSON; o runtime os reconcilia com o layout compilado antes de preencher buffers. Os mesmos controles de tamanho, UTF-8, versão e trailing bytes se aplicam. Usar `CookedPlatformOwned`; não ampliar `PlatformMaterialAsset` globalmente. Identidade e referências devem vir dos assets/cook requests existentes.

O runtime valida e carrega o material pelo stream source da engine. Os módulos e pipelines WebGPU são compartilhados por identidade e estado compatível, com ownership e liberação explícitos. Um shader ausente ou inválido deve interromper a operação com identificação do asset, variante e stage; não selecionar automaticamente o shader básico do Cube Test.

## Escopo desta etapa

Entregar compilação e consumo reais para os shaders necessários ao Cube Test e à UI desktop, incluindo Textured e RoundedShape do catálogo compartilhado, e preparar os artefatos das variantes PBR/sombras exigidas pelo inventário. A criação de recursos, múltiplas luzes e passes de sombras do renderer será uma etapa subsequente com critérios próprios; compilar seus shaders não comprova renderização desses recursos.

Shaders customizados devem usar o mesmo caminho e receber diagnósticos de recursos incompatíveis com WebGPU. Não prometer suporte irrestrito a HLSL ou a todas as capacidades dos backends de console/Windows.

## Continuidade até o DemoDisc completo

Depois desta etapa: implementar texturas e buffers de materiais, iluminação/PBR e passes de sombras conforme os dados originais; integrar WebGPU ao grupo HD do projeto após confirmar a seleção desktop; exercitar splash/menu/carregamento e transições aditivas; compilar os módulos DemoDisc, TiltPlay, Zombislayer e Raytracing; integrar a física selecionada pelos componentes e manifest da engine; preservar o ray tracer por software e seus uploads de textura.

Uma falha de codegen deve ser corrigida no produtor, nunca nos arquivos gerados. Troca do backend de física, alteração do modelo de execução ou redução de conteúdo exigem feedback arquitetural de Helena. O objetivo final continua sendo abrir todas as cenas selecionadas pelo menu, com controles, carregamento e retorno funcionando.

## Aceitação proposta

- O build oficial declara artefatos WebGPU derivados dos HLSL originais e não depende dos dois WGSL fixos do protótipo.
- Erros de HLSL, include, tradução, linking e schema produzem falha identificável; publicação não expõe um par parcialmente compilado.
- Alterar fonte, include, variante ou toolchain invalida o artefato correspondente.
- O player cria módulos/pipelines a partir dos artefatos e mantém o Cube Test, texto, sprites, clipping e formas arredondadas.
- Uma seleção de shader customizado percorre o mesmo contrato e preserva sua identidade.
- O material PBR mantém seus dados na passagem cooker → pacote → runtime; a paridade visual completa é aceita na etapa do renderer.
- A execução final das 37 cenas será registrada separadamente. Não usar a aceitação anterior do Cube Test como evidência dessa expansão.

Esta etapa ainda não executou testes novos. A aprovação do plano de implementação deve incluir as validações a executar. Capturas de tela continuam fora do fluxo sem autorização explícita de Helena.

## Referências

- Especificação WGSL: https://www.w3.org/TR/WGSL/
- Tint e suporte a reader SPIR-V/writer WGSL: https://dawn.googlesource.com/tint/+/94a5fd476e4e5206f74325d69a7bcabfae37b56e/src/reader/spirv
- Inventário local: `C:\dev\helworks\builds\helengine-webgpu\full-demodisc\scene-inventory.json` (39 cenas lidas pelo serializador oficial, zero erros; inventário bruto, anterior à expansão de blueprints e ao cooking).
