import { defineStore } from 'pinia';

const TOKEN_KEY = 'irrigacao_token';
const USER_KEY = 'irrigacao_user';

export const useAuthStore = defineStore('auth', {
    state: () => ({
        token: localStorage.getItem(TOKEN_KEY) || '',
        user: JSON.parse(localStorage.getItem(USER_KEY) || 'null')
    }),
    getters: {
        isAuthenticated: (state) => Boolean(state.token),
        userName: (state) => state.user?.nome || ''
    },
    actions: {
        setSession({ token, user }) {
            this.token = token;
            this.user = user;
            localStorage.setItem(TOKEN_KEY, token);
            localStorage.setItem(USER_KEY, JSON.stringify(user));
        },
        logout() {
            this.token = '';
            this.user = null;
            localStorage.removeItem(TOKEN_KEY);
            localStorage.removeItem(USER_KEY);
        }
    }
});
