import api from './api';

export const readingService = {
    async latest(dispositivoId) {
        const { data } = await api.get('/api/readings/latest', {
            params: { dispositivo_id: dispositivoId }
        });
        return data;
    },
    async history(dispositivoId, { limit = 100, from, to } = {}) {
        const { data } = await api.get('/api/readings', {
            params: { dispositivo_id: dispositivoId, limit, from, to }
        });
        return data;
    }
};
