require('dotenv').config({ path: '../../.env' }); 
require('./configs/db'); 

const express = require('express');
const cors = require('cors');
const authRoutes = require('./routes/authRoutes');

const app = express();
const port = 8080;

app.use(cors());
app.use(express.json());

app.use('/api/auth', authRoutes);

app.get('/api', (req, res) => {
    res.json({ status: "Backend operacional" });
});

app.listen(port, () => {
    console.log(`Servidor rodando em http://localhost:${port}`);
});