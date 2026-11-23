const db = require('../configs/db');

const TelemetryModel = {
    async insertReading({ temperatura, umidade, umidade_solo, timestamp_embarcado }) {
        const queryText = `
            INSERT INTO leituras_sensores (temperatura, umidade, umidade_solo, timestamp_embarcado)
            VALUES ($1, $2, $3, $4)
            RETURNING id, temperatura, umidade, umidade_solo, timestamp_embarcado, criado_em
        `;
        const { rows } = await db.query(queryText, [
            Math.round(temperatura),
            Math.round(umidade),
            Math.round(umidade_solo),
            timestamp_embarcado || null,
        ]);
        return rows[0];
    },

    async getRecentReadings(limit = 20) {
        const safeLimit = Math.min(Math.max(Number(limit) || 20, 1), 100);
        const queryText = `
            SELECT id, temperatura, umidade, umidade_solo, timestamp_embarcado, criado_em
            FROM leituras_sensores
            ORDER BY criado_em DESC, id DESC
            LIMIT $1
        `;
        const { rows } = await db.query(queryText, [safeLimit]);
        return rows;
    },

    async insertIrrigationEvent({ status, origem = 'AUTOMATICO_REDE_NEURAL', duracao_segundos = 0 }) {
        const queryText = `
            INSERT INTO historico_irrigacao (status, origem, duracao_segundos)
            VALUES ($1, $2, $3)
            RETURNING id, status, origem, duracao_segundos, criado_em
        `;
        const { rows } = await db.query(queryText, [
            status,
            origem,
            Math.max(0, Math.round(Number(duracao_segundos) || 0)),
        ]);
        return rows[0];
    },

    async getIrrigationHistory(limit = 20) {
        const safeLimit = Math.min(Math.max(Number(limit) || 20, 1), 100);
        const queryText = `
            SELECT id, status, origem, duracao_segundos, criado_em
            FROM historico_irrigacao
            ORDER BY criado_em DESC, id DESC
            LIMIT $1
        `;
        const { rows } = await db.query(queryText, [safeLimit]);
        return rows;
    },

    async getLatestIrrigationEvent() {
        const queryText = `
            SELECT id, status, origem, duracao_segundos, criado_em
            FROM historico_irrigacao
            ORDER BY criado_em DESC, id DESC
            LIMIT 1
        `;
        const { rows } = await db.query(queryText);
        return rows[0] || null;
    },
};

module.exports = TelemetryModel;
