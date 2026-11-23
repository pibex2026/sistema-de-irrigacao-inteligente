const jwt = require('jsonwebtoken');
const { getJwtSecret } = require('../configs/env');

/**
 * Express middleware that validates a Bearer JWT in the Authorization header.
 */
function authMiddleware(req, res, next) {
    const authHeader = req.headers.authorization;
    if (!authHeader || typeof authHeader !== 'string' || !authHeader.startsWith('Bearer ')) {
        return res.status(401).json({ error: 'Token de autenticação não fornecido.' });
    }

    const token = authHeader.slice(7).trim();
    if (!token) {
        return res.status(401).json({ error: 'Token de autenticação não fornecido.' });
    }

    try {
        const decoded = jwt.verify(token, getJwtSecret());
        req.user = { id: decoded.id, email: decoded.email };
        return next();
    } catch (_err) {
        return res.status(401).json({ error: 'Token inválido ou expirado.' });
    }
}

module.exports = authMiddleware;
