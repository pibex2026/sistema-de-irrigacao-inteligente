const DeviceModel = require('../models/deviceModel');

const deviceAuthMiddleware = async (req, res, next) => {
    const apiKey = req.headers['x-api-key'];

    if (!apiKey) {
        return res.status(401).json({ error: 'API key não fornecida.' });
    }

    try {
        const device = await DeviceModel.findByApiKey(apiKey);
        if (!device) {
            return res.status(401).json({ error: 'API key inválida.' });
        }

        req.dispositivo = device;
        return next();
    } catch (error) {
        return res.status(500).json({ error: 'Erro ao autenticar dispositivo.' });
    }
};

module.exports = deviceAuthMiddleware;
