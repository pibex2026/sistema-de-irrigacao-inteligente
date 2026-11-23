const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../../.env') }); 
require('dotenv').config(); // Fallback se houver .env na pasta local
const { validateEnv } = require('./configs/env');
validateEnv();
const db = require('./configs/db'); 

const express = require('express');
const cors = require('cors');
const authRoutes = require('./routes/authRoutes');
const telemetryRoutes = require('./routes/telemetryRoutes');

const app = express();
const port = process.env.PORT || 8080;

const corsOrigins = process.env.CORS_ORIGIN
    ? process.env.CORS_ORIGIN.split(',').map((s) => s.trim()).filter(Boolean)
    : null;

app.use(cors(corsOrigins ? { origin: corsOrigins } : undefined));
app.use(express.json());

app.use('/api/auth', authRoutes);
app.use('/api', telemetryRoutes);

app.get('/api', async (req, res) => {
    try {
        await db.query('SELECT 1');
        return res.json({ status: 'Backend operacional', db: 'ok' });
    } catch (_err) {
        return res.status(503).json({ status: 'Banco de dados indisponível', db: 'error' });
    }
});

// Middleware global para tratamento de erros inesperados
app.use((err, req, res, next) => {
    console.error('Erro não tratado na aplicação:', err);
    res.status(500).json({ error: 'Erro interno do servidor.' });
});

app.listen(port, () => {
    console.log(`Servidor rodando em http://localhost:${port}`);
});