const db = require('../configs/db');

const UserModel = {
    async findByEmail(email) {
        const queryText = 'SELECT * FROM usuarios WHERE email = $1';
        const { rows } = await db.query(queryText, [email]);
        return rows[0];
    },

    async findById(id) {
        const queryText = 'SELECT id, nome, email, criado_em FROM usuarios WHERE id = $1';
        const { rows } = await db.query(queryText, [id]);
        return rows[0];
    },

    async findByIdWithPassword(id) {
        const queryText = 'SELECT id, nome, email, senha FROM usuarios WHERE id = $1';
        const { rows } = await db.query(queryText, [id]);
        return rows[0];
    },

    async updatePassword(id, passwordHash) {
        const queryText = 'UPDATE usuarios SET senha = $1 WHERE id = $2 RETURNING id';
        const { rows } = await db.query(queryText, [passwordHash, id]);
        return rows[0];
    },

    async create(nome, email, passwordHash) {
        const queryText = `
            INSERT INTO usuarios (nome, email, senha) 
            VALUES ($1, $2, $3) 
            RETURNING id, nome, email, criado_em
        `;
        const { rows } = await db.query(queryText, [nome, email, passwordHash]);
        return rows[0];
    }
};

module.exports = UserModel;