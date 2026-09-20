function isValidEmail(email) {
    return typeof email === 'string' && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

function validateEmail(email) {
    if (!email || !isValidEmail(email)) {
        throw new Error('E-mail inválido.');
    }
    return email.trim().toLowerCase();
}

function validatePassword(senha, field = 'senha') {
    if (!senha || senha.length < 8) {
        throw new Error(`O campo '${field}' deve ter no mínimo 8 caracteres.`);
    }
    return senha;
}

function validateRegister(body) {
    const { nome, email, senha } = body || {};

    if (!nome || !String(nome).trim()) {
        throw new Error('O campo nome é obrigatório.');
    }
    if (!email) {
        throw new Error('O campo e-mail é obrigatório.');
    }
    if (!senha) {
        throw new Error('O campo senha é obrigatório.');
    }

    return {
        nome: String(nome).trim(),
        email: validateEmail(email),
        senha: validatePassword(senha)
    };
}

function validateLogin(body) {
    const { email, senha } = body || {};
    if (!email || !senha) {
        throw new Error('E-mail e senha são obrigatórios.');
    }
    return { email: validateEmail(email), senha };
}

module.exports = { validateRegister, validateLogin, validatePassword };
