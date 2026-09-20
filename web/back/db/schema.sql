-- Sistema de Irrigacao Inteligente - PIBEX 2026
-- Schema do banco de dados (PostgreSQL)

CREATE TABLE IF NOT EXISTS usuarios (
    id        SERIAL PRIMARY KEY,
    nome      VARCHAR(255) NOT NULL,
    email     VARCHAR(255) UNIQUE NOT NULL,
    senha     VARCHAR(255) NOT NULL,
    criado_em TIMESTAMPTZ DEFAULT now() NOT NULL
);

CREATE TABLE IF NOT EXISTS dispositivos (
    id        SERIAL PRIMARY KEY,
    usuario_id INTEGER NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
    nome      VARCHAR(255) NOT NULL,
    api_key   VARCHAR(64) UNIQUE NOT NULL,
    criado_em TIMESTAMPTZ DEFAULT now() NOT NULL
);

CREATE TABLE IF NOT EXISTS leituras (
    id             SERIAL PRIMARY KEY,
    dispositivo_id INTEGER NOT NULL REFERENCES dispositivos(id) ON DELETE CASCADE,
    temperatura    REAL,
    umidade        REAL,
    umidade_solo   REAL,
    timestamp      TIMESTAMPTZ DEFAULT now() NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_leituras_dispositivo_timestamp
    ON leituras (dispositivo_id, timestamp DESC);
