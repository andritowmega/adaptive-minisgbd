#include "consultas/Parser.h"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace minisgbd {

namespace {

enum class TipoToken { PALABRA, NUMERO, CADENA, SIMBOLO, FIN };

struct Token {
    TipoToken tipo;
    std::string texto;
};

std::string aMayusculas(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

std::vector<Token> tokenizar(const std::string& linea) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t n = linea.size();

    while (i < n) {
        char c = linea[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            i++;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t inicio = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(linea[i])) || linea[i] == '_')) i++;
            tokens.push_back({TipoToken::PALABRA, linea.substr(inicio, i - inicio)});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '-' && i + 1 < n && std::isdigit(static_cast<unsigned char>(linea[i + 1])))) {
            size_t inicio = i;
            i++;
            while (i < n && std::isdigit(static_cast<unsigned char>(linea[i]))) i++;
            tokens.push_back({TipoToken::NUMERO, linea.substr(inicio, i - inicio)});
            continue;
        }
        if (c == '\'' || c == '"') {
            char comilla = c;
            i++;
            size_t inicio = i;
            while (i < n && linea[i] != comilla) i++;
            tokens.push_back({TipoToken::CADENA, linea.substr(inicio, i - inicio)});
            if (i < n) i++;  // consumir la comilla de cierre
            continue;
        }
        if (c == '>' || c == '<') {
            if (i + 1 < n && linea[i + 1] == '=') {
                tokens.push_back({TipoToken::SIMBOLO, std::string(1, c) + "="});
                i += 2;
            } else {
                tokens.push_back({TipoToken::SIMBOLO, std::string(1, c)});
                i++;
            }
            continue;
        }
        if (c == '(' || c == ')' || c == ',' || c == ':' || c == '=' || c == '*') {
            tokens.push_back({TipoToken::SIMBOLO, std::string(1, c)});
            i++;
            continue;
        }
        throw std::runtime_error(std::string("Parser: caracter inesperado '") + c + "'");
    }
    tokens.push_back({TipoToken::FIN, ""});
    return tokens;
}

