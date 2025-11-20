# Sugestões para Organização dos Menus ImGui

Estas ideias ajudam a deixar o GtaNavViewer mais limpo, focado no fluxo de trabalho e com menos atrito para operações repetidas.

## Estrutura geral
- **Barra superior com submenus claros**: "Arquivo" (carregar/salvar cenas), "Visualização" (debug draw e overlays), "Navmesh" (build/auto-build) e "Ajuda" (atalhos, infos de câmera). Mantém ações globais separadas das de edição.
- **Janela lateral principal**: concentrar menus de edição (Meshes e Navmesh) num painel lateral fixo, liberando o resto da tela para a visualização 3D.
- **Colapsáveis e tabs**: usar `ImGui::CollapsingHeader` para grupos amplos (ex.: Meshes, Navmesh, Debug) e `ImGui::BeginTabBar` para alternar modos (Edição vs. Diagnóstico), evitando listas longas sempre abertas.

## Mesh Menu
- **Lista de instâncias em tabela**: apresentar cada mesh em linhas com colunas para nome, visibilidade e botões de seleção/remoção. Facilita ler vários itens de uma vez.
- **Editor compacto por item**: expandir detalhes de um item ao clicar/selecionar; dentro, sliders de posição/rotação, cor de debug e botão de focar câmera. Mantém a lista curta por padrão.
- **Filtros e ordenação**: campo de busca por nome e ordenação por nome/data de carga para achar meshes rapidamente em cenas grandes.
- **Agrupamento por origem**: permitir agrupar meshes pelo diretório/arquivo de origem ou tags (ex.: "interior", "exterior"), exibindo grupos colapsáveis.

## Navmesh Menu
- **Seções separadas**: 1) Configuração de build (parâmetros Recast/Detour), 2) Construção Automática (checkboxes/bitmask), 3) Ferramentas de depuração (draw flags, pick info). Cada seção em colapsável.
- **Preset rápido de parâmetros**: botões para "Rápido", "Padrão" e "Detalhado" preenchendo valores comuns antes de permitir ajuste fino.
- **Histórico de builds**: mostrar o último tempo de build, quantidade de polígonos e origem das meshes usadas; útil para saber se é preciso rebuild.

## Visibilidade e acessibilidade
- **Estado persistente**: lembrar quais colapsáveis estavam abertos e quais filtros estavam ativos para não reconfigurar a cada run.
- **Hotkeys contextuais**: atalhos para abrir/fechar painel lateral, focar mesh selecionada e toggles rápidos de debug (ex.: `F1` para wireframe, `F2` para áreas).
- **Indicação de seleção**: destacar a mesh selecionada na lista e no viewport (outline/cor), evitando confusão em cenas com muitas instâncias.

## Escalabilidade
- **Paginação/virtualização**: em cenas com muitas meshes, usar `ImGuiListClipper` para não renderizar todas as linhas simultaneamente.
- **Seções de ajuda inline**: `ImGui::TextDisabled("(?)")` com tooltips curtos explicando o efeito de cada opção, principalmente parâmetros de build.
- **Modo minimalista**: permitir esconder opções avançadas, exibindo apenas toggles essenciais para navegação rápida durante demonstrações.

## Workflow sugerido
1. Carregar várias meshes via submenu Arquivo.
2. Ajustar posição/rotação da mesh selecionada no painel Meshes (detalhes expandidos).
3. Ativar apenas os gatilhos automáticos desejados em "Construção Automática" no Navmesh Menu.
4. Usar presets de build e depuração em tabs "Edição"/"Diagnóstico" conforme necessidade.

## Ideias futuras
- **Snapshots de layout**: salvar/recuperar disposição das janelas e estados de colapsáveis.
- **Logs integrados**: janela "Console" com filtros por categoria (carregamento, build, erros) e timestamps para diagnósticos rápidos.
- **Perfis de usuário**: diferentes perfis de UI ("Artista", "Técnico", "Apresentação") alternando visibilidade de painéis e detalhes.
