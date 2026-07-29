#pragma once

#include <vector>

#include "comun/Tipos.h"

namespace minisgbd {

struct ConsultaSintetica {
    TipoFiltro tipo = TipoFiltro::IGUALDAD;
    int valorIgualdad = 0;  // válido si tipo == IGUALDAD
    int valorDesde = 0;     // válido si tipo == RANGO
    int valorHasta = 0;     // válido si tipo == RANGO
};

// Genera una carga sintética de consultas SELECT sobre una columna entera,
// con una proporción configurable de consultas puntuales vs. de rango — es
// lo que el harness usa para comparar estrategias de indexación bajo
// distintos patrones de acceso (90/10, 50/50, 10/90, etc).
class GeneradorCargas {
public:
    static std::vector<ConsultaSintetica> generar(int totalConsultas, double proporcionPuntual, int dominioMinimo,
                                                   int dominioMaximo, int anchoRangoPromedio, unsigned semilla);
};

}  // namespace minisgbd
