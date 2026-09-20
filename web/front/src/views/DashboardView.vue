<script setup>
import { ref, onMounted, onBeforeUnmount, nextTick } from 'vue';
import Chart from 'chart.js/auto';
import { deviceService } from '@/services/deviceService';
import { readingService } from '@/services/readingService';

const dispositivos = ref([]);
const dispositivo = ref(null);
const ultimaLeitura = ref(null);
const historico = ref([]);
const novoNome = ref('');
const error = ref('');
const carregando = ref(false);

let chart = null;
const chartCanvas = ref(null);
let pollLatest = null;
let pollHistory = null;

const LATEST_INTERVAL = 10000;
const HISTORY_INTERVAL = 30000;

function formatTimestamp(iso) {
    if (!iso) return '—';
    const date = new Date(iso);
    if (Number.isNaN(date.getTime())) return iso;
    return date.toLocaleString('pt-BR');
}

function formatNumber(value, unit = '') {
    if (value === null || value === undefined) return '—';
    return `${value}${unit}`;
}

async function carregarDispositivos() {
    try {
        dispositivos.value = await deviceService.list();
    } catch (err) {
        error.value = err.response?.data?.error || 'Erro ao carregar dispositivos.';
    }
}

async function criarDispositivo() {
    const nome = novoNome.value.trim();
    if (!nome) return;

    error.value = '';
    try {
        const novo = await deviceService.create(nome);
        dispositivos.value.push(novo);
        novoNome.value = '';
        selecionarDispositivo(novo);
    } catch (err) {
        error.value = err.response?.data?.error || 'Erro ao criar dispositivo.';
    }
}

async function removerDispositivo(d) {
    error.value = '';
    try {
        await deviceService.remove(d.id);
        dispositivos.value = dispositivos.value.filter((item) => item.id !== d.id);
        if (dispositivo.value && dispositivo.value.id === d.id) {
            dispositivo.value = null;
            ultimaLeitura.value = null;
            historico.value = [];
            destroyChart();
        }
    } catch (err) {
        error.value = err.response?.data?.error || 'Erro ao remover dispositivo.';
    }
}

async function selecionarDispositivo(d) {
    dispositivo.value = d;
    ultimaLeitura.value = null;
    historico.value = [];
    destroyChart();
    await carregarDados();
}

async function carregarLatest() {
    if (!dispositivo.value) return;
    try {
        ultimaLeitura.value = await readingService.latest(dispositivo.value.id);
    } catch (err) {
        if (err.response?.status !== 401) {
            error.value = err.response?.data?.error || 'Erro ao buscar leituras.';
        }
    }
}

async function carregarHistorico() {
    if (!dispositivo.value) return;
    try {
        historico.value = await readingService.history(dispositivo.value.id, { limit: 100 });
        await nextTick();
        renderChart();
    } catch (err) {
        if (err.response?.status !== 401) {
            error.value = err.response?.data?.error || 'Erro ao buscar histórico.';
        }
    }
}

async function carregarDados() {
    carregando.value = true;
    await Promise.all([carregarLatest(), carregarHistorico()]);
    carregando.value = false;
}

function destroyChart() {
    if (chart) {
        chart.destroy();
        chart = null;
    }
}

