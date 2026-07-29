#pragma once

#include <fstream>
#include <string>

#include "almacenamiento/Pagina.h"

namespace minisgbd {

// E/S cruda de páginas sobre un archivo de datos (un archivo por tabla). No
// sabe nada de buffering, esquemas ni índices: solo lee/escribe páginas
// completas por número de página.
class GestorArchivos {
public:
    explicit GestorArchivos(const std::string& rutaArchivo);
    ~GestorArchivos();

    GestorArchivos(const GestorArchivos&) = delete;
    GestorArchivos& operator=(const GestorArchivos&) = delete;

    // Crea una página nueva al final del archivo (inicializada vacía) y
    // devuelve su número.
    uint32_t asignarNuevaPagina();

    void leerPagina(uint32_t numeroPagina, Pagina& destino);
    void escribirPagina(uint32_t numeroPagina, const Pagina& pagina);

    uint32_t numeroPaginas() const;

private:
    std::string rutaArchivo_;
    mutable std::fstream archivo_;
    uint32_t numeroPaginas_ = 0;
};

}  // namespace minisgbd
