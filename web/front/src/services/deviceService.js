import api from './api';

export const deviceService = {
    async list() {
        const { data } = await api.get('/api/devices');
        return data;
    },
    async create(nome) {
        const { data } = await api.post('/api/devices', { nome });
        return data;
    },
    async remove(id) {
        const { data } = await api.delete(`/api/devices/${id}`);
        return data;
    }
};
