// Cliente Frontend para API do Sistema de Irrigação Inteligente
const API_BASE = 'http://localhost:8080/api';

// Elementos da interface
const authSection = document.getElementById('authSection');
const dashboardSection = document.getElementById('dashboardSection');
const tabLogin = document.getElementById('tabLogin');
const tabRegister = document.getElementById('tabRegister');
const loginForm = document.getElementById('loginForm');
const registerForm = document.getElementById('registerForm');
const authMessage = document.getElementById('authMessage');
const userGreeting = document.getElementById('userGreeting');
const userName = document.getElementById('userName');
const logoutBtn = document.getElementById('logoutBtn');

// Controle de abas
tabLogin.addEventListener('click', () => {
  loginForm.classList.remove('hidden');
  registerForm.classList.add('hidden');
  tabLogin.className = 'flex-1 py-2 font-semibold text-green-700 border-b-2 border-green-700';
  tabRegister.className = 'flex-1 py-2 font-semibold text-gray-500 hover:text-green-700';
  clearMessage();
});

tabRegister.addEventListener('click', () => {
  loginForm.classList.add('hidden');
  registerForm.classList.remove('hidden');
  tabRegister.className = 'flex-1 py-2 font-semibold text-green-700 border-b-2 border-green-700';
  tabLogin.className = 'flex-1 py-2 font-semibold text-gray-500 hover:text-green-700';
  clearMessage();
});

function showMessage(text, isError = true) {
  authMessage.textContent = text;
  authMessage.className = `mt-4 text-sm text-center font-medium ${isError ? 'text-red-600' : 'text-green-600'}`;
  authMessage.classList.remove('hidden');
}

function clearMessage() {
  authMessage.classList.add('hidden');
  authMessage.textContent = '';
}

// Submissão de Cadastro
registerForm.addEventListener('submit', async (e) => {
  e.preventDefault();
  clearMessage();

  const nome = document.getElementById('regName').value.trim();
  const email = document.getElementById('regEmail').value.trim();
  const senha = document.getElementById('regPassword').value;

  try {
    const res = await fetch(`${API_BASE}/auth/register`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ nome, email, senha })
    });

    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || 'Falha no cadastro');
    }

    showMessage('Conta criada com sucesso! Faça login.', false);
    registerForm.reset();
    setTimeout(() => tabLogin.click(), 1500);
  } catch (err) {
    showMessage(err.message, true);
  }
});

// Submissão de Login
loginForm.addEventListener('submit', async (e) => {
  e.preventDefault();
  clearMessage();

  const email = document.getElementById('loginEmail').value.trim();
  const senha = document.getElementById('loginPassword').value;

  try {
    const res = await fetch(`${API_BASE}/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, senha })
    });

    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || 'Credenciais inválidas');
    }

    // Salva autenticação
    localStorage.setItem('auth_token', data.token);
    localStorage.setItem('user_data', JSON.stringify(data.user));

    enterDashboard(data.user);
  } catch (err) {
    showMessage(err.message, true);
  }
});

let telemetryIntervalId = null;
let currentValveOpen = false;

// Logout
logoutBtn.addEventListener('click', () => {
  if (telemetryIntervalId !== null) {
    clearInterval(telemetryIntervalId);
    telemetryIntervalId = null;
  }
  localStorage.removeItem('auth_token');
  localStorage.removeItem('user_data');
  authSection.classList.remove('hidden');
  dashboardSection.classList.add('hidden');
  userGreeting.classList.add('hidden');
  logoutBtn.classList.add('hidden');
});

function enterDashboard(user) {
  authSection.classList.add('hidden');
  dashboardSection.classList.remove('hidden');
  userName.textContent = user.nome;
  userGreeting.classList.remove('hidden');
  logoutBtn.classList.remove('hidden');

  startTelemetryPolling();
}

function getAuthHeaders() {
  const token = localStorage.getItem('auth_token');
  return {
    'Content-Type': 'application/json',
    ...(token ? { Authorization: `Bearer ${token}` } : {}),
  };
}

function renderValveState(isOpen) {
  currentValveOpen = Boolean(isOpen);
  const valveStatus = document.getElementById('valveStatus');
  const toggleBtn = document.getElementById('toggleValveBtn');

  if (currentValveOpen) {
    valveStatus.textContent = 'Aberta';
    valveStatus.className = 'px-4 py-2 rounded-lg font-semibold text-sm bg-green-100 text-green-800';
    toggleBtn.textContent = 'Fechar Válvula';
    toggleBtn.className = 'bg-red-600 hover:bg-red-700 text-white px-4 py-2 rounded-lg text-sm font-medium transition';
  } else {
    valveStatus.textContent = 'Fechada';
    valveStatus.className = 'px-4 py-2 rounded-lg font-semibold text-sm bg-gray-100 text-gray-700';
    toggleBtn.textContent = 'Acionar Manualmente';
    toggleBtn.className = 'bg-green-600 hover:bg-green-700 text-white px-4 py-2 rounded-lg text-sm font-medium transition';
  }
}

function renderEspBadge(online, lastSeen) {
  const badge = document.getElementById('espStatusBadge');
  if (!badge) return;
  if (online) {
    badge.textContent = '● ESP32 Conectado';
    badge.className = 'inline-flex items-center px-3 py-1 rounded-full text-xs font-medium bg-green-100 text-green-800';
  } else {
    const suffix = lastSeen ? ` (última: ${new Date(lastSeen).toLocaleTimeString('pt-BR')})` : '';
    badge.textContent = `● ESP32 Offline${suffix}`;
    badge.className = 'inline-flex items-center px-3 py-1 rounded-full text-xs font-medium bg-amber-100 text-amber-800';
  }
}

async function fetchTelemetryAndHistory() {
  const metricTemp = document.getElementById('metricTemp');
  const metricHum = document.getElementById('metricHum');
  const metricSoil = document.getElementById('metricSoil');

  try {
    const [telRes, histRes] = await Promise.all([
      fetch(`${API_BASE}/telemetry?limit=10`, { headers: getAuthHeaders() }),
      fetch(`${API_BASE}/irrigation/history?limit=10`, { headers: getAuthHeaders() }),
    ]);

    if (telRes.status === 401 || histRes.status === 401) {
      logoutBtn.click();
      showMessage('Sessão expirada. Faça login novamente.', true);
      return;
    }

    if (telRes.ok) {
      const tel = await telRes.json();
      if (tel.latest) {
        metricTemp.textContent = tel.latest.temperatura;
        metricHum.textContent = tel.latest.umidade;
        metricSoil.textContent = tel.latest.umidade_solo;
      } else {
        metricTemp.textContent = '--';
        metricHum.textContent = '--';
        metricSoil.textContent = '--';
      }
      renderValveState(tel.valve_open);
      renderEspBadge(tel.esp32_online, tel.last_seen);
    }

    if (histRes.ok) {
      const { history } = await histRes.json();
      renderIrrigationHistory(history || []);
    }
  } catch (err) {
    console.error('Erro ao buscar telemetria:', err);
  }
}

function renderIrrigationHistory(history) {
  const tbody = document.getElementById('irrigationHistoryBody');
  if (!tbody) return;
  tbody.textContent = '';

  if (!history.length) {
    const tr = document.createElement('tr');
    const td = document.createElement('td');
    td.colSpan = 4;
    td.className = 'py-3 text-gray-400';
    td.textContent = 'Nenhum evento de irrigação registrado.';
    tr.appendChild(td);
    tbody.appendChild(tr);
    return;
  }

  history.forEach((ev) => {
    const tr = document.createElement('tr');
    tr.className = 'border-b last:border-b-0';

    const tdDate = document.createElement('td');
    tdDate.className = 'py-2 pr-4';
    tdDate.textContent = ev.criado_em ? new Date(ev.criado_em).toLocaleString('pt-BR') : '--';

    const tdStatus = document.createElement('td');
    tdStatus.className = 'py-2 pr-4 font-medium';
    tdStatus.textContent = ev.status || '--';

    const tdOrigin = document.createElement('td');
    tdOrigin.className = 'py-2 pr-4';
    tdOrigin.textContent = ev.origem || '--';

    const tdDur = document.createElement('td');
    tdDur.className = 'py-2';
    tdDur.textContent = `${ev.duracao_segundos ?? 0}s`;

    tr.append(tdDate, tdStatus, tdOrigin, tdDur);
    tbody.appendChild(tr);
  });
}

function startTelemetryPolling() {
  if (telemetryIntervalId !== null) {
    clearInterval(telemetryIntervalId);
  }
  fetchTelemetryAndHistory();
  telemetryIntervalId = setInterval(fetchTelemetryAndHistory, 5000);
}

const toggleValveBtn = document.getElementById('toggleValveBtn');
if (toggleValveBtn) {
  toggleValveBtn.addEventListener('click', async () => {
    const feedback = document.getElementById('valveFeedback');
    const targetOpen = !currentValveOpen;
    toggleValveBtn.disabled = true;

    try {
      const res = await fetch(`${API_BASE}/irrigation/manual`, {
        method: 'POST',
        headers: getAuthHeaders(),
        body: JSON.stringify({ open: targetOpen }),
      });

      if (res.status === 401) {
        logoutBtn.click();
        return;
      }

      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Falha ao acionar válvula.');
      }

      renderValveState(data.valve_open);
      if (feedback) {
        feedback.textContent = `Comando "${data.valve_open ? 'ABRIR' : 'FECHAR'}" enviado com sucesso.`;
        feedback.className = 'text-xs text-green-700';
        feedback.classList.remove('hidden');
      }
      fetchTelemetryAndHistory();
    } catch (err) {
      if (feedback) {
        feedback.textContent = err.message;
        feedback.className = 'text-xs text-red-600';
        feedback.classList.remove('hidden');
      }
    } finally {
      toggleValveBtn.disabled = false;
    }
  });
}

// Verifica sessão pré-existente
window.addEventListener('DOMContentLoaded', () => {
  const token = localStorage.getItem('auth_token');
  const userData = localStorage.getItem('user_data');

  if (token && userData) {
    try {
      const user = JSON.parse(userData);
      enterDashboard(user);
    } catch {
      localStorage.clear();
    }
  }
});
