const deviceService = require('../services/deviceService');

const deviceController = {
    async create(req, res) {
        try {
            const device = await deviceService.createDevice(req.userId, req.body);
            return res.status(201).json(device);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    },

    async list(req, res) {
        try {
            const devices = await deviceService.listDevices(req.userId);
            return res.status(200).json(devices);
        } catch (error) {
            return res.status(500).json({ error: error.message });
        }
    },

    async remove(req, res) {
        try {
            const result = await deviceService.removeDevice(req.userId, req.params.id);
            return res.status(200).json(result);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    }
};

module.exports = deviceController;
