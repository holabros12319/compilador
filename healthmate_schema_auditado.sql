-- ============================================================
-- HealthMate - Esquema PostgreSQL auditado y mejorado
-- Alineado con el modelo de datos del proyecto (ER del informe)
-- ============================================================

-- 1. EXTENSIONES
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- 2. TABLA DE AUDITORÍA (PRIMERO, para que los triggers la encuentren)
CREATE TABLE auditoria (
    id SERIAL PRIMARY KEY,
    usuario_id UUID,
    accion TEXT NOT NULL,
    tabla_afectada VARCHAR(100),
    registro_id UUID,
    ip VARCHAR(50),
    user_agent TEXT,
    endpoint TEXT,
    metodo_http VARCHAR(10),
    severidad VARCHAR(20) CHECK (severidad IN ('info','warning','critical')),
    fecha TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- 3. FUNCIÓN DE AUDITORÍA
-- Mejora: usa TIMESTAMPTZ implícito vía CURRENT_TIMESTAMP del servidor;
-- captura el usuario de app si se configura con set_config('app.current_user_id', ..., true)
CREATE OR REPLACE FUNCTION fn_auditar_sistema_medico()
RETURNS TRIGGER AS $$
DECLARE
    id_registro UUID;
    usuario_app UUID;
BEGIN
    IF (TG_OP = 'DELETE') THEN id_registro := OLD.id;
    ELSE id_registro := NEW.id; END IF;

    BEGIN
        usuario_app := current_setting('app.current_user_id', true)::uuid;
    EXCEPTION WHEN OTHERS THEN
        usuario_app := NULL;
    END;

    INSERT INTO auditoria (usuario_id, accion, tabla_afectada, registro_id, severidad)
    VALUES (usuario_app, TG_OP, TG_TABLE_NAME, id_registro,
        CASE
            WHEN TG_OP = 'DELETE' THEN 'critical'
            WHEN TG_OP = 'UPDATE' THEN 'warning'
            ELSE 'info'
        END);

    IF (TG_OP = 'DELETE') THEN RETURN OLD; END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- 4. USUARIOS Y ROLES (roles alineados al proyecto: paciente, cuidador, admin)
CREATE TABLE usuarios (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    estado BOOLEAN NOT NULL DEFAULT TRUE,
    fecha_creacion TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE roles (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    nombre VARCHAR(50) UNIQUE NOT NULL
);

INSERT INTO roles (nombre) VALUES ('admin'), ('cuidador'), ('paciente')
    ON CONFLICT (nombre) DO NOTHING;

CREATE TABLE usuario_rol (
    usuario_id UUID NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
    rol_id UUID NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
    PRIMARY KEY (usuario_id, rol_id)
);

-- 5. PERFILES Y SALUD
CREATE TABLE perfiles (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    usuario_id UUID NOT NULL UNIQUE REFERENCES usuarios(id) ON DELETE CASCADE,
    nombre_completo VARCHAR(255),
    telefono VARCHAR(20),
    fecha_nacimiento DATE,
    tipo_sangre VARCHAR(5)
);

CREATE TABLE antecedentes (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    perfil_id UUID NOT NULL REFERENCES perfiles(id) ON DELETE CASCADE,
    tipo VARCHAR(50),
    descripcion TEXT,
    fecha_registro TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE alergias (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    nombre VARCHAR(100) UNIQUE NOT NULL,
    categoria VARCHAR(50),
    descripcion TEXT
);

CREATE TABLE perfil_alergias (
    perfil_id UUID NOT NULL REFERENCES perfiles(id) ON DELETE CASCADE,
    alergia_id UUID NOT NULL REFERENCES alergias(id) ON DELETE CASCADE,
    severidad VARCHAR(50) CHECK (severidad IN ('leve','moderada','grave')),
    PRIMARY KEY (perfil_id, alergia_id)
);

-- 6. GESTIÓN DE MEDICAMENTOS (alineado al ER: Usuario N:M Medicamento)
CREATE TABLE medicamentos (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    nombre VARCHAR(255) NOT NULL,
    descripcion TEXT,
    compuesto_activo VARCHAR(255),
    UNIQUE (nombre, compuesto_activo)
);

CREATE TABLE usuario_medicamento (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    usuario_id UUID NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
    medicamento_id UUID NOT NULL REFERENCES medicamentos(id) ON DELETE RESTRICT,
    dosis VARCHAR(100),
    frecuencia VARCHAR(100),
    fecha_inicio DATE,
    fecha_fin DATE,
    activo BOOLEAN NOT NULL DEFAULT TRUE,
    CHECK (fecha_fin IS NULL OR fecha_fin >= fecha_inicio)
);

CREATE TABLE recordatorios (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    usuario_medicamento_id UUID NOT NULL REFERENCES usuario_medicamento(id) ON DELETE CASCADE,
    hora TIME NOT NULL,
    dias_semana VARCHAR(20),
    activo BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE TABLE dosis_tomada (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    recordatorio_id UUID NOT NULL REFERENCES recordatorios(id) ON DELETE CASCADE,
    fecha_programada TIMESTAMPTZ NOT NULL,
    fecha_confirmacion TIMESTAMPTZ,
    estado VARCHAR(50) NOT NULL DEFAULT 'pendiente'
        CHECK (estado IN ('pendiente','tomado','omitido','pospuesto')),
    CHECK (fecha_confirmacion IS NULL OR fecha_confirmacion >= fecha_programada)
);

-- Trigger: auto-cambio de estado al confirmar toma
CREATE OR REPLACE FUNCTION fn_actualizar_estado_toma()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.fecha_confirmacion IS NOT NULL AND OLD.fecha_confirmacion IS NULL THEN
        NEW.estado := 'tomado';
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER tr_auto_estado_toma
    BEFORE UPDATE ON dosis_tomada
    FOR EACH ROW EXECUTE FUNCTION fn_actualizar_estado_toma();

-- 7. CITAS MÉDICAS (alineado al ER: Usuario 1:N Cita)
CREATE TABLE citas (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    paciente_id UUID NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
    especialista VARCHAR(255),
    lugar VARCHAR(255),
    fecha_hora TIMESTAMPTZ NOT NULL,
    motivo TEXT,
    estado VARCHAR(50) NOT NULL DEFAULT 'pendiente'
        CHECK (estado IN ('pendiente','confirmada','cancelada','completada'))
);

-- 8. SIGNOS Y SÍNTOMAS
CREATE TABLE signos_vitales (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    paciente_id UUID NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
    presion_sistolica INT CHECK (presion_sistolica BETWEEN 40 AND 250),
    presion_diastolica INT CHECK (presion_diastolica BETWEEN 30 AND 150),
    frecuencia_cardiaca INT CHECK (frecuencia_cardiaca > 0),
    oxigenacion INT CHECK (oxigenacion BETWEEN 0 AND 100),
    glucosa DECIMAL(5,2),
    temperatura DECIMAL(4,2) CHECK (temperatura BETWEEN 30 AND 45),
    fecha_registro TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE catalogo_sintomas (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    nombre VARCHAR(100) UNIQUE NOT NULL
);

CREATE TABLE registro_sintomas (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    paciente_id UUID NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
    sintoma_id UUID NOT NULL REFERENCES catalogo_sintomas(id) ON DELETE RESTRICT,
    intensidad INT NOT NULL CHECK (intensidad BETWEEN 1 AND 10),
    fecha TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- 9. TRIGGERS DE AUDITORÍA (tablas con datos sensibles)
CREATE TRIGGER tr_audita_usuarios
    AFTER INSERT OR UPDATE OR DELETE ON usuarios
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_perfiles
    AFTER INSERT OR UPDATE OR DELETE ON perfiles
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_usuario_medicamento
    AFTER INSERT OR UPDATE OR DELETE ON usuario_medicamento
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_recordatorios
    AFTER INSERT OR UPDATE OR DELETE ON recordatorios
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_dosis_tomada
    AFTER INSERT OR UPDATE OR DELETE ON dosis_tomada
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_citas
    AFTER INSERT OR UPDATE OR DELETE ON citas
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_signos
    AFTER INSERT OR UPDATE OR DELETE ON signos_vitales
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

CREATE TRIGGER tr_audita_registro_sintomas
    AFTER INSERT OR UPDATE OR DELETE ON registro_sintomas
    FOR EACH ROW EXECUTE FUNCTION fn_auditar_sistema_medico();

-- 10. ÍNDICES
CREATE INDEX idx_usuarios_email_search ON usuarios(email);
CREATE INDEX idx_citas_fecha_search ON citas(fecha_hora);
CREATE INDEX idx_citas_paciente_fecha ON citas(paciente_id, fecha_hora);
CREATE INDEX idx_auditoria_fecha_search ON auditoria(fecha);

CREATE INDEX idx_perfiles_usuario ON perfiles(usuario_id);
CREATE INDEX idx_antecedentes_perfil ON antecedentes(perfil_id);
CREATE INDEX idx_perfil_alergias_perfil ON perfil_alergias(perfil_id);

CREATE INDEX idx_usuario_medicamento_usuario ON usuario_medicamento(usuario_id);
CREATE INDEX idx_usuario_medicamento_medicamento ON usuario_medicamento(medicamento_id);
CREATE INDEX idx_recordatorios_um ON recordatorios(usuario_medicamento_id);
CREATE INDEX idx_dosis_tomada_recordatorio ON dosis_tomada(recordatorio_id);
CREATE INDEX idx_dosis_tomada_fecha_prog ON dosis_tomada(fecha_programada);

CREATE INDEX idx_signos_paciente_fecha ON signos_vitales(paciente_id, fecha_registro);
CREATE INDEX idx_registro_sintomas_paciente_fecha ON registro_sintomas(paciente_id, fecha);
CREATE INDEX idx_registro_sintomas_sintoma ON registro_sintomas(sintoma_id);
