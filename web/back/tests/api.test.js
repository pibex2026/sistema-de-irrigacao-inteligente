const test = require('node:test');
const assert = require('node:assert/strict');
const jwt = require('jsonwebtoken');

const { validateEnv, getJwtSecret } = require('../configs/env');
const authMiddleware = require('../middlewares/authMiddleware');
const deviceAuthMiddleware = require('../middlewares/deviceAuthMiddleware');
const { createRateLimiter } = require('../middlewares/rateLimitMiddleware');
const authService = require('../services/authService');
const telemetryService = require('../services/telemetryService');

function makeMockRes() {
    const res = { statusCode: 200, body: null };
    res.status = (code) => {
        res.statusCode = code;
        return res;
    };
    res.json = (payload) => {
        res.body = payload;
        return res;
    };
    return res;
}

test('validateEnv throws in production when JWT_SECRET is missing', () => {
    const prevEnv = process.env.NODE_ENV;
    const prevSecret = process.env.JWT_SECRET;
    try {
        process.env.NODE_ENV = 'production';
        delete process.env.JWT_SECRET;
        assert.throws(() => validateEnv(), /JWT_SECRET/);
    } finally {
        process.env.NODE_ENV = prevEnv;
        if (prevSecret !== undefined) process.env.JWT_SECRET = prevSecret;
    }
});

test('getJwtSecret returns configured secret when set', () => {
    const prevSecret = process.env.JWT_SECRET;
    try {
        process.env.JWT_SECRET = 'unit_test_secret_abc';
        assert.equal(getJwtSecret(), 'unit_test_secret_abc');
    } finally {
        if (prevSecret !== undefined) process.env.JWT_SECRET = prevSecret;
        else delete process.env.JWT_SECRET;
    }
});

test('authMiddleware rejects missing or invalid token and accepts valid token', () => {
    process.env.JWT_SECRET = 'unit_test_secret_abc';

    const resMissing = makeMockRes();
    let nextCalled = false;
    authMiddleware({ headers: {} }, resMissing, () => { nextCalled = true; });
    assert.equal(resMissing.statusCode, 401);
    assert.equal(nextCalled, false);

    const resInvalid = makeMockRes();
    authMiddleware({ headers: { authorization: 'Bearer invalid.token.here' } }, resInvalid, () => { nextCalled = true; });
    assert.equal(resInvalid.statusCode, 401);
    assert.equal(nextCalled, false);

    const token = jwt.sign({ id: 42, email: 'produtor@ifpe.edu.br' }, process.env.JWT_SECRET);
    const reqValid = { headers: { authorization: `Bearer ${token}` } };
    const resValid = makeMockRes();
    authMiddleware(reqValid, resValid, () => { nextCalled = true; });
    assert.equal(nextCalled, true);
    assert.deepEqual(reqValid.user, { id: 42, email: 'produtor@ifpe.edu.br' });
});

test('deviceAuthMiddleware validates X-Device-Key header', () => {
    process.env.DEVICE_API_KEY = 'esp32_secret_key';

    const resMissing = makeMockRes();
    let nextCalled = false;
    deviceAuthMiddleware({ headers: {} }, resMissing, () => { nextCalled = true; });
    assert.equal(resMissing.statusCode, 401);
    assert.equal(nextCalled, false);

    const resWrong = makeMockRes();
    deviceAuthMiddleware({ headers: { 'x-device-key': 'wrong_key' } }, resWrong, () => { nextCalled = true; });
    assert.equal(resWrong.statusCode, 401);
    assert.equal(nextCalled, false);

    const resOk = makeMockRes();
    deviceAuthMiddleware({ headers: { 'x-device-key': 'esp32_secret_key' } }, resOk, () => { nextCalled = true; });
    assert.equal(nextCalled, true);
});

test('rateLimitMiddleware blocks requests exceeding max within window', () => {
    const limiter = createRateLimiter({ windowMs: 60_000, max: 2 });
    const req = { ip: '127.0.0.1', headers: {} };

    let passed = 0;
    limiter(req, makeMockRes(), () => { passed++; });
    limiter(req, makeMockRes(), () => { passed++; });
    assert.equal(passed, 2);

    const resBlocked = makeMockRes();
    limiter(req, resBlocked, () => { passed++; });
    assert.equal(resBlocked.statusCode, 429);
    assert.equal(passed, 2);
});

test('authService rejects non-string types and short passwords', async () => {
    await assert.rejects(
        () => authService.registerUser(123, 'a@b.com', '12345678'),
        (err) => err.statusCode === 400
    );
    await assert.rejects(
        () => authService.registerUser('Victor', 'a@b.com', '123'),
        (err) => err.statusCode === 400
    );
    await assert.rejects(
        () => authService.loginUser({ email: 'a@b.com' }, '12345678'),
        (err) => err.statusCode === 400
    );
});

test('telemetryService rejects out-of-range sensor readings', async () => {
    await assert.rejects(
        () => telemetryService.ingestTelemetry({ temperatura: 150, umidade: 50, umidade_solo: 50 }),
        (err) => err.statusCode === 400
    );
    await assert.rejects(
        () => telemetryService.ingestTelemetry({ temperatura: 25, umidade: 120, umidade_solo: 50 }),
        (err) => err.statusCode === 400
    );
});
