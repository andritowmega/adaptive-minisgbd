#pragma once

#include <cstdint>

#include "almacenamiento/Pagina.h"

namespace minisgbd {

class GestorArchivos;

// Un marco (frame) del buffer pool: una página en memoria más los metadatos
// que necesita el GestorBuffer para decidir reemplazo y persistencia.
struct Marco {
    Pagina pagina;
    GestorArchivos* archivo = nullptr;
    uint32_t idPagina = 0;
    bool ocupado = false;
    bool sucio = false;
    int contadorPines = 0;
};

}  // namespace minisgbd
