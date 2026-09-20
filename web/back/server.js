require('dotenv').config({ path: '../../.env' });
require('./configs/db');

const express = require('express');
const cors = require('cors');
const authRoutes = require('./routes/authRoutes');
const deviceRoutes = require('./routes/deviceRoutes');
const readingRoutes = require('./routes/readingRoutes');
const authMiddleware = require('./middlewares/authMiddleware');

const app = express();
const port = process.env.PORT || 8080;

app.use(cors());
app.use(express.json());

app.use('/api/auth', authRoutes);
app.use('/api/devices', authMiddleware, deviceRoutes);
app.use('/api/readings', readingRoutes);

app.get('/api', (req, res) => {
    res.json({ status: 'Backend operacional' });
});

app.listen(port, () => {
    console.log(`Servidor rodando em http://localhost:${port}`);
});
