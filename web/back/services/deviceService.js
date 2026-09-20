const crypto = require('crypto');
const DeviceModel = require('../models/deviceModel');

function generateApiKey() {
    return crypto.randomBytes(24).toString('hex');
}

const deviceService = {
    async createDevice(usuarioId, body) {
        const { nome } = body || {};

        if (!nome || !String(nome).trim()) {
            throw new Error('O nome do dispositivo é obrigatório.');
        }

        const apiKey = generateApiKey();
        return DeviceModel.create(usuarioId, String(nome).trim(), apiKey);
    },

    async listDevices(usuarioId) {
        return DeviceModel.findAllByUser(usuarioId);
    },

    async removeDevice(usuarioId, deviceId) {
        const device = await DeviceModel.findByIdAndUser(deviceId, usuarioId);
        if (!device) {
            throw new Error('Dispositivo não encontrado.');
        }
        await DeviceModel.remove(deviceId, usuarioId);
        return { message: 'Dispositivo removido com sucesso.' };
    }
};

module.exports = deviceService;
