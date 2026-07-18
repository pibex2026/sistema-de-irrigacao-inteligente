const { Pool } = require('pg');

const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
});

pool.connect((err) => {
    if (err) {
        console.error('Erro ao conectar ao PostgreSQL:', err.stack);
    } else {
        console.log('Conexão com o PostgreSQL estabelecida com sucesso.');
    }
});

module.exports = {
    query: (text, params) => pool.query(text, params),
};