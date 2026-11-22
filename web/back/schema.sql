-- Schema de inicialização do Banco de Dados PostgreSQL
-- Projeto: Sistema de Irrigação Inteligente (PIBEX 2026)

CREATE TABLE IF NOT EXISTS usuarios (
    id SERIAL PRIMARY KEY,
    nome VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    senha VARCHAR(255) NOT NULL,
    criado_em TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_usuarios_email ON usuarios(email);

-- Tabela para persistência futura de telemetria dos sensores do ESP32
CREATE TABLE IF NOT EXISTS leituras_sensores (
    id SERIAL PRIMARY KEY,
    temperatura SMALLINT NOT NULL,
    umidade SMALLINT NOT NULL,
    umidade_solo SMALLINT NOT NULL,
    timestamp_embarcado VARCHAR(30),
    criado_em TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Tabela para histórico de acionamento da válvula / moto-bomba
CREATE TABLE IF NOT EXISTS historico_irrigacao (
    id SERIAL PRIMARY KEY,
    status VARCHAR(20) NOT NULL, -- 'INICIADA', 'FINALIZADA'
    origem VARCHAR(50) DEFAULT 'AUTOMATICO_REDE_NEURAL', -- 'AUTOMATICO_REDE_NEURAL', 'MANUAL_DASHBOARD', 'FALLBACK_THRESHOLD'
    duracao_segundos INT DEFAULT 0,
    criado_em TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
