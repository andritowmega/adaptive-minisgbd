#pragma once

#include <vector>

#include "buffer/GestorBuffer.h"
#include "catalogo/Catalogo.h"
#include "consultas/PlanConsulta.h"
#include "decision/GestorIndices.h"

namespace minisgbd {

// Ejecuta un PlanConsulta ya parseado sobre el almacenamiento. Para
// SELECCIONAR/ELIMINAR con filtro, reporta el acceso al GestorIndices (esto
// es lo que alimenta al Motor de Decisión) y usa el índice vigente si existe;
// si no, hace un recorrido completo de la tabla aplicando el filtro en
// memoria. INSERTAR/ELIMINAR mantienen sincronizados los índices activos de
// todas las columnas de la tabla (si no se hiciera, un índice quedaría
// desactualizado apenas se insertara una fila después de construirlo).
class Ejecutor {
public:
    Ejecutor(Catalogo& catalogo, GestorBuffer& buffer, GestorIndices& gestorIndices);

    // Para SELECCIONAR devuelve las filas resultantes; para el resto, un
    // vector vacío. Lanza std::runtime_error ante tabla/columna inexistente
    // u otros errores de ejecución.
    std::vector<std::vector<Valor>> ejecutar(const PlanConsulta& plan);

private:
    Catalogo& catalogo_;
    GestorBuffer& buffer_;
    GestorIndices& gestorIndices_;

    void ejecutarCrearTabla(const PlanConsulta& plan);
    void ejecutarInsertar(const PlanConsulta& plan);
    std::vector<std::vector<Valor>> ejecutarSeleccionar(const PlanConsulta& plan);
    void ejecutarEliminar(const PlanConsulta& plan);

    // Resuelve un filtro a la lista de RID que lo cumplen, vía índice si el
    // GestorIndices ofrece uno vigente para esa columna, o con un recorrido
    // completo de la tabla en caso contrario. Siempre registra el acceso.
    std::vector<RID> resolverFiltro(Tabla& tabla, const std::string& nombreTabla, const CondicionFiltro& filtro);

    std::vector<RID> buscarConIndice(IIndice& indice, const Columna& columnaInfo, const CondicionFiltro& filtro);
    std::vector<RID> escanearConFiltro(Tabla& tabla, const Columna& columnaInfo, const CondicionFiltro& filtro);
    std::vector<RID> escanearTodo(Tabla& tabla);

    // Actualiza (inserta o elimina, según agregar) la entrada de todos los
    // índices activos de la tabla para el registro dado.
    void actualizarIndicesRegistro(const std::string& nombreTabla, const Esquema& esquema,
                                    const std::vector<Valor>& valores, RID rid, bool agregar);

    static bool cumpleFiltro(const Valor& valorColumna, const CondicionFiltro& filtro);
};

}  // namespace minisgbd
