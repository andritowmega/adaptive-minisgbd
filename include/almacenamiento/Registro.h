#pragma once

#include <cstdint>
#include <vector>

#include "comun/Tipos.h"

namespace minisgbd {

// Convierte una tupla (vector<Valor>) a/desde su representación binaria de
// longitud fija, según el Esquema de la tabla. TEXTO se trunca o rellena con
// ceros hasta Columna::longitudTexto.
class Registro {
public:
    static std::vector<uint8_t> serializar(const std::vector<Valor>& valores, const Esquema& esquema);
    static std::vector<Valor> deserializar(const std::vector<uint8_t>& datos, const Esquema& esquema);
};

}  // namespace minisgbd
