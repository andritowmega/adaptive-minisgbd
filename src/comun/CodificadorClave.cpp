#include "comun/CodificadorClave.h"

#include <stdexcept>

namespace minisgbd {

std::vector<uint8_t> CodificadorClave::codificar(const Valor& valor, const Columna& columna) {
    if (columna.tipo == TipoDato::ENTERO) {
        return codificarEntero(std::get<int>(valor));
    }
    return codificarTexto(std::get<std::string>(valor), columna.longitudTexto);
}

Valor CodificadorClave::decodificar(const std::vector<uint8_t>& clave, const Columna& columna) {
    if (columna.tipo == TipoDato::ENTERO) {
        return decodificarEntero(clave);
    }
    return decodificarTexto(clave);
}

std::vector<uint8_t> CodificadorClave::codificarEntero(int valor) {
    // XOR con el bit de signo: convierte el rango con signo [MIN,MAX] en un
    // rango sin signo que preserva el orden, para poder usar memcmp.
    uint32_t sinSigno = static_cast<uint32_t>(valor) ^ 0x80000000u;
    std::vector<uint8_t> bytes(4);
    bytes[0] = static_cast<uint8_t>((sinSigno >> 24) & 0xFF);
    bytes[1] = static_cast<uint8_t>((sinSigno >> 16) & 0xFF);
    bytes[2] = static_cast<uint8_t>((sinSigno >> 8) & 0xFF);
    bytes[3] = static_cast<uint8_t>(sinSigno & 0xFF);
    return bytes;
}

int CodificadorClave::decodificarEntero(const std::vector<uint8_t>& clave) {
    if (clave.size() != 4) throw std::invalid_argument("CodificadorClave: clave entera de longitud invalida");
    uint32_t sinSigno = (static_cast<uint32_t>(clave[0]) << 24) | (static_cast<uint32_t>(clave[1]) << 16) |
                         (static_cast<uint32_t>(clave[2]) << 8) | static_cast<uint32_t>(clave[3]);
    return static_cast<int>(sinSigno ^ 0x80000000u);
}

std::vector<uint8_t> CodificadorClave::codificarTexto(const std::string& valor, size_t longitud) {
    std::vector<uint8_t> bytes(longitud, 0);
    size_t copiar = std::min(valor.size(), longitud);
    for (size_t i = 0; i < copiar; ++i) bytes[i] = static_cast<uint8_t>(valor[i]);
    return bytes;
}

std::string CodificadorClave::decodificarTexto(const std::vector<uint8_t>& clave) {
    std::string texto(reinterpret_cast<const char*>(clave.data()), clave.size());
    size_t fin = texto.find('\0');
    if (fin != std::string::npos) texto.resize(fin);
    return texto;
}

}  // namespace minisgbd
