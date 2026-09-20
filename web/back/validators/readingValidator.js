function toNumber(value, field) {
    if (value === undefined || value === null || value === '') {
        return null;
    }
    const num = Number(value);
    if (!Number.isFinite(num)) {
        throw new Error(`O campo '${field}' deve ser um número válido.`);
    }
    return num;
}

function validateReading(body) {
    const temperatura = toNumber(body.temperatura, 'temperatura');
    const umidade = toNumber(body.umidade, 'umidade');
    const umidadeSolo = toNumber(body.umidade_solo, 'umidade_solo');

    if (umidade !== null && (umidade < 0 || umidade > 100)) {
        throw new Error("O campo 'umidade' deve estar entre 0 e 100.");
    }
    if (umidadeSolo !== null && (umidadeSolo < 0 || umidadeSolo > 100)) {
        throw new Error("O campo 'umidade_solo' deve estar entre 0 e 100.");
    }

    let timestamp = body.timestamp || null;
    if (timestamp !== null && Number.isNaN(Date.parse(timestamp))) {
        throw new Error("O campo 'timestamp' deve ser uma data válida.");
    }

    return { temperatura, umidade, umidadeSolo, timestamp };
}

module.exports = { validateReading };
