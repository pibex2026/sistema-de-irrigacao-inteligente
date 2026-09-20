const express = require('express');
const router = express.Router();
const readingController = require('../controllers/readingController');
const deviceAuthMiddleware = require('../middlewares/deviceAuthMiddleware');
const authMiddleware = require('../middlewares/authMiddleware');

router.post('/', deviceAuthMiddleware, readingController.ingest);
router.get('/latest', authMiddleware, readingController.latest);
router.get('/', authMiddleware, readingController.history);

module.exports = router;
