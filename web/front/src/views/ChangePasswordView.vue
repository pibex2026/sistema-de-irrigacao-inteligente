<script setup>
import { ref } from 'vue';
import { authService } from '@/services/authService';

const senhaAtual = ref('');
const novaSenha = ref('');
const confirmacao = ref('');
const error = ref('');
const success = ref('');
const loading = ref(false);

async function submit() {
    error.value = '';
    success.value = '';

    if (novaSenha.value !== confirmacao.value) {
        error.value = 'A confirmação não confere com a nova senha.';
        return;
    }

    loading.value = true;
    try {
        const result = await authService.changePassword({
            senha_atual: senhaAtual.value,
            nova_senha: novaSenha.value
        });
        success.value = result.message;
        senhaAtual.value = '';
        novaSenha.value = '';
        confirmacao.value = '';
    } catch (err) {
        error.value = err.response?.data?.error || 'Não foi possível alterar a senha.';
    } finally {
        loading.value = false;
    }
}
</script>

<template>
  <main class="page">
    <div class="card form-card">
      <h1>Trocar senha</h1>
      <p class="text-muted text-sm">Atualize a senha da sua conta.</p>

      <div v-if="error" class="alert alert-error">{{ error }}</div>
      <div v-if="success" class="alert alert-success">{{ success }}</div>

      <form @submit.prevent="submit">
        <div class="field">
          <label for="senha_atual">Senha atual</label>
          <input id="senha_atual" v-model="senhaAtual" type="password" autocomplete="current-password" required />
        </div>
        <div class="field">
          <label for="nova_senha">Nova senha</label>
          <input id="nova_senha" v-model="novaSenha" type="password" autocomplete="new-password" required />
        </div>
        <div class="field">
          <label for="confirmacao">Confirmar nova senha</label>
          <input id="confirmacao" v-model="confirmacao" type="password" autocomplete="new-password" required />
        </div>
        <button class="btn btn-primary" type="submit" :disabled="loading">
          {{ loading ? 'Salvando...' : 'Salvar nova senha' }}
        </button>
      </form>
    </div>
  </main>
</template>

<style scoped>
.form-card {
    max-width: 480px;
    padding: var(--space-6);
}

.form-card h1 {
    margin: 0 0 var(--space-1);
    font-size: 1.4rem;
}
</style>
