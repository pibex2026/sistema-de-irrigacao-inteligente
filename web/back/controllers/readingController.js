const readingService = require('../services/readingService');
const DeviceModel = require('../models/deviceModel');

const readingController = {
    async ingest(req, res) {
        try {
            const reading = await readingService.saveReading(req.dispositivo.id, req.body);
            return res.status(201).json(reading);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    },

    async latest(req, res) {
        try {
            const dispositivoId = await resolveOwnedDevice(req);
            const reading = await readingService.getLatest(dispositivoId);
            return res.status(200).json(reading || null);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    },

    async history(req, res) {
        try {
            const dispositivoId = await resolveOwnedDevice(req);
            const readings = await readingService.getHistory(dispositivoId, req.query);
            return res.status(200).json(readings);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    }
};

async function resolveOwnedDevice(req) {
    const dispositivoId = parseInt(req.query.dispositivo_id, 10);
    if (!Number.isInteger(dispositivoId)) {
        throw new Error('O parâmetro dispositivo_id é obrigatório.');
    }

    const device = await DeviceModel.findByIdAndUser(dispositivoId, req.userId);
    if (!device) {
        throw new Error('Dispositivo não encontrado.');
    }
    return device.id;
}

module.exports = readingController;
