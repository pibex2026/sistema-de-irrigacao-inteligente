const express = require('express');
const router = express.Router();
const telemetryController = require('../controllers/telemetryController');
const authMiddleware = require('../middlewares/authMiddleware');
const deviceAuthMiddleware = require('../middlewares/deviceAuthMiddleware');

router.post('/telemetry', deviceAuthMiddleware, telemetryController.postTelemetry);
router.get('/telemetry', authMiddleware, telemetryController.getTelemetry);
router.get('/irrigation/history', authMiddleware, telemetryController.getIrrigationHistory);
router.post('/irrigation/manual', authMiddleware, telemetryController.postManualIrrigation);

module.exports = router;
