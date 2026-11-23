const TelemetryModel = require('../models/telemetryModel');

const ONLINE_THRESHOLD_MS = 60 * 1000;

let pendingManualCommand = null;
let lastDeviceValveState = null;

function isValidNumberInRange(val, min, max) {
    return typeof val === 'number' && Number.isFinite(val) && val >= min && val <= max;
}

const telemetryService = {
    async ingestTelemetry(payload) {
        const {
            temperatura,
            umidade,
            umidade_solo,
            timestamp_embarcado,
            valvula_aberta,
            evento_irrigacao,
        } = payload || {};

        if (
            !isValidNumberInRange(temperatura, -40, 85) ||
            !isValidNumberInRange(umidade, 0, 100) ||
            !isValidNumberInRange(umidade_solo, 0, 100)
        ) {
            const err = new Error('Leituras de sensores inválidas ou fora da faixa permitida.');
            err.statusCode = 400;
            throw err;
        }

        const safeTimestamp = typeof timestamp_embarcado === 'string'
            ? timestamp_embarcado.slice(0, 30)
            : null;

        const reading = await TelemetryModel.insertReading({
            temperatura,
            umidade,
            umidade_solo,
            timestamp_embarcado: safeTimestamp,
        });

        if (typeof valvula_aberta === 'boolean') {
            lastDeviceValveState = valvula_aberta;
        }

        let event = null;
        if (evento_irrigacao && typeof evento_irrigacao === 'object') {
            const allowedStatuses = ['INICIADA', 'FINALIZADA'];
            if (!allowedStatuses.includes(evento_irrigacao.status)) {
                const err = new Error('Status de evento_irrigacao inválido.');
                err.statusCode = 400;
                throw err;
            }
            const origem = typeof evento_irrigacao.origem === 'string'
                ? evento_irrigacao.origem.slice(0, 50)
                : 'AUTOMATICO_REDE_NEURAL';
            event = await TelemetryModel.insertIrrigationEvent({
                status: evento_irrigacao.status,
                origem,
                duracao_segundos: evento_irrigacao.duracao_segundos,
            });
        }

        const commandToDeliver = pendingManualCommand;
        pendingManualCommand = null;

        return {
            ok: true,
            reading,
            event,
            pending_command: commandToDeliver,
        };
    },

    async getTelemetrySummary(limit = 20) {
        const [readings, latestEvent] = await Promise.all([
            TelemetryModel.getRecentReadings(limit),
            TelemetryModel.getLatestIrrigationEvent(),
        ]);

        const latest = readings[0] || null;
        let esp32Online = false;
        if (latest && latest.criado_em) {
            const ageMs = Date.now() - new Date(latest.criado_em).getTime();
            esp32Online = ageMs >= 0 && ageMs <= ONLINE_THRESHOLD_MS;
        }

        let valveOpen = false;
        if (lastDeviceValveState !== null && esp32Online) {
            valveOpen = lastDeviceValveState;
        } else if (latestEvent) {
            valveOpen = latestEvent.status === 'INICIADA';
        }

        return {
            latest,
            readings,
            valve_open: valveOpen,
            esp32_online: esp32Online,
            last_seen: latest ? latest.criado_em : null,
        };
    },

    async getIrrigationHistory(limit = 20) {
        const history = await TelemetryModel.getIrrigationHistory(limit);
        return { history };
    },

    async triggerManualIrrigation({ open, acao }) {
        let targetOpen;
        if (typeof open === 'boolean') {
            targetOpen = open;
        } else if (acao === 'ABRIR') {
            targetOpen = true;
        } else if (acao === 'FECHAR') {
            targetOpen = false;
        } else {
            const err = new Error('Informe "open" (boolean) ou "acao" ("ABRIR" | "FECHAR").');
            err.statusCode = 400;
            throw err;
        }

        const status = targetOpen ? 'INICIADA' : 'FINALIZADA';
        const event = await TelemetryModel.insertIrrigationEvent({
            status,
            origem: 'MANUAL_DASHBOARD',
            duracao_segundos: 0,
        });

        pendingManualCommand = {
            valve_open: targetOpen,
            requested_at: new Date().toISOString(),
        };
        lastDeviceValveState = targetOpen;

        return {
            ok: true,
            valve_open: targetOpen,
            event,
        };
    },
};

module.exports = telemetryService;
