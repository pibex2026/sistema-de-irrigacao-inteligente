/**
 * Creates a simple in-memory sliding-window rate limiter middleware.
 */
function createRateLimiter({ windowMs = 15 * 60 * 1000, max = 20 } = {}) {
    const hits = new Map();

    return function rateLimitMiddleware(req, res, next) {
        const key = req.ip || req.headers['x-forwarded-for'] || 'unknown';
        const now = Date.now();
        const recent = (hits.get(key) || []).filter((ts) => now - ts < windowMs);

        if (recent.length >= max) {
            return res.status(429).json({
                error: 'Muitas tentativas. Tente novamente mais tarde.',
            });
        }

        recent.push(now);
        hits.set(key, recent);
        return next();
    };
}

module.exports = { createRateLimiter };
