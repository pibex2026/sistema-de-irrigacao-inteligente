const express = require('express');
const router = express.Router();
const authController = require('../controllers/authController');
const authMiddleware = require('../middlewares/authMiddleware');
const { createRateLimiter } = require('../middlewares/rateLimitMiddleware');

const authLimiter = createRateLimiter({ windowMs: 15 * 60 * 1000, max: 20 });

router.post('/register', authLimiter, authController.register);
router.post('/login', authLimiter, authController.login);
router.get('/me', authMiddleware, authController.me);
router.patch('/password', authMiddleware, authController.changePassword);

module.exports = router;