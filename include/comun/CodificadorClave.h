#pragma once

#include <cstdint>
#include <vector>

#include "comun/Tipos.h"

namespace minisgbd {

// Convierte un Valor de columna a una secuencia de bytes de longitud fija
// que se puede comparar con memcmp() y obtener el mismo orden que la
// comparación "natural" del valor original. Esto es lo que necesitan los
// índices (Hash Extensible solo para igualdad, B+ Tree además para orden)
// para trabajar sobre bytes crudos sin conocer el tipo de dato en tiempo de
// ejecución.
//
// ENTERO: se codifica en big-endian con el bit de signo invertido, truco
// clásico para que la comparación de enteros con signo coincida con la
// comparación lexicográfica de sus bytes sin signo.
// TEXTO: se trunca o rellena con ceros a la derecha hasta longitudTexto; el
// relleno con 0x00 preserva el orden lexicográfico habitual de strings.
class CodificadorClave {
public:
    static std::vector<uint8_t> codificar(const Valor& valor, const Columna& columna);
    static Valor decodificar(const std::vector<uint8_t>& clave, const Columna& columna);

    static size_t longitudClave(const Columna& columna) { return columna.tamanoBytes(); }

    // Sentinelas para rangos abiertos (WHERE col > v, WHERE col < v): el
    // mínimo y máximo valor representable en la codificación de esta
    // columna, útiles como extremo de un buscarRango() cuando el filtro no
    // tiene cota inferior o superior.
    static std::vector<uint8_t> minimo(const Columna& columna) {
        return std::vector<uint8_t>(columna.tamanoBytes(), 0x00);
    }
    static std::vector<uint8_t> maximo(const Columna& columna) {
        return std::vector<uint8_t>(columna.tamanoBytes(), 0xFF);
    }

private:
    static std::vector<uint8_t> codificarEntero(int valor);
    static int decodificarEntero(const std::vector<uint8_t>& clave);
    static std::vector<uint8_t> codificarTexto(const std::string& valor, size_t longitud);
    static std::string decodificarTexto(const std::vector<uint8_t>& clave);
};

}  // namespace minisgbd
