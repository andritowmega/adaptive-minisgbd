#include "consultas/Ejecutor.h"

#include <algorithm>
#include <stdexcept>

#include "almacenamiento/Registro.h"
#include "comun/CodificadorClave.h"

namespace minisgbd {

Ejecutor::Ejecutor(Catalogo& catalogo, GestorBuffer& buffer, GestorIndices& gestorIndices)
    : catalogo_(catalogo), buffer_(buffer), gestorIndices_(gestorIndices) {}

std::vector<std::vector<Valor>> Ejecutor::ejecutar(const PlanConsulta& plan) {
    switch (plan.operacion) {
        case TipoOperacion::CREAR_TABLA:
            ejecutarCrearTabla(plan);
            return {};
        case TipoOperacion::INSERTAR:
            ejecutarInsertar(plan);
            return {};
        case TipoOperacion::SELECCIONAR:
            return ejecutarSeleccionar(plan);
        case TipoOperacion::ELIMINAR:
            ejecutarEliminar(plan);
            return {};
    }
    return {};
}

void Ejecutor::ejecutarCrearTabla(const PlanConsulta& plan) {
    Esquema esquema;
    esquema.nombreTabla = plan.tabla;
    esquema.columnas = plan.columnasDefinicion;
    catalogo_.registrarTabla(esquema);
}

void Ejecutor::ejecutarInsertar(const PlanConsulta& plan) {
    Tabla& tabla = catalogo_.obtenerTabla(plan.tabla);
    std::vector<uint8_t> bytes = Registro::serializar(plan.valoresInsertar, tabla.esquema);

    uint32_t totalPaginas = tabla.archivoDatos.numeroPaginas();
    uint32_t numeroPagina = totalPaginas == 0 ? buffer_.asignarPaginaNueva(tabla.archivoDatos) : totalPaginas - 1;
    if (totalPaginas == 0) buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, false);

    Pagina& pagina = buffer_.fijarPagina(tabla.archivoDatos, numeroPagina);
    auto slot = pagina.insertarRegistro(bytes);
    if (!slot.has_value()) {
        buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, false);
        numeroPagina = buffer_.asignarPaginaNueva(tabla.archivoDatos);
        buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, false);
        Pagina& paginaNueva = buffer_.fijarPagina(tabla.archivoDatos, numeroPagina);
        slot = paginaNueva.insertarRegistro(bytes);
        buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, true);
    } else {
        buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, true);
    }

    RID rid{numeroPagina, *slot};
    actualizarIndicesRegistro(plan.tabla, tabla.esquema, plan.valoresInsertar, rid, /*agregar=*/true);
}

std::vector<std::vector<Valor>> Ejecutor::ejecutarSeleccionar(const PlanConsulta& plan) {
    Tabla& tabla = catalogo_.obtenerTabla(plan.tabla);
    std::vector<RID> rids =
        plan.filtro.presente ? resolverFiltro(tabla, plan.tabla, plan.filtro) : escanearTodo(tabla);

    std::vector<std::vector<Valor>> resultado;
    resultado.reserve(rids.size());
    for (RID rid : rids) {
        Pagina& pagina = buffer_.fijarPagina(tabla.archivoDatos, rid.numeroPagina);
        std::vector<uint8_t> bytes;
        bool ok = pagina.obtenerRegistro(rid.numeroSlot, bytes);
        buffer_.liberarPagina(tabla.archivoDatos, rid.numeroPagina, false);
        if (ok) resultado.push_back(Registro::deserializar(bytes, tabla.esquema));
    }

    if (plan.tieneOrden) {
        int indiceColumna = tabla.esquema.indiceColumna(plan.columnaOrden);
        if (indiceColumna < 0) {
            throw std::runtime_error("Ejecutor: la columna '" + plan.columnaOrden +
                                      "' de ORDENAR POR no existe en la tabla '" + plan.tabla + "'");
        }
        bool descendente = plan.ordenDescendente;
        // Internal Sort: el resultado ya esta materializado por completo en
        // memoria (no hace falta un External Merge Sort para el volumen de
        // datos de este prototipo), asi que alcanza con ordenar el vector.
        std::sort(resultado.begin(), resultado.end(),
                  [indiceColumna, descendente](const std::vector<Valor>& a, const std::vector<Valor>& b) {
                      return descendente ? b[indiceColumna] < a[indiceColumna] : a[indiceColumna] < b[indiceColumna];
                  });
    }

    return resultado;
}

void Ejecutor::ejecutarEliminar(const PlanConsulta& plan) {
    Tabla& tabla = catalogo_.obtenerTabla(plan.tabla);
    std::vector<RID> rids = resolverFiltro(tabla, plan.tabla, plan.filtro);

    for (RID rid : rids) {
        Pagina& pagina = buffer_.fijarPagina(tabla.archivoDatos, rid.numeroPagina);
        std::vector<uint8_t> bytes;
        bool ok = pagina.obtenerRegistro(rid.numeroSlot, bytes);
        if (ok) pagina.eliminarRegistro(rid.numeroSlot);
        buffer_.liberarPagina(tabla.archivoDatos, rid.numeroPagina, true);

        if (ok) {
            std::vector<Valor> valores = Registro::deserializar(bytes, tabla.esquema);
            actualizarIndicesRegistro(plan.tabla, tabla.esquema, valores, rid, /*agregar=*/false);
        }
    }
}

std::vector<RID> Ejecutor::resolverFiltro(Tabla& tabla, const std::string& nombreTabla,
                                           const CondicionFiltro& filtro) {
    gestorIndices_.registrarAcceso(nombreTabla, filtro.columna, filtro.tipo);

    int indiceColumna = tabla.esquema.indiceColumna(filtro.columna);
    if (indiceColumna < 0) {
        throw std::runtime_error("Ejecutor: la columna '" + filtro.columna + "' no existe en la tabla '" +
                                  nombreTabla + "'");
    }
    const Columna& columnaInfo = tabla.esquema.columnas[indiceColumna];

    IIndice* indice = gestorIndices_.obtenerIndiceSiExiste(nombreTabla, filtro.columna);
    if (indice != nullptr) return buscarConIndice(*indice, columnaInfo, filtro);
    return escanearConFiltro(tabla, columnaInfo, filtro);
}

std::vector<RID> Ejecutor::buscarConIndice(IIndice& indice, const Columna& columnaInfo,
                                            const CondicionFiltro& filtro) {
    if (filtro.tipo == TipoFiltro::IGUALDAD) {
        std::vector<uint8_t> clave = CodificadorClave::codificar(filtro.valorIgualdad, columnaInfo);
        auto resultado = indice.buscarPuntual(clave);
        // Nota: buscarPuntual devuelve solo la primera coincidencia — el
        // índice asume, para búsquedas de igualdad, que la columna no tiene
        // valores duplicados (ver docs/diseno_sistema.md, limitacion
        // aceptada para no sacrificar el O(1) del Hash en el benchmark).
        return resultado.has_value() ? std::vector<RID>{*resultado} : std::vector<RID>{};
    }

    std::vector<uint8_t> desde =
        filtro.tieneCotaInferior ? CodificadorClave::codificar(filtro.valorDesde, columnaInfo)
                                  : CodificadorClave::minimo(columnaInfo);
    std::vector<uint8_t> hasta =
        filtro.tieneCotaSuperior ? CodificadorClave::codificar(filtro.valorHasta, columnaInfo)
                                  : CodificadorClave::maximo(columnaInfo);
    return indice.buscarRango(desde, hasta);
}

std::vector<RID> Ejecutor::escanearConFiltro(Tabla& tabla, const Columna& columnaInfo,
                                              const CondicionFiltro& filtro) {
    std::vector<RID> resultado;
    int indiceColumna = tabla.esquema.indiceColumna(columnaInfo.nombre);
    uint32_t totalPaginas = tabla.archivoDatos.numeroPaginas();

    for (uint32_t numeroPagina = 0; numeroPagina < totalPaginas; ++numeroPagina) {
        Pagina& pagina = buffer_.fijarPagina(tabla.archivoDatos, numeroPagina);
        uint16_t numeroSlots = pagina.numeroSlots();
        for (uint16_t slot = 0; slot < numeroSlots; ++slot) {
            if (!pagina.estaOcupado(slot)) continue;
            std::vector<uint8_t> bytes;
            pagina.obtenerRegistro(slot, bytes);
            std::vector<Valor> valores = Registro::deserializar(bytes, tabla.esquema);
            if (cumpleFiltro(valores[indiceColumna], filtro)) {
                resultado.push_back(RID{numeroPagina, slot});
            }
        }
        buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, false);
    }
    return resultado;
}

std::vector<RID> Ejecutor::escanearTodo(Tabla& tabla) {
    std::vector<RID> resultado;
    uint32_t totalPaginas = tabla.archivoDatos.numeroPaginas();
    for (uint32_t numeroPagina = 0; numeroPagina < totalPaginas; ++numeroPagina) {
        Pagina& pagina = buffer_.fijarPagina(tabla.archivoDatos, numeroPagina);
        uint16_t numeroSlots = pagina.numeroSlots();
        for (uint16_t slot = 0; slot < numeroSlots; ++slot) {
            if (pagina.estaOcupado(slot)) resultado.push_back(RID{numeroPagina, slot});
        }
        buffer_.liberarPagina(tabla.archivoDatos, numeroPagina, false);
    }
    return resultado;
}

void Ejecutor::actualizarIndicesRegistro(const std::string& nombreTabla, const Esquema& esquema,
                                          const std::vector<Valor>& valores, RID rid, bool agregar) {
    for (size_t i = 0; i < esquema.columnas.size(); ++i) {
        const Columna& columnaInfo = esquema.columnas[i];
        IIndice* indice = gestorIndices_.obtenerIndiceSiExiste(nombreTabla, columnaInfo.nombre);
        if (indice == nullptr) continue;
        std::vector<uint8_t> clave = CodificadorClave::codificar(valores[i], columnaInfo);
        if (agregar) {
            indice->insertar(clave, rid);
        } else {
            indice->eliminar(clave, rid);
        }
    }
}

bool Ejecutor::cumpleFiltro(const Valor& valorColumna, const CondicionFiltro& filtro) {
    if (filtro.tipo == TipoFiltro::IGUALDAD) return valorColumna == filtro.valorIgualdad;
    if (filtro.tieneCotaInferior && valorColumna < filtro.valorDesde) return false;
    if (filtro.tieneCotaSuperior && filtro.valorHasta < valorColumna) return false;
    return true;
}

}  // namespace minisgbd