// Cursor sobre la secuencia de tokens con helpers de consumo/verificación.
// actual() nunca se sale de rango: una vez alcanzado FIN, se queda ahí.
class Cursor {
public:
    explicit Cursor(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    const Token& actual() const { return tokens_[pos_]; }

    Token avanzar() {
        Token t = actual();
        if (pos_ + 1 < tokens_.size()) pos_++;
        return t;
    }

    bool esPalabra(const std::string& palabraEnMayusculas) const {
        return actual().tipo == TipoToken::PALABRA && aMayusculas(actual().texto) == palabraEnMayusculas;
    }
    bool esSimbolo(const std::string& simbolo) const {
        return actual().tipo == TipoToken::SIMBOLO && actual().texto == simbolo;
    }

    void esperarPalabra(const std::string& palabraEnMayusculas) {
        if (!esPalabra(palabraEnMayusculas)) {
            throw std::runtime_error("Parser: se esperaba '" + palabraEnMayusculas + "' cerca de '" +
                                      actual().texto + "'");
        }
        avanzar();
    }
    void esperarSimbolo(const std::string& simbolo) {
        if (!esSimbolo(simbolo)) {
            throw std::runtime_error("Parser: se esperaba '" + simbolo + "' cerca de '" + actual().texto + "'");
        }
        avanzar();
    }
    std::string esperarIdentificador() {
        if (actual().tipo != TipoToken::PALABRA) {
            throw std::runtime_error("Parser: se esperaba un identificador cerca de '" + actual().texto + "'");
        }
        return avanzar().texto;
    }

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
};

Valor parsearValorLiteral(Cursor& cursor) {
    if (cursor.actual().tipo == TipoToken::NUMERO) {
        return Valor(std::stoi(cursor.avanzar().texto));
    }
    if (cursor.actual().tipo == TipoToken::CADENA) {
        return Valor(cursor.avanzar().texto);
    }
    if (cursor.actual().tipo == TipoToken::PALABRA) {
        // Caso comun: el usuario escribio un texto sin comillas (p.ej. `andres`
        // en vez de `'andres'`). Aca siempre se espera un VALOR, nunca un
        // nombre de tabla o columna, asi que el mensaje sugiere la correccion
        // en vez de solo rechazar el token.
        throw std::runtime_error("Parser: '" + cursor.actual().texto +
                                  "' parece un valor de texto sin comillas — escribilo como '" +
                                  cursor.actual().texto + "' (con comillas simples o dobles)");
    }
    throw std::runtime_error("Parser: se esperaba un valor (numero o cadena entre comillas) cerca de '" +
                              cursor.actual().texto + "'");
}

PlanConsulta parsearCrearTabla(Cursor& cursor) {
    PlanConsulta plan;
    plan.operacion = TipoOperacion::CREAR_TABLA;
    cursor.esperarPalabra("TABLA");
    plan.tabla = cursor.esperarIdentificador();
    cursor.esperarSimbolo("(");

    while (true) {
        Columna columna;
        columna.nombre = cursor.esperarIdentificador();
        cursor.esperarSimbolo(":");
        std::string tipoTexto = aMayusculas(cursor.esperarIdentificador());

        if (tipoTexto == "ENTERO") {
            columna.tipo = TipoDato::ENTERO;
        } else if (tipoTexto == "TEXTO") {
            columna.tipo = TipoDato::TEXTO;
            cursor.esperarSimbolo("(");
            if (cursor.actual().tipo != TipoToken::NUMERO) {
                throw std::runtime_error("Parser: se esperaba la longitud de TEXTO entre parentesis");
            }
            columna.longitudTexto = static_cast<size_t>(std::stoul(cursor.avanzar().texto));
            cursor.esperarSimbolo(")");
        } else {
            throw std::runtime_error("Parser: tipo de dato desconocido '" + tipoTexto + "'");
        }

        plan.columnasDefinicion.push_back(columna);
        if (cursor.esSimbolo(",")) {
            cursor.avanzar();
            continue;
        }
        break;
    }
    cursor.esperarSimbolo(")");
    return plan;
}

PlanConsulta parsearInsertar(Cursor& cursor) {
    PlanConsulta plan;
    plan.operacion = TipoOperacion::INSERTAR;
    cursor.esperarPalabra("EN");
    plan.tabla = cursor.esperarIdentificador();
    cursor.esperarPalabra("VALORES");
    cursor.esperarSimbolo("(");

    while (true) {
        plan.valoresInsertar.push_back(parsearValorLiteral(cursor));
        if (cursor.esSimbolo(",")) {
            cursor.avanzar();
            continue;
        }
        break;
    }
    cursor.esperarSimbolo(")");
    return plan;
}

CondicionFiltro parsearCondicionDonde(Cursor& cursor) {
    CondicionFiltro filtro;
    filtro.presente = true;
    filtro.columna = cursor.esperarIdentificador();

    if (cursor.esSimbolo("=")) {
        cursor.avanzar();
        filtro.tipo = TipoFiltro::IGUALDAD;
        filtro.valorIgualdad = parsearValorLiteral(cursor);
    } else if (cursor.esPalabra("ENTRE")) {
        cursor.avanzar();
        filtro.tipo = TipoFiltro::RANGO;
        filtro.valorDesde = parsearValorLiteral(cursor);
        cursor.esperarPalabra("Y");
        filtro.valorHasta = parsearValorLiteral(cursor);
    } else if (cursor.esSimbolo(">") || cursor.esSimbolo(">=")) {
        cursor.avanzar();
        filtro.tipo = TipoFiltro::RANGO;
        filtro.tieneCotaSuperior = false;
        filtro.valorDesde = parsearValorLiteral(cursor);
    } else if (cursor.esSimbolo("<") || cursor.esSimbolo("<=")) {
        cursor.avanzar();
        filtro.tipo = TipoFiltro::RANGO;
        filtro.tieneCotaInferior = false;
        filtro.valorHasta = parsearValorLiteral(cursor);
    } else {
        throw std::runtime_error("Parser: se esperaba '=', 'ENTRE', '>', '>=', '<' o '<=' en la condicion DONDE");
    }
    return filtro;
}

PlanConsulta parsearSeleccionar(Cursor& cursor) {
    PlanConsulta plan;
    plan.operacion = TipoOperacion::SELECCIONAR;
    cursor.esperarSimbolo("*");
    cursor.esperarPalabra("DE");
    plan.tabla = cursor.esperarIdentificador();
    if (cursor.esPalabra("DONDE")) {
        cursor.avanzar();
        plan.filtro = parsearCondicionDonde(cursor);
    }
    if (cursor.esPalabra("ORDENAR")) {
        cursor.avanzar();
        cursor.esperarPalabra("POR");
        plan.tieneOrden = true;
        plan.columnaOrden = cursor.esperarIdentificador();
        if (cursor.esPalabra("DESC")) {
            cursor.avanzar();
            plan.ordenDescendente = true;
        } else if (cursor.esPalabra("ASC")) {
            cursor.avanzar();
        }
    }
    return plan;
}

PlanConsulta parsearEliminar(Cursor& cursor) {
    PlanConsulta plan;
    plan.operacion = TipoOperacion::ELIMINAR;
    cursor.esperarPalabra("DE");
    plan.tabla = cursor.esperarIdentificador();
    cursor.esperarPalabra("DONDE");
    plan.filtro = parsearCondicionDonde(cursor);
    return plan;
}

}  // namespace

PlanConsulta Parser::parsear(const std::string& linea) {
    Cursor cursor(tokenizar(linea));

    if (cursor.actual().tipo != TipoToken::PALABRA) {
        throw std::runtime_error("Parser: se esperaba un comando (CREAR, INSERTAR, SELECCIONAR, ELIMINAR)");
    }
    std::string comando = aMayusculas(cursor.avanzar().texto);

    PlanConsulta plan;
    if (comando == "CREAR") {
        plan = parsearCrearTabla(cursor);
    } else if (comando == "INSERTAR") {
        plan = parsearInsertar(cursor);
    } else if (comando == "SELECCIONAR") {
        plan = parsearSeleccionar(cursor);
    } else if (comando == "ELIMINAR") {
        plan = parsearEliminar(cursor);
    } else {
        throw std::runtime_error("Parser: comando desconocido '" + comando + "'");
    }

    if (cursor.actual().tipo != TipoToken::FIN) {
        throw std::runtime_error("Parser: texto sobrante despues del comando cerca de '" + cursor.actual().texto +
                                  "'");
    }
    return plan;
}

}  // namespace minisgbd
