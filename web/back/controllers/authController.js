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
            return res.status(400).json({ error: error.message });
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
            return res.status(400).json({ error: error.message });
        }
    }
};

module.exports = authController;