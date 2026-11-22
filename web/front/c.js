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

// Logout
logoutBtn.addEventListener('click', () => {
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

  startMetricsSimulation();
}

// Simulação de telemetria em tempo real
function startMetricsSimulation() {
  const metricTemp = document.getElementById('metricTemp');
  const metricHum = document.getElementById('metricHum');
  const metricSoil = document.getElementById('metricSoil');

  function update() {
    // Valores simulados dentro das faixas normais de leitura
    metricTemp.textContent = (26 + Math.random() * 4).toFixed(1);
    metricHum.textContent = Math.round(55 + Math.random() * 15);
    metricSoil.textContent = Math.round(40 + Math.random() * 10);
  }

  update();
  setInterval(update, 5000);
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
