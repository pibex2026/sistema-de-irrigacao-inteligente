const ReadingModel = require('../models/readingModel');
const { validateReading } = require('../validators/readingValidator');

const readingService = {
    async saveReading(dispositivoId, body) {
        const { temperatura, umidade, umidadeSolo, timestamp } = validateReading(body);

        if (temperatura === null && umidade === null && umidadeSolo === null) {
            throw new Error('Informe ao menos uma leitura (temperatura, umidade ou umidade_solo).');
        }

        return ReadingModel.create({
            dispositivoId,
            temperatura,
            umidade,
            umidadeSolo,
            timestamp
        });
    },

    async getLatest(dispositivoId) {
        return ReadingModel.findLatest(dispositivoId);
    },

    async getHistory(dispositivoId, query) {
        const limit = Math.min(Math.max(parseInt(query.limit, 10) || 100, 1), 1000);
        const offset = Math.max(parseInt(query.offset, 10) || 0, 0);
        const from = query.from || null;
        const to = query.to || null;

        return ReadingModel.findHistory({ dispositivoId, limit, offset, from, to });
    }
};

module.exports = readingService;
