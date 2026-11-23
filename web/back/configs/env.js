const crypto = require('crypto');

let ephemeralDevSecret = null;

/**
 * Validates required runtime environment variables at startup.
 * Fails fast in production when JWT_SECRET is missing.
 */
function validateEnv() {
    const isProd = process.env.NODE_ENV === 'production';

    if (!process.env.JWT_SECRET) {
        if (isProd) {
            throw new Error('Variável de ambiente obrigatória JWT_SECRET não definida em produção.');
        }
        if (!ephemeralDevSecret) {
            ephemeralDevSecret = crypto.randomBytes(32).toString('hex');
            console.warn('[AVISO DE SEGURANÇA] JWT_SECRET não definido. Utilizando segredo efêmero de sessão (apenas desenvolvimento).');
        }
    }
}

/**
 * Returns the active JWT signing secret.
 * Never falls back to a hardcoded string.
 */
function getJwtSecret() {
    if (process.env.JWT_SECRET) {
        return process.env.JWT_SECRET;
    }
    if (process.env.NODE_ENV === 'production') {
        throw new Error('JWT_SECRET não definido em produção.');
    }
    if (!ephemeralDevSecret) {
        ephemeralDevSecret = crypto.randomBytes(32).toString('hex');
        console.warn('[AVISO DE SEGURANÇA] JWT_SECRET não definido. Utilizando segredo efêmero de sessão (apenas desenvolvimento).');
    }
    return ephemeralDevSecret;
}

module.exports = {
    validateEnv,
    getJwtSecret,
};
