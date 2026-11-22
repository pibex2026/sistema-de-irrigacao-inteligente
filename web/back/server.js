const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../../.env') }); 
require('dotenv').config(); // Fallback se houver .env na pasta local
require('./configs/db'); 

const express = require('express');
const cors = require('cors');
const authRoutes = require('./routes/authRoutes');

const app = express();
const port = process.env.PORT || 8080;

app.use(cors());
app.use(express.json());

app.use('/api/auth', authRoutes);

app.get('/api', (req, res) => {
    res.json({ status: "Backend operacional" });
});

// Middleware global para tratamento de erros inesperados
app.use((err, req, res, next) => {
    console.error('Erro não tratado na aplicação:', err);
    res.status(500).json({ error: 'Erro interno do servidor.' });
});

app.listen(port, () => {
    console.log(`Servidor rodando em http://localhost:${port}`);
});