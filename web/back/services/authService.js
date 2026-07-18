const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const UserModel = require('../models/userModel');

const authService = {
    async registerUser(nome, email, senha) {
        if (!senha || senha.length < 8) {
            throw new Error('A senha deve ter no mínimo 8 caracteres.');
        }

        const userExists = await UserModel.findByEmail(email);
        if (userExists) {
            throw new Error('Este e-mail já está cadastrado.');
        }

        const salt = await bcrypt.genSalt(10);
        const passwordHash = await bcrypt.hash(senha, salt);

        const newUser = await UserModel.create(nome, email, passwordHash);
        return newUser;
    },

    async loginUser(email, senha) {
        const user = await UserModel.findByEmail(email);
        if (!user) {
            throw new Error('E-mail ou senha incorretos.');
        }

        const isMatch = await bcrypt.compare(senha, user.senha);
        if (!isMatch) {
            throw new Error('E-mail ou senha incorretos.');
        }

        const token = jwt.sign(
            { id: user.id, email: user.email },
            process.env.JWT_SECRET || 'chave_secreta_provisoria_pibex',
            { expiresIn: '1d' }
        );

        return {
            user: { id: user.id, nome: user.nome, email: user.email },
            token
        };
    }
};

module.exports = authService;