const authService = require('../services/authService');

const authController = {
    async register(req, res) {
        try {
            const newUser = await authService.registerUser(req.body);
            return res.status(201).json(newUser);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    },

    async login(req, res) {
        try {
            const result = await authService.loginUser(req.body);
            return res.status(200).json(result);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    },

    async changePassword(req, res) {
        try {
            const result = await authService.changePassword(req.userId, req.body);
            return res.status(200).json(result);
        } catch (error) {
            return res.status(400).json({ error: error.message });
        }
    }
};

module.exports = authController;
