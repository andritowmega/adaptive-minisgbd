#include "decision/PoliticaDecision.h"

namespace minisgbd {

TipoIndice PoliticaDecision::decidir(uint64_t contadorIgualdad, uint64_t contadorRango, TipoIndice tipoActual) {
    uint64_t total = contadorIgualdad + contadorRango;
    if (total < N_MIN) return tipoActual;

    double proporcionIgualdad = static_cast<double>(contadorIgualdad) / static_cast<double>(total);
    double proporcionRango = static_cast<double>(contadorRango) / static_cast<double>(total);

    if (proporcionIgualdad >= UMBRAL) return TipoIndice::HASH;
    if (proporcionRango >= UMBRAL) return TipoIndice::BMAS;

    // Patron mixto (ninguna proporcion domina claramente): Hash y B+Tree no
    // fallan de forma simetrica ante el tipo de consulta contrario. B+Tree
    // resuelve una busqueda puntual en O(log n) — no es optimo, pero es
    // razonable. Hash, en cambio, no puede resolver un rango en absoluto:
    // degrada a un recorrido completo de todos los buckets. Por eso, si
    // todavia no hay ningun indice construido, el default en la zona mixta
    // es B+Tree (el peor caso menos costoso) en vez de dejar la columna sin
    // indice. Si ya hay un indice vigente (Hash o BMAS), se mantiene sin
    // cambios: reconstruirlo en cada ventana mixta seria puro desperdicio
    // cuando cualquiera de los dos ya le gana claramente a no tener indice.
    if (tipoActual == TipoIndice::NINGUNO) return TipoIndice::BMAS;
    return tipoActual;
}

}  // namespace minisgbd
