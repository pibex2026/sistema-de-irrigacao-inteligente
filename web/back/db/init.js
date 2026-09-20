require('dotenv').config({ path: '../../.env' });

const fs = require('fs');
const path = require('path');
const { Pool } = require('pg');

const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
});

const schemaPath = path.join(__dirname, 'schema.sql');
const schema = fs.readFileSync(schemaPath, 'utf8');

(async () => {
    try {
        await pool.query(schema);
        console.log('Schema aplicado com sucesso.');
    } catch (error) {
        console.error('Erro ao aplicar o schema:', error.message);
        process.exitCode = 1;
    } finally {
        await pool.end();
    }
})();
