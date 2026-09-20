<script setup>
import { useRouter } from 'vue-router';
import { useAuthStore } from '@/stores/auth';

const router = useRouter();
const auth = useAuthStore();

function logout() {
    auth.logout();
    router.push({ name: 'login' });
}
</script>

<template>
  <header class="navbar">
    <div class="navbar__inner">
      <router-link :to="{ name: 'dashboard' }" class="brand">
        <span class="brand__mark">💧</span>
        <span class="brand__text">Irrigação Inteligente</span>
      </router-link>

      <nav class="nav">
        <router-link :to="{ name: 'dashboard' }" class="nav__link">Dashboard</router-link>
        <router-link :to="{ name: 'change-password' }" class="nav__link">Trocar senha</router-link>
        <span v-if="auth.userName" class="nav__user">{{ auth.userName }}</span>
        <button class="btn btn-ghost" @click="logout">Sair</button>
      </nav>
    </div>
  </header>
</template>

<style scoped>
.navbar {
    background: var(--color-surface);
    border-bottom: 1px solid var(--color-border);
}

.navbar__inner {
    max-width: 1120px;
    margin: 0 auto;
    padding: var(--space-3) var(--space-5);
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: var(--space-4);
}

.brand {
    display: inline-flex;
    align-items: center;
    gap: var(--space-2);
    font-weight: 700;
    color: var(--color-ink);
    text-decoration: none;
}

.brand__mark {
    font-size: 1.2rem;
}

.nav {
    display: flex;
    align-items: center;
    gap: var(--space-4);
}

.nav__link {
    color: var(--color-muted);
    font-weight: 500;
    text-decoration: none;
}

.nav__link.router-link-active {
    color: var(--color-primary);
}

.nav__user {
    color: var(--color-ink);
    font-weight: 600;
    font-size: 0.9rem;
}
</style>
