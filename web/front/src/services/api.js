import axios from 'axios';

const TOKEN_KEY = 'irrigacao_token';

const api = axios.create({
    baseURL: import.meta.env.VITE_API_URL || 'http://localhost:8080'
});

api.interceptors.request.use((config) => {
    const token = localStorage.getItem(TOKEN_KEY);
    if (token) {
        config.headers.Authorization = `Bearer ${token}`;
    }
    return config;
});

api.interceptors.response.use(
    (response) => response,
    (error) => {
        if (error.response && error.response.status === 401) {
            const token = localStorage.getItem(TOKEN_KEY);
            if (token) {
                localStorage.removeItem(TOKEN_KEY);
                localStorage.removeItem('irrigacao_user');
                window.location.href = '/login';
            }
        }
        return Promise.reject(error);
    }
);

export default api;
