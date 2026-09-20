const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const UserModel = require('../models/userModel');
const { validateRegister, validateLogin, validatePassword } = require('../validators/authValidator');

const JWT_SECRET = process.env.JWT_SECRET || 'chave_secreta_provisoria_pibex';

function generateToken(user) {
    return jwt.sign(
        { id: user.id, email: user.email },
        JWT_SECRET,
        { expiresIn: '1d' }
    );
}

const authService = {
    async registerUser(body) {
        const { nome, email, senha } = validateRegister(body);

        const userExists = await UserModel.findByEmail(email);
        if (userExists) {
            throw new Error('Este e-mail já está cadastrado.');
        }

        const salt = await bcrypt.genSalt(10);
        const passwordHash = await bcrypt.hash(senha, salt);

        const newUser = await UserModel.create(nome, email, passwordHash);
        return newUser;
    },

    async loginUser(body) {
        const { email, senha } = validateLogin(body);

        const user = await UserModel.findByEmail(email);
        if (!user) {
            throw new Error('E-mail ou senha incorretos.');
        }

        const isMatch = await bcrypt.compare(senha, user.senha);
        if (!isMatch) {
            throw new Error('E-mail ou senha incorretos.');
        }

        const token = generateToken(user);

        return {
            user: { id: user.id, nome: user.nome, email: user.email },
            token
        };
    },

    async changePassword(userId, body) {
        const { senha_atual, nova_senha } = body || {};

        if (!senha_atual || !nova_senha) {
            throw new Error('Senha atual e nova senha são obrigatórias.');
        }

        const novaSenha = validatePassword(nova_senha, 'nova_senha');

        const user = await UserModel.findById(userId);
        const fullUser = await UserModel.findByEmail(user.email);

        const isMatch = await bcrypt.compare(senha_atual, fullUser.senha);
        if (!isMatch) {
            throw new Error('A senha atual está incorreta.');
        }

        const salt = await bcrypt.genSalt(10);
        const passwordHash = await bcrypt.hash(novaSenha, salt);

        await UserModel.updatePassword(userId, passwordHash);
        return { message: 'Senha alterada com sucesso.' };
    }
};

module.exports = authService;
