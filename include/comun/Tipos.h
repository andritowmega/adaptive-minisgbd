#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace minisgbd {

// Identificador de registro: página + slot dentro de la página.
struct RID {
    uint32_t numeroPagina = 0;
    uint16_t numeroSlot = 0;

    bool operator==(const RID& otro) const {
        return numeroPagina == otro.numeroPagina && numeroSlot == otro.numeroSlot;
    }
    bool operator<(const RID& otro) const {
        if (numeroPagina != otro.numeroPagina) return numeroPagina < otro.numeroPagina;
        return numeroSlot < otro.numeroSlot;
    }
};

enum class TipoDato {
    ENTERO,
    TEXTO
};

// Valor de una columna. TEXTO se almacena como std::string ya truncado/rellenado
// a la longitud fija de la columna (ver Columna::longitudTexto).
using Valor = std::variant<int, std::string>;

struct Columna {
    std::string nombre;
    TipoDato tipo;
    size_t longitudTexto = 0;  // solo relevante si tipo == TEXTO (longitud fija)

    size_t tamanoBytes() const {
        return tipo == TipoDato::ENTERO ? sizeof(int) : longitudTexto;
    }
};

// Esquema de una tabla. Todos los registros de la tabla tienen longitud fija,
// determinada por la suma de tamanoBytes() de sus columnas — esto simplifica
// la página tipo slotted (una actualización nunca cambia el tamaño del registro).
struct Esquema {
    std::string nombreTabla;
    std::vector<Columna> columnas;

    size_t tamanoRegistro() const {
        size_t total = 0;
        for (const auto& columna : columnas) total += columna.tamanoBytes();
        return total;
    }

    int indiceColumna(const std::string& nombre) const {
        for (size_t i = 0; i < columnas.size(); ++i) {
            if (columnas[i].nombre == nombre) return static_cast<int>(i);
        }
        return -1;
    }
};

// Tipo de índice que el Motor de Decisión puede elegir para una columna.
enum class TipoIndice {
    NINGUNO,
    HASH,
    BMAS
};

// Clasificación de un filtro WHERE, tal como lo distingue el Parser: es lo
// único que el Motor de Decisión necesita saber de cada consulta.
enum class TipoFiltro {
    IGUALDAD,
    RANGO
};

}  // namespace minisgbd
