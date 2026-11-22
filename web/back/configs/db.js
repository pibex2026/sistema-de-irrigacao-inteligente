const { Pool } = require('pg');

const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
});

// Testa a conexão inicial e libera o cliente para o pool
pool.connect((err, client, release) => {
    if (err) {
        console.error('Erro ao conectar ao PostgreSQL:', err.message);
    } else {
        console.log('Conexão com o PostgreSQL estabelecida com sucesso.');
        if (release) release();
    }
});

pool.on('error', (err) => {
    console.error('Erro inesperado no cliente do pool PostgreSQL:', err);
});

module.exports = {
    query: (text, params) => pool.query(text, params),
    pool,
};