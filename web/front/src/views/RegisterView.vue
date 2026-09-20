<script setup>
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import { authService } from '@/services/authService';
import { useAuthStore } from '@/stores/auth';

const router = useRouter();
const auth = useAuthStore();

const nome = ref('');
const email = ref('');
const senha = ref('');
const error = ref('');
const loading = ref(false);

async function submit() {
    error.value = '';
    loading.value = true;
    try {
        await authService.register({
            nome: nome.value,
            email: email.value,
            senha: senha.value
        });
        const result = await authService.login({ email: email.value, senha: senha.value });
        auth.setSession(result);
        router.push({ name: 'dashboard' });
    } catch (err) {
        error.value = err.response?.data?.error || 'Não foi possível cadastrar.';
    } finally {
        loading.value = false;
    }
}
</script>

<template>
  <main class="auth-page">
    <div class="auth-card card">
      <h1 class="auth-title">Criar conta</h1>
      <p class="text-muted text-sm auth-subtitle">Cadastre-se para monitorar seus dispositivos.</p>

      <div v-if="error" class="alert alert-error">{{ error }}</div>

      <form @submit.prevent="submit">
        <div class="field">
          <label for="nome">Nome</label>
          <input id="nome" v-model="nome" type="text" autocomplete="name" required />
        </div>
        <div class="field">
          <label for="email">E-mail</label>
          <input id="email" v-model="email" type="email" autocomplete="email" required />
        </div>
        <div class="field">
          <label for="senha">Senha</label>
          <input id="senha" v-model="senha" type="password" autocomplete="new-password" required />
        </div>
        <button class="btn btn-primary auth-submit" type="submit" :disabled="loading">
          {{ loading ? 'Cadastrando...' : 'Cadastrar' }}
        </button>
      </form>

      <p class="text-sm auth-footer">
        Já tem conta?
        <router-link :to="{ name: 'login' }">Entrar</router-link>
      </p>
    </div>
  </main>
</template>

<style scoped>
.auth-page {
    flex: 1;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: var(--space-5);
}

.auth-card {
    width: 100%;
    max-width: 420px;
    padding: var(--space-6);
}

.auth-title {
    margin: 0 0 var(--space-1);
    font-size: 1.5rem;
}

.auth-subtitle {
    margin: 0 0 var(--space-5);
}

.auth-submit {
    width: 100%;
    margin-top: var(--space-2);
}

.auth-footer {
    margin-top: var(--space-5);
    text-align: center;
}
</style>
