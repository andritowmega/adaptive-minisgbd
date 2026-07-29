#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "comun/Tipos.h"

namespace minisgbd {

// Interfaz común a toda estructura de índice, ya sea Hash Extensible o
// B+ Tree. Trabaja sobre bytes ya codificados (ver CodificadorClave), no
// sobre Valor directamente, porque así es como se almacenan en las páginas
// de disco: sin conocer en tiempo de ejecución si la columna es entera o de
// texto.
class IIndice {
public:
    virtual ~IIndice() = default;

    virtual void insertar(const std::vector<uint8_t>& clave, RID rid) = 0;

    // Búsqueda por igualdad exacta. Devuelve el primer RID encontrado (no se
    // asume clave única a nivel de índice: para claves duplicadas, usar
    // buscarRango(clave, clave) si se necesitan todos los RID).
    virtual std::optional<RID> buscarPuntual(const std::vector<uint8_t>& clave) = 0;

    // Búsqueda por rango cerrado [desde, hasta]. HashExtensible la soporta
    // pero con un costo de recorrido completo (a propósito: el experimento
    // del artículo demuestra que para este patrón conviene un B+ Tree).
    virtual std::vector<RID> buscarRango(const std::vector<uint8_t>& desde, const std::vector<uint8_t>& hasta) = 0;

    virtual bool eliminar(const std::vector<uint8_t>& clave, RID rid) = 0;
};

}  // namespace minisgbd
