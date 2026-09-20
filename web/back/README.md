# Backend — Sistema de Irrigação Inteligente

API REST (Node.js + Express + PostgreSQL) responsável por receber as leituras dos
sensores (enviadas pelo ESP32) e servir os dados para o frontend.

## Requisitos

- Node.js 18+
- PostgreSQL em execução (local via pgAdmin, ou via Docker)
- `.env` configurado na raiz do projeto (ver `.env.example`)

## Configuração

As variáveis de ambiente são lidas do `.env` na raiz do repositório:

| Variável       | Descrição                              |
| -------------- | -------------------------------------- |
| `PORT`         | Porta da API (padrão `8080`)           |
| `JWT_SECRET`   | Segredo usado para assinar os tokens JWT |
| `DATABASE_URL` | String de conexão com o PostgreSQL     |

## Execução

```bash
# dentro de web/back
npm install

# cria/atualiza as tabelas (idempotente)
npm run db:init

# inicia a API
npm start
```

A API fica disponível em `http://localhost:8080`.

## Docker

```bash
# dentro de web/back
docker compose up --build
```

Sobe a API e o banco PostgreSQL juntos.

---

## Autenticação

Existem dois mecanismos de autenticação:

- **JWT (usuário):** usado pelo frontend. Obtido via `POST /api/auth/login` e
  enviado no header `Authorization: Bearer <token>`.
- **API key (dispositivo):** identifica o ESP32 que envia leituras. A chave é
  gerada no cadastro do dispositivo (`POST /api/devices`) e enviada no header
  `X-Api-Key: <api_key>`.

## Estrutura de payload (leitura dos sensores)

O ESP32 envia uma leitura para `POST /api/readings`:

```json
{
  "temperatura": 26.4,
  "umidade": 58,
  "umidade_solo": 40,
  "timestamp": "2026-08-31T10:00:00Z"
}
```

Regras:

- `temperatura`, `umidade` e `umidade_solo` são numéricos e opcionais, porém
  **ao menos um** deve ser informado.
- `umidade` e `umidade_solo` devem estar entre `0` e `100`.
- `timestamp` é opcional; se omitido, o servidor usa a data/hora atual.

---

## Endpoints

### Autenticação

| Método | Rota                     | Auth | Descrição                        |
| ------ | ------------------------ | ---- | -------------------------------- |
| POST   | `/api/auth/register`     | —    | Cria um usuário                  |
| POST   | `/api/auth/login`        | —    | Autentica e retorna token JWT    |
| POST   | `/api/auth/change-password` | JWT | Altera a senha do usuário logado |

Body de `register`:

```json
{ "nome": "Ricardo", "email": "ricardo@exemplo.com", "senha": "senha1234" }
```

Body de `change-password`:

```json
{ "senha_atual": "senha1234", "nova_senha": "novaSenha456" }
```

### Dispositivos

| Método | Rota             | Auth | Descrição                                  |
| ------ | ---------------- | ---- | ------------------------------------------ |
| POST   | `/api/devices`   | JWT  | Cadastra um dispositivo e gera `api_key`   |
| GET    | `/api/devices`   | JWT  | Lista os dispositivos do usuário           |
| DELETE | `/api/devices/:id` | JWT | Remove um dispositivo do usuário          |

Body de criação:

```json
{ "nome": "ESP32 - Canteiro 1" }
```

Resposta de criação (guarde a `api_key`):

```json
{ "id": 1, "usuario_id": 1, "nome": "ESP32 - Canteiro 1",
  "api_key": "5f2c...", "criado_em": "2026-08-31T10:00:00.000Z" }
```

### Leituras

| Método | Rota                | Auth    | Descrição                                    |
| ------ | ------------------- | ------- | -------------------------------------------- |
| POST   | `/api/readings`     | API key | Ingestão de leitura enviada pelo ESP32       |
| GET    | `/api/readings/latest` | JWT  | Última leitura de um dispositivo             |
| GET    | `/api/readings`     | JWT     | Histórico de leituras (com filtros)          |

Parâmetros de `GET /api/readings` (query string):

| Parâmetro        | Descrição                                     | Padrão |
| ---------------- | --------------------------------------------- | ------ |
| `dispositivo_id` | Obrigatório. ID do dispositivo                | —      |
| `limit`          | Máximo de registros (1–1000)                  | `100`  |
| `offset`         | Deslocamento para paginação                   | `0`    |
| `from`           | Data inicial (ISO 8601)                       | —      |
| `to`             | Data final (ISO 8601)                         | —      |

Exemplo:

```
GET /api/readings?dispositivo_id=1&limit=50&from=2026-08-01T00:00:00Z
```

`GET /api/readings/latest` aceita apenas `dispositivo_id`.

---

## Erros

Em caso de erro, a API retorna status HTTP 4xx/5xx com o corpo:

```json
{ "error": "Descrição do erro" }
```
