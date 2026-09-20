const db = require('../configs/db');

const DeviceModel = {
    async create(usuarioId, nome, apiKey) {
        const queryText = `
            INSERT INTO dispositivos (usuario_id, nome, api_key)
            VALUES ($1, $2, $3)
            RETURNING id, usuario_id, nome, api_key, criado_em
        `;
        const { rows } = await db.query(queryText, [usuarioId, nome, apiKey]);
        return rows[0];
    },

    async findByApiKey(apiKey) {
        const queryText = 'SELECT * FROM dispositivos WHERE api_key = $1';
        const { rows } = await db.query(queryText, [apiKey]);
        return rows[0];
    },

    async findById(id) {
        const queryText = 'SELECT * FROM dispositivos WHERE id = $1';
        const { rows } = await db.query(queryText, [id]);
        return rows[0];
    },

    async findByIdAndUser(id, usuarioId) {
        const queryText = 'SELECT * FROM dispositivos WHERE id = $1 AND usuario_id = $2';
        const { rows } = await db.query(queryText, [id, usuarioId]);
        return rows[0];
    },

    async findAllByUser(usuarioId) {
        const queryText = `
            SELECT id, usuario_id, nome, api_key, criado_em
            FROM dispositivos
            WHERE usuario_id = $1
            ORDER BY id
        `;
        const { rows } = await db.query(queryText, [usuarioId]);
        return rows;
    },

    async remove(id, usuarioId) {
        const queryText = `
            DELETE FROM dispositivos
            WHERE id = $1 AND usuario_id = $2
            RETURNING id
        `;
        const { rows } = await db.query(queryText, [id, usuarioId]);
        return rows[0];
    }
};

module.exports = DeviceModel;
