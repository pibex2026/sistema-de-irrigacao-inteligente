const db = require('../configs/db');

const UserModel = {
    async findByEmail(email) {
        const queryText = 'SELECT * FROM usuarios WHERE email = $1';
        const { rows } = await db.query(queryText, [email]);
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