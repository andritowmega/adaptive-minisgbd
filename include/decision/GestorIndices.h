#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "buffer/GestorBuffer.h"
#include "catalogo/Catalogo.h"
#include "indices/IIndice.h"

namespace minisgbd {

// Modo de operación del GestorIndices — pensado para el harness de
// evaluación (paso 6), que necesita comparar la selección automática contra
// las dos líneas base que motivan el aporte distintivo del proyecto.
enum class ModoGestorIndices {
    SIN_INDICES,          // nunca construye ni usa indices (linea base "sin indices")
    TIPO_FIJO,            // construye siempre tipoFijo en el primer acceso, ignora PoliticaDecision
    SELECCION_AUTOMATICA  // comportamiento normal: PoliticaDecision + ventanas de N_MIN consultas
};

// Orquestador del aporte distintivo del proyecto: recibe cada acceso por
// filtro WHERE (vía el Ejecutor), lo registra en el Catálogo, aplica la
// PoliticaDecision al completarse una ventana de consultas, y construye de
// forma perezosa (escaneo completo de la tabla) el índice elegido cuando
// cambia. Mantiene en memoria los objetos IIndice activos, uno por columna
// indexada, y los reconstruye bajo demanda si el catálogo indica un tipo
// pero aún no están cargados (por ejemplo, tras reiniciar el proceso).
class GestorIndices {
public:
    GestorIndices(Catalogo& catalogo, GestorBuffer& buffer, std::string directorioIndices,
                  ModoGestorIndices modo = ModoGestorIndices::SELECCION_AUTOMATICA,
                  TipoIndice tipoFijo = TipoIndice::NINGUNO);

    void registrarAcceso(const std::string& tabla, const std::string& columna, TipoFiltro tipoFiltro);

    // Índice vigente para (tabla,columna), o nullptr si no corresponde índice.
    IIndice* obtenerIndiceSiExiste(const std::string& tabla, const std::string& columna);

private:
    Catalogo& catalogo_;
    GestorBuffer& buffer_;
    std::string directorioIndices_;
    ModoGestorIndices modo_;
    TipoIndice tipoFijo_;
    std::map<std::pair<std::string, std::string>, std::unique_ptr<IIndice>> indicesActivos_;

    // Descarta el índice en memoria (si había uno; su archivo queda
    // abandonado en disco, misma simplificación que en el resto del
    // proyecto) y, si tipo != NINGUNO, construye uno nuevo desde cero
    // escaneando la tabla completa.
    void construirIndice(const std::string& tabla, const std::string& columna, TipoIndice tipo);
};

}  // namespace minisgbd
