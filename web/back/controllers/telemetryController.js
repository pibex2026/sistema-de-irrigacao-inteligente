const telemetryService = require('../services/telemetryService');

const telemetryController = {
    async postTelemetry(req, res) {
        try {
            const result = await telemetryService.ingestTelemetry(req.body);
            return res.status(201).json(result);
        } catch (error) {
            if (error.statusCode) {
                return res.status(error.statusCode).json({ error: error.message });
            }
            console.error('Erro ao registrar telemetria:', error);
            return res.status(500).json({ error: 'Erro interno ao registrar telemetria.' });
        }
    },

    async getTelemetry(req, res) {
        try {
            const summary = await telemetryService.getTelemetrySummary(req.query.limit);
            return res.status(200).json(summary);
        } catch (error) {
            console.error('Erro ao consultar telemetria:', error);
            return res.status(500).json({ error: 'Erro interno ao consultar telemetria.' });
        }
    },

    async getIrrigationHistory(req, res) {
        try {
            const result = await telemetryService.getIrrigationHistory(req.query.limit);
            return res.status(200).json(result);
        } catch (error) {
            console.error('Erro ao consultar histórico de irrigação:', error);
            return res.status(500).json({ error: 'Erro interno ao consultar histórico.' });
        }
    },

    async postManualIrrigation(req, res) {
        try {
            const result = await telemetryService.triggerManualIrrigation(req.body || {});
            return res.status(200).json(result);
        } catch (error) {
            if (error.statusCode) {
                return res.status(error.statusCode).json({ error: error.message });
            }
            console.error('Erro no acionamento manual:', error);
            return res.status(500).json({ error: 'Erro interno ao acionar válvula.' });
        }
    },
};

module.exports = telemetryController;
