const authService = require('../services/authService');

const authController = {
    async register(req, res) {
        try {
            const { nome, email, senha } = req.body;

            if (!nome || !email || !senha) {
                return res.status(400).json({ error: 'Todos os campos são obrigatórios.' });
            }

            const newUser = await authService.registerUser(nome, email, senha);
            return res.status(201).json(newUser);
        } catch (error) {
            if (error.statusCode) {
                return res.status(error.statusCode).json({ error: error.message });
            }
            console.error('Erro no cadastro:', error);
            return res.status(500).json({ error: 'Erro interno ao processar cadastro.' });
        }
    },

    async login(req, res) {
        try {
            const { email, senha } = req.body;

            if (!email || !senha) {
                return res.status(400).json({ error: 'E-mail e senha são obrigatórios.' });
            }

            const result = await authService.loginUser(email, senha);
            return res.status(200).json(result);
        } catch (error) {
            if (error.statusCode) {
                return res.status(error.statusCode).json({ error: error.message });
            }
            console.error('Erro no login:', error);
            return res.status(500).json({ error: 'Erro interno ao processar login.' });
        }
    },

    async me(req, res) {
        try {
            const user = await authService.getProfile(req.user.id);
            return res.status(200).json({ user });
        } catch (error) {
            if (error.statusCode) {
                return res.status(error.statusCode).json({ error: error.message });
            }
            console.error('Erro ao buscar perfil:', error);
            return res.status(500).json({ error: 'Erro interno ao buscar perfil.' });
        }
    },

    async changePassword(req, res) {
        try {
            const { senhaAtual, novaSenha } = req.body || {};
            if (!senhaAtual || !novaSenha) {
                return res.status(400).json({ error: 'Senha atual e nova senha são obrigatórias.' });
            }
            const result = await authService.changePassword(req.user.id, senhaAtual, novaSenha);
            return res.status(200).json(result);
        } catch (error) {
            if (error.statusCode) {
                return res.status(error.statusCode).json({ error: error.message });
            }
            console.error('Erro ao alterar senha:', error);
            return res.status(500).json({ error: 'Erro interno ao alterar senha.' });
        }
    }
};

module.exports = authController;