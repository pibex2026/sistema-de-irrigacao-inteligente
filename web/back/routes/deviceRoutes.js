const express = require('express');
const router = express.Router();
const deviceController = require('../controllers/deviceController');

router.post('/', deviceController.create);
router.get('/', deviceController.list);
router.delete('/:id', deviceController.remove);

module.exports = router;
