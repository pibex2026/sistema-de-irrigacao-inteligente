# Documentação Técnica — Sistema de Irrigação Inteligente (PIBEX 2026)

Índice central dos documentos em `docs/` organizados por **contexto e assunto**:

## 1. Arquitetura & Firmware (`docs/architecture/`)
| Documento | Contexto / Assunto |
| :--- | :--- |
| **[firmware-espidf.md](architecture/firmware-espidf.md)** | Arquitetura do firmware ESP32 em **PlatformIO + ESP-IDF v5.5 (C++17 + FreeRTOS Dual-Core)**, drivers nativos, FSM, LittleFS, NVS, rede assíncrona e orçamento de memória. |
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
