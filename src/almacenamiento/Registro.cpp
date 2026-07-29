#include "almacenamiento/Registro.h"

#include <cstring>
#include <stdexcept>

namespace minisgbd {

std::vector<uint8_t> Registro::serializar(const std::vector<Valor>& valores, const Esquema& esquema) {
    if (valores.size() != esquema.columnas.size()) {
        throw std::invalid_argument("Registro::serializar: numero de valores no coincide con el esquema");
    }

    std::vector<uint8_t> datos(esquema.tamanoRegistro(), 0);
    size_t offset = 0;
    for (size_t i = 0; i < esquema.columnas.size(); ++i) {
        const Columna& columna = esquema.columnas[i];
        if (columna.tipo == TipoDato::ENTERO) {
            int valorEntero = std::get<int>(valores[i]);
            std::memcpy(datos.data() + offset, &valorEntero, sizeof(int));
        } else {
            const std::string& texto = std::get<std::string>(valores[i]);
            size_t longitudCopia = std::min(texto.size(), columna.longitudTexto);
            std::memcpy(datos.data() + offset, texto.data(), longitudCopia);
            // El resto del campo queda en 0 (relleno) por la inicialización del vector.
        }
        offset += columna.tamanoBytes();
    }
    return datos;
}

std::vector<Valor> Registro::deserializar(const std::vector<uint8_t>& datos, const Esquema& esquema) {
    if (datos.size() != esquema.tamanoRegistro()) {
        throw std::invalid_argument("Registro::deserializar: tamano de datos no coincide con el esquema");
    }

    std::vector<Valor> valores;
    valores.reserve(esquema.columnas.size());
    size_t offset = 0;
    for (const Columna& columna : esquema.columnas) {
        if (columna.tipo == TipoDato::ENTERO) {
            int valorEntero;
            std::memcpy(&valorEntero, datos.data() + offset, sizeof(int));
            valores.emplace_back(valorEntero);
        } else {
            const char* inicio = reinterpret_cast<const char*>(datos.data() + offset);
            std::string texto(inicio, columna.longitudTexto);
            // Recorta el relleno de ceros final para que comparaciones/impresión sean limpias.
            size_t fin = texto.find('\0');
            if (fin != std::string::npos) texto.resize(fin);
            valores.emplace_back(texto);
        }
        offset += columna.tamanoBytes();
    }
    return valores;
}

}  // namespace minisgbd
