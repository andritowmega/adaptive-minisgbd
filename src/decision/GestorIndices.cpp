#include "decision/GestorIndices.h"

#include <cstdio>
#include <stdexcept>

#include "almacenamiento/Registro.h"
#include "comun/CodificadorClave.h"
#include "decision/PoliticaDecision.h"
#include "indices/ArbolBMas.h"
#include "indices/HashExtensible.h"

namespace minisgbd {

GestorIndices::GestorIndices(Catalogo& catalogo, GestorBuffer& buffer, std::string directorioIndices,
                              ModoGestorIndices modo, TipoIndice tipoFijo)
    : catalogo_(catalogo),
      buffer_(buffer),
      directorioIndices_(std::move(directorioIndices)),
      modo_(modo),
      tipoFijo_(tipoFijo) {}

void GestorIndices::registrarAcceso(const std::string& tabla, const std::string& columna, TipoFiltro tipoFiltro) {
    if (modo_ == ModoGestorIndices::SIN_INDICES) return;  // linea base: nunca construir nada

    if (modo_ == ModoGestorIndices::TIPO_FIJO) {
        // Ignora la ventana de N_MIN y la proporcion observada: construye
        // tipoFijo_ apenas se accede por primera vez a esta columna.
        if (catalogo_.tipoIndiceActual(tabla, columna) == TipoIndice::NINGUNO) {
            construirIndice(tabla, columna, tipoFijo_);
            catalogo_.actualizarTipoIndiceYResetear(tabla, columna, tipoFijo_);
        }
        return;
    }

    // ModoGestorIndices::SELECCION_AUTOMATICA: comportamiento normal.
    catalogo_.registrarAcceso(tabla, columna, tipoFiltro);

    auto [contadorIgualdad, contadorRango] = catalogo_.contadoresActuales(tabla, columna);
    uint64_t total = contadorIgualdad + contadorRango;
    if (total < PoliticaDecision::N_MIN) return;  // ventana aun no completa

    TipoIndice actual = catalogo_.tipoIndiceActual(tabla, columna);
    TipoIndice decidido = PoliticaDecision::decidir(contadorIgualdad, contadorRango, actual);
    if (decidido != actual) {
        construirIndice(tabla, columna, decidido);
    }
    catalogo_.actualizarTipoIndiceYResetear(tabla, columna, decidido);
}

IIndice* GestorIndices::obtenerIndiceSiExiste(const std::string& tabla, const std::string& columna) {
    if (modo_ == ModoGestorIndices::SIN_INDICES) return nullptr;

    auto clave = std::make_pair(tabla, columna);
    auto it = indicesActivos_.find(clave);
    if (it != indicesActivos_.end()) return it->second.get();

    TipoIndice tipo = catalogo_.tipoIndiceActual(tabla, columna);
    if (tipo == TipoIndice::NINGUNO) return nullptr;

    construirIndice(tabla, columna, tipo);  // reconstruccion perezosa (p.ej. tras reiniciar el proceso)
    return indicesActivos_[clave].get();
}

void GestorIndices::construirIndice(const std::string& tabla, const std::string& columna, TipoIndice tipo) {
    auto clave = std::make_pair(tabla, columna);
    indicesActivos_.erase(clave);
    if (tipo == TipoIndice::NINGUNO) return;

    Tabla& t = catalogo_.obtenerTabla(tabla);
    int indiceColumna = t.esquema.indiceColumna(columna);
    if (indiceColumna < 0) {
        throw std::runtime_error("GestorIndices: la columna '" + columna + "' no existe en la tabla '" + tabla + "'");
    }
    const Columna& columnaInfo = t.esquema.columnas[indiceColumna];
    size_t longitudClave = CodificadorClave::longitudClave(columnaInfo);

    std::string ruta = directorioIndices_ + "/idx_" + tabla + "_" + columna + ".idx";
    std::remove(ruta.c_str());  // asegura reconstruccion desde cero (evita reusar datos de un tipo anterior)

    std::unique_ptr<IIndice> nuevoIndice;
    if (tipo == TipoIndice::HASH) {
        nuevoIndice = std::make_unique<HashExtensible>(ruta, buffer_, longitudClave);
    } else {
        nuevoIndice = std::make_unique<ArbolBMas>(ruta, buffer_, longitudClave);
    }

    uint32_t totalPaginas = t.archivoDatos.numeroPaginas();
    for (uint32_t numeroPagina = 0; numeroPagina < totalPaginas; ++numeroPagina) {
        Pagina& pagina = buffer_.fijarPagina(t.archivoDatos, numeroPagina);
        uint16_t numeroSlots = pagina.numeroSlots();
        for (uint16_t slot = 0; slot < numeroSlots; ++slot) {
            if (!pagina.estaOcupado(slot)) continue;
            std::vector<uint8_t> bytesRegistro;
            pagina.obtenerRegistro(slot, bytesRegistro);
            std::vector<Valor> valores = Registro::deserializar(bytesRegistro, t.esquema);
            std::vector<uint8_t> claveCodificada = CodificadorClave::codificar(valores[indiceColumna], columnaInfo);
            nuevoIndice->insertar(claveCodificada, RID{numeroPagina, slot});
        }
        buffer_.liberarPagina(t.archivoDatos, numeroPagina, false);
    }

    indicesActivos_[clave] = std::move(nuevoIndice);
}

}  // namespace minisgbd
