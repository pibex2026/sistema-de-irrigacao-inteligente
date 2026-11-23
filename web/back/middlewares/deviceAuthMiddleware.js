const crypto = require('crypto');

/**
 * Middleware that authenticates ESP32 device requests via X-Device-Key header.
 */
function deviceAuthMiddleware(req, res, next) {
    const expectedKey = process.env.DEVICE_API_KEY;
    if (!expectedKey) {
        return res.status(503).json({ error: 'DEVICE_API_KEY não configurada no servidor.' });
    }

    const providedKey = req.headers['x-device-key'];
    if (!providedKey || typeof providedKey !== 'string') {
        return res.status(401).json({ error: 'Cabeçalho X-Device-Key ausente.' });
    }

    const expectedBuf = Buffer.from(expectedKey, 'utf8');
    const providedBuf = Buffer.from(providedKey, 'utf8');
    if (expectedBuf.length !== providedBuf.length || !crypto.timingSafeEqual(expectedBuf, providedBuf)) {
        return res.status(401).json({ error: 'Chave de dispositivo inválida.' });
    }

    return next();
}

module.exports = deviceAuthMiddleware;
