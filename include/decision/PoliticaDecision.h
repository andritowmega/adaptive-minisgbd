#pragma once

#include <cstdint>

#include "comun/Tipos.h"

namespace minisgbd {

// Política de decisión de índice: pura función de los contadores acumulados
// desde la última ventana de evaluación, sin machine learning. Umbral y
// tamaño de ventana fijados con el usuario para el artículo (sección 7.4 de
// docs/diseno_sistema.md).
//
// Histéresis: como GestorIndices resetea los contadores cada vez que se
// completa una ventana de N_MIN consultas (haya cambiado el tipo o no), un
// índice ya vigente solo se reemplaza si el patrón contrario domina una
// ventana COMPLETA nueva — no hace falta estado adicional para lograr esto,
// es una consecuencia directa de reiniciar la ventana en cada evaluación.
class PoliticaDecision {
public:
    static constexpr uint64_t N_MIN = 20;
    static constexpr double UMBRAL = 0.7;

    // Dado el estado acumulado de una columna, decide el tipo de índice que
    // debería estar vigente. Si aún no hay suficientes muestras (total <
    // N_MIN), se mantiene tipoActual. Si el patrón es mixto (ninguna
    // proporción alcanza el umbral): se mantiene tipoActual si ya había un
    // índice construido, o se elige B+Tree por defecto si no había ninguno
    // — Hash no resuelve rangos en absoluto (recorrido completo de buckets),
    // mientras que B+Tree resuelve puntuales en O(log n); ante la duda,
    // conviene el que falla menos mal.
    static TipoIndice decidir(uint64_t contadorIgualdad, uint64_t contadorRango, TipoIndice tipoActual);
};

}  // namespace minisgbd
