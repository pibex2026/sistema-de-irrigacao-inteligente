const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const UserModel = require('../models/userModel');
const { getJwtSecret } = require('../configs/env');

const authService = {
    async registerUser(nome, email, senha) {
        if (!senha || senha.length < 8) {
            const err = new Error('A senha deve ter no mínimo 8 caracteres.');
            err.statusCode = 400;
            throw err;
        }

        const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        if (!email || !emailRegex.test(email)) {
            const err = new Error('Formato de e-mail inválido.');
            err.statusCode = 400;
            throw err;
        }

        const userExists = await UserModel.findByEmail(email);
        if (userExists) {
            const err = new Error('Este e-mail já está cadastrado.');
            err.statusCode = 409;
            throw err;
        }

        const salt = await bcrypt.genSalt(10);
        const passwordHash = await bcrypt.hash(senha, salt);

        const newUser = await UserModel.create(nome, email, passwordHash);
        return newUser;
    },

    async loginUser(email, senha) {
        const user = await UserModel.findByEmail(email);
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
    }
};

module.exports = authService;