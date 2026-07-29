#pragma once

#include <cstdint>
#include <cstring>

#include "almacenamiento/Pagina.h"

namespace minisgbd::util_pagina {

// Los índices (Hash Extensible, B+ Tree) usan páginas con su propio formato
// binario crudo, distinto del slotted page de Registro — estos helpers leen
// y escriben enteros de ancho fijo en offsets arbitrarios de una Pagina.
// Nunca se mezclan con los métodos insertarRegistro/obtenerRegistro de Pagina.

inline uint32_t leerU32(const Pagina& pagina, size_t offset) {
    uint32_t valor;
    std::memcpy(&valor, pagina.bytesCrudos() + offset, sizeof(valor));
    return valor;
}

inline void escribirU32(Pagina& pagina, size_t offset, uint32_t valor) {
    std::memcpy(pagina.bytesCrudos() + offset, &valor, sizeof(valor));
}

inline uint16_t leerU16(const Pagina& pagina, size_t offset) {
    uint16_t valor;
    std::memcpy(&valor, pagina.bytesCrudos() + offset, sizeof(valor));
    return valor;
}

inline void escribirU16(Pagina& pagina, size_t offset, uint16_t valor) {
    std::memcpy(pagina.bytesCrudos() + offset, &valor, sizeof(valor));
}

inline uint8_t leerU8(const Pagina& pagina, size_t offset) { return pagina.bytesCrudos()[offset]; }

inline void escribirU8(Pagina& pagina, size_t offset, uint8_t valor) { pagina.bytesCrudos()[offset] = valor; }

}  // namespace minisgbd::util_pagina