function renderChart() {
    destroyChart();
    if (!chartCanvas.value || historico.value.length === 0) return;

    const ordenado = [...historico.value].reverse();
    const labels = ordenado.map((item) => formatTimestamp(item.timestamp));

    chart = new Chart(chartCanvas.value, {
        type: 'line',
        data: {
            labels,
            datasets: [
                {
                    label: 'Temperatura (°C)',
                    data: ordenado.map((item) => item.temperatura),
                    borderColor: '#d9a441',
                    backgroundColor: 'rgba(217, 164, 65, 0.1)',
                    tension: 0.3,
                    fill: false
                },
                {
                    label: 'Umidade do ar (%)',
                    data: ordenado.map((item) => item.umidade),
                    borderColor: '#1f6e5b',
                    backgroundColor: 'rgba(31, 110, 91, 0.1)',
                    tension: 0.3,
                    fill: false
                },
                {
                    label: 'Umidade do solo (%)',
                    data: ordenado.map((item) => item.umidade_solo),
                    borderColor: '#7a4fb0',
                    backgroundColor: 'rgba(122, 79, 176, 0.1)',
                    tension: 0.3,
                    fill: false
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { position: 'bottom' }
            },
            scales: {
                y: { beginAtZero: true }
            }
        }
    });
}

onMounted(async () => {
    await carregarDispositivos();
    if (dispositivos.value.length > 0) {
        await selecionarDispositivo(dispositivos.value[0]);
    }

    pollLatest = setInterval(carregarLatest, LATEST_INTERVAL);
    pollHistory = setInterval(carregarHistorico, HISTORY_INTERVAL);
});

onBeforeUnmount(() => {
    if (pollLatest) clearInterval(pollLatest);
    if (pollHistory) clearInterval(pollHistory);
    destroyChart();
});
</script>

<template>
  <main class="page">
    <header class="dash-header">
      <h1>Dashboard</h1>
      <p class="text-muted text-sm">Acompanhe em tempo real as leituras dos seus dispositivos.</p>
    </header>

    <div v-if="error" class="alert alert-error">{{ error }}</div>

    <section class="card section">
      <div class="section__head">
        <h2>Dispositivos</h2>
      </div>

      <form class="device-form" @submit.prevent="criarDispositivo">
        <input v-model="novoNome" type="text" placeholder="Nome do dispositivo (ex.: Canteiro 1)" />
        <button class="btn btn-primary" type="submit" :disabled="!novoNome.trim()">Adicionar</button>
      </form>

      <div v-if="dispositivos.length === 0" class="text-muted text-sm">
        Nenhum dispositivo cadastrado ainda. Adicione o primeiro acima.
      </div>

      <div v-else class="device-list">
        <div
          v-for="d in dispositivos"
          :key="d.id"
          class="device-item"
          :class="{ 'device-item--active': dispositivo && dispositivo.id === d.id }"
        >
          <button class="device-item__select" @click="selecionarDispositivo(d)">
            <span>{{ d.nome }}</span>
          </button>
          <button class="btn btn-danger device-item__remove" @click="removerDispositivo(d)">Remover</button>
        </div>
      </div>
    </section>

    <section v-if="dispositivo" class="card section">
      <div class="section__head">
        <h2>{{ dispositivo.nome }}</h2>
        <span class="mono text-sm text-muted">api_key: {{ dispositivo.api_key }}</span>
      </div>

      <div class="metrics">
        <div class="metric">
          <span class="metric__label">Temperatura</span>
          <span class="metric__value">{{ formatNumber(ultimaLeitura?.temperatura) }}<small>°C</small></span>
        </div>
        <div class="metric">
          <span class="metric__label">Umidade do ar</span>
          <span class="metric__value">{{ formatNumber(ultimaLeitura?.umidade) }}<small>%</small></span>
        </div>
        <div class="metric">
          <span class="metric__label">Umidade do solo</span>
          <span class="metric__value">{{ formatNumber(ultimaLeitura?.umidade_solo) }}<small>%</small></span>
        </div>
      </div>

      <p class="text-muted text-sm">
        Última atualização: {{ formatTimestamp(ultimaLeitura?.timestamp) }}
      </p>
    </section>

    <section v-if="dispositivo" class="card section">
      <div class="section__head">
        <h2>Histórico</h2>
        <span class="text-muted text-sm" v-if="carregando">Carregando…</span>
      </div>
      <div class="chart-wrap">
        <canvas ref="chartCanvas"></canvas>
      </div>
      <p v-if="historico.length === 0 && !carregando" class="text-muted text-sm">
        Sem leituras registradas para este dispositivo.
      </p>
    </section>
  </main>
</template>

<style scoped>
.dash-header h1 {
    margin: 0 0 var(--space-1);
    font-size: 1.6rem;
}

.section {
    padding: var(--space-5);
    margin-bottom: var(--space-5);
}

.section__head {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--space-4);
    flex-wrap: wrap;
    margin-bottom: var(--space-4);
}

.section__head h2 {
    margin: 0;
    font-size: 1.1rem;
}

.device-form {
    display: flex;
    gap: var(--space-2);
    margin-bottom: var(--space-4);
}

.device-form input {
    flex: 1;
    padding: 11px 12px;
    border: 1px solid var(--color-border);
    border-radius: var(--radius-sm);
    font-size: 0.95rem;
}

.device-form input:focus {
    outline: none;
    border-color: var(--color-primary);
    box-shadow: 0 0 0 3px var(--color-primary-soft);
}

.device-list {
    display: flex;
    flex-direction: column;
    gap: var(--space-2);
}

.device-item {
    display: flex;
    align-items: center;
    gap: var(--space-2);
    border: 1px solid var(--color-border);
    border-radius: var(--radius-sm);
    padding: var(--space-2);
}

.device-item--active {
    border-color: var(--color-primary);
    background-color: var(--color-primary-soft);
}

.device-item__select {
    flex: 1;
    text-align: left;
    background: none;
    border: none;
    font-size: 0.95rem;
    font-weight: 600;
    color: var(--color-ink);
    padding: var(--space-2);
}

.device-item__remove {
    font-size: 0.85rem;
    padding: 8px 12px;
}

.metrics {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: var(--space-4);
}

.metric {
    background: var(--color-bg);
    border: 1px solid var(--color-border);
    border-radius: var(--radius-md);
    padding: var(--space-4);
    display: flex;
    flex-direction: column;
    gap: var(--space-1);
}

.metric__label {
    font-size: 0.85rem;
    color: var(--color-muted);
}

.metric__value {
    font-size: 2rem;
    font-weight: 700;
    color: var(--color-ink);
}

.metric__value small {
    font-size: 1rem;
    font-weight: 600;
    color: var(--color-muted);
    margin-left: 2px;
}

.chart-wrap {
    position: relative;
    height: 320px;
}
</style>
