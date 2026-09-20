import { createRouter, createWebHistory } from 'vue-router';

const routes = [
    {
        path: '/login',
        name: 'login',
        component: () => import('@/views/LoginView.vue'),
        meta: { requiresAuth: false }
    },
    {
        path: '/register',
        name: 'register',
        component: () => import('@/views/RegisterView.vue'),
        meta: { requiresAuth: false }
    },
    {
        path: '/',
        name: 'dashboard',
        component: () => import('@/views/DashboardView.vue'),
        meta: { requiresAuth: true }
    },
    {
        path: '/trocar-senha',
        name: 'change-password',
        component: () => import('@/views/ChangePasswordView.vue'),
        meta: { requiresAuth: true }
    }
];

const router = createRouter({
    history: createWebHistory(),
    routes
});

router.beforeEach((to) => {
    const token = localStorage.getItem('irrigacao_token');

    if (to.meta.requiresAuth && !token) {
        return { name: 'login' };
    }

    if (!to.meta.requiresAuth && token) {
        return { name: 'dashboard' };
    }

    return true;
});

export default router;
