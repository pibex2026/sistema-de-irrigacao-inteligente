const db = require('../configs/db');

const ReadingModel = {
    async create({ dispositivoId, temperatura, umidade, umidadeSolo, timestamp }) {
        const queryText = `
            INSERT INTO leituras (dispositivo_id, temperatura, umidade, umidade_solo, timestamp)
            VALUES ($1, $2, $3, $4, COALESCE($5, now()))
            RETURNING id, dispositivo_id, temperatura, umidade, umidade_solo, timestamp
        `;
        const { rows } = await db.query(queryText, [
            dispositivoId,
            temperatura,
            umidade,
            umidadeSolo,
            timestamp
        ]);
        return rows[0];
    },

    async findLatest(dispositivoId) {
        const queryText = `
            SELECT id, dispositivo_id, temperatura, umidade, umidade_solo, timestamp
            FROM leituras
            WHERE dispositivo_id = $1
            ORDER BY timestamp DESC
            LIMIT 1
        `;
        const { rows } = await db.query(queryText, [dispositivoId]);
        return rows[0];
    },

    async findHistory({ dispositivoId, limit = 100, offset = 0, from, to }) {
        const conditions = ['dispositivo_id = $1'];
        const params = [dispositivoId];
        let index = 2;

        if (from) {
            conditions.push(`timestamp >= $${index++}`);
            params.push(from);
        }
        if (to) {
            conditions.push(`timestamp <= $${index++}`);
            params.push(to);
        }

        const where = conditions.join(' AND ');
        const queryText = `
            SELECT id, dispositivo_id, temperatura, umidade, umidade_solo, timestamp
            FROM leituras
            WHERE ${where}
            ORDER BY timestamp DESC
            LIMIT $${index++} OFFSET $${index++}
        `;
        params.push(limit, offset);

        const { rows } = await db.query(queryText, params);
        return rows;
    }
};

module.exports = ReadingModel;
