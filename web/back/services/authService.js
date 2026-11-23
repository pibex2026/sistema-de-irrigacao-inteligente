const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const UserModel = require('../models/userModel');
const { getJwtSecret } = require('../configs/env');

function normalizeEmail(email) {
    if (typeof email !== 'string') return '';
    return email.trim().toLowerCase();
}

const authService = {
    async registerUser(nome, email, senha) {
        if (typeof nome !== 'string' || typeof email !== 'string' || typeof senha !== 'string') {
            const err = new Error('Tipos de dados inválidos.');
            err.statusCode = 400;
            throw err;
        }

        const cleanNome = nome.trim();
        const cleanEmail = normalizeEmail(email);

        if (cleanNome.length < 2 || cleanNome.length > 100) {
            const err = new Error('O nome deve ter entre 2 e 100 caracteres.');
            err.statusCode = 400;
            throw err;
        }

        if (senha.length < 8 || senha.length > 128) {
            const err = new Error('A senha deve ter entre 8 e 128 caracteres.');
            err.statusCode = 400;
            throw err;
        }

        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        if (!cleanEmail || cleanEmail.length > 255 || !emailRegex.test(cleanEmail)) {
            const err = new Error('Formato de e-mail inválido.');
            err.statusCode = 400;
            throw err;
        }

        const userExists = await UserModel.findByEmail(cleanEmail);
        if (userExists) {
            const err = new Error('Este e-mail já está cadastrado.');
            err.statusCode = 409;
            throw err;
        }

        const salt = await bcrypt.genSalt(10);
        const passwordHash = await bcrypt.hash(senha, salt);

        const newUser = await UserModel.create(cleanNome, cleanEmail, passwordHash);
        return newUser;
    },

    async loginUser(email, senha) {
        if (typeof email !== 'string' || typeof senha !== 'string') {
            const err = new Error('E-mail e senha devem ser strings.');
            err.statusCode = 400;
            throw err;
        }

        const cleanEmail = normalizeEmail(email);
        if (!cleanEmail || !senha) {
            const err = new Error('E-mail e senha são obrigatórios.');
            err.statusCode = 400;
            throw err;
        }

        const user = await UserModel.findByEmail(cleanEmail);
        if (!user) {
            const err = new Error('E-mail ou senha incorretos.');
            err.statusCode = 401;
            throw err;
        }

        const isMatch = await bcrypt.compare(senha, user.senha);
        if (!isMatch) {
            const err = new Error('E-mail ou senha incorretos.');
            err.statusCode = 401;
            throw err;
        }

        const secret = getJwtSecret();

        const token = jwt.sign(
            { id: user.id, email: user.email },
            secret,
            { expiresIn: '1d' }
        );

        return {
            user: { id: user.id, nome: user.nome, email: user.email },
            token
        };
    },

    async getProfile(userId) {
        const user = await UserModel.findById(userId);
        if (!user) {
            const err = new Error('Usuário não encontrado.');
            err.statusCode = 404;
            throw err;
        }
        return user;
    },

    async changePassword(userId, senhaAtual, novaSenha) {
        if (typeof senhaAtual !== 'string' || typeof novaSenha !== 'string') {
            const err = new Error('Tipos de dados inválidos.');
            err.statusCode = 400;
            throw err;
        }
        if (novaSenha.length < 8 || novaSenha.length > 128) {
            const err = new Error('A nova senha deve ter entre 8 e 128 caracteres.');
            err.statusCode = 400;
            throw err;
        }

        const user = await UserModel.findByIdWithPassword(userId);
        if (!user) {
            const err = new Error('Usuário não encontrado.');
            err.statusCode = 404;
            throw err;
        }

        const isMatch = await bcrypt.compare(senhaAtual, user.senha);
        if (!isMatch) {
            const err = new Error('Senha atual incorreta.');
            err.statusCode = 401;
            throw err;
        }

        const salt = await bcrypt.genSalt(10);
        const passwordHash = await bcrypt.hash(novaSenha, salt);
        await UserModel.updatePassword(userId, passwordHash);
        return { ok: true, message: 'Senha alterada com sucesso.' };
    }
};

module.exports = authService;