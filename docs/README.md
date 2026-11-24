# Documentação Técnica — Sistema de Irrigação Inteligente (PIBEX 2026)

Índice central dos documentos em `docs/` organizados por **contexto e assunto**:

## 1. Arquitetura & Firmware (`docs/architecture/`)
| Documento | Contexto / Assunto |
| :--- | :--- |
| **[firmware-espidf.md](architecture/firmware-espidf.md)** | Arquitetura do firmware ESP32 em **PlatformIO + ESP-IDF v5.5 (C++17 + FreeRTOS SMP `tskNO_AFFINITY`)**, drivers nativos, FSM, LittleFS, NVS, rede assíncrona e orçamento de memória. |
| **[embedded-web-api.md](architecture/embedded-web-api.md)** | Servidor HTTP embarcado (`esp_http_server` + mDNS `irrigacao.local`), interface multi-abas gzip (`view/index.html`) e contrato dos endpoints JSON/CSV (`/api/status`, `/api/performance`, `/api/system`, `/api/config`, etc.). |

## 2. Inteligência Artificial & TinyML (`docs/ai/`)
| Documento | Contexto / Assunto |
| :--- | :--- |
| **[tinyml-pipeline.md](ai/tinyml-pipeline.md)** | Especificação do modelo **PhysMLP-27 Full INT8 (`5.480 B`)**, engenharia psicrométrica (VPD), *Full-8 Context Dropout*, *Golden Vectors* compiláveis, fallback físico e *fine-tuning* com CSVs de campo. |
| **[audit-and-benchmark.md](ai/audit-and-benchmark.md)** | Auditoria forense do pipeline v1, diagnóstico dos 9 bugs (vazamento de tuplas, `class_weight`, falsa quantização) e benchmark experimental de 10 arquiteturas + 12 pranchas gráficas. |

## 3. Pesquisa & Dados (`docs/research/`)
| Documento | Contexto / Assunto |
| :--- | :--- |
| **[datasets-summary.md](research/datasets-summary.md)** | Resumo comparativo dos datasets avaliados e justificativa da escolha do *Smart Agriculture Dataset*. |
| **[datasets-evaluation.md](research/datasets-evaluation.md)** | Análise estatística detalhada, prova por hash MD5 e resolução da Classe 2 (`THERMAL_STRESS_WET_SOIL`). |
| **[articles.md](research/articles.md)** | Levantamento bibliográfico e referências científicas do projeto. |

## 4. Planejamento & Execução (`docs/planning/`)
| Documento | Contexto / Assunto |
| :--- | :--- |
| **[action-plan.md](planning/action-plan.md)** | Plano de ação mestre, checklist de execução por fases e protocolo de 8 testes de aceitação em bancada física. |
| **[todo.md](planning/todo.md)** | Lista detalhada de tarefas técnicas (Embarcado, IA e Web). |
| **[checklists.md](planning/checklists.md)** | Checklist macro de execução das entregas do PIBEX 2026. |
| **[gap-analysis.md](gap-analysis.md)** | Auditoria estática de lacunas funcionais (21 gaps identificados no commit `93ee4c1`). |
| **[resumo-ia-e-firmware.md](resumo-ia-e-firmware.md)** | Resumo estruturado das tecnologias e estratégias de TinyML e Firmware ESP32. |

## 5. Evolução, Etapas & Viradas de Arquitetura (`docs/evolution/`)
| Documento | Contexto / Assunto |
| :--- | :--- |
| **[evolution/README.md](evolution/README.md)** | Índice da série de evolução (`84` commits, `96e0cc2..3c5ac4c`) e quadro-síntese do ecossistema. |
| **[01-timeline-and-phases.md](evolution/01-timeline-and-phases.md)** | Linha do tempo cronológica, 6 fases de desenvolvimento e mapa de commits. |
| **[02-architectural-pivots.md](evolution/02-architectural-pivots.md)** | 10 Viradas de Arquitetura (ADRs retrospectivos) com antes, depois e impacto. |
| **[03-execution-points-and-flows.md](evolution/03-execution-points-and-flows.md)** | Pontos de entrada, topologia FreeRTOS SMP, FSM, fluxo E2E e comandos de build/testes. |
| **[04-implementation-map.md](evolution/04-implementation-map.md)** | Mapa detalhado de implementação por camada (`embedded/`, `ai-model/`, `web/`, Docker/CI). |
| **[05-maturity-gaps-and-roadmap.md](evolution/05-maturity-gaps-and-roadmap.md)** | Matriz de maturidade, rastreamento dos 21 gaps, dívidas técnicas e roadmap. |
