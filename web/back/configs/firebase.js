const admin = require('firebase-admin');

// Inicialização opcional do Firebase Admin (se fornecido via variáveis de ambiente)
let firebaseApp = null;

try {
    if (process.env.FIREBASE_SERVICE_ACCOUNT) {
        const serviceAccount = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT);
        firebaseApp = admin.initializeApp({
            credential: admin.credential.cert(serviceAccount)
        });
        console.log('[Firebase] Inicializado com sucesso via Service Account.');
    } else if (process.env.GOOGLE_APPLICATION_CREDENTIALS) {
        firebaseApp = admin.initializeApp();
        console.log('[Firebase] Inicializado com sucesso via GOOGLE_APPLICATION_CREDENTIALS.');
    } else {
        // Firebase não configurado neste ambiente
    }
} catch (error) {
    console.warn('[Firebase] Aviso: Falha ao inicializar Firebase Admin:', error.message);
}

module.exports = {
    admin,
    firebaseApp
};
