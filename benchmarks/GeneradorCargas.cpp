#include "GeneradorCargas.h"

#include <algorithm>
#include <random>

namespace minisgbd {

std::vector<ConsultaSintetica> GeneradorCargas::generar(int totalConsultas, double proporcionPuntual,
                                                          int dominioMinimo, int dominioMaximo,
                                                          int anchoRangoPromedio, unsigned semilla) {
    std::vector<ConsultaSintetica> consultas;
    consultas.reserve(totalConsultas);

    std::mt19937 rng(semilla);
    std::bernoulli_distribution esPuntual(proporcionPuntual);
    std::uniform_int_distribution<int> distribucionValor(dominioMinimo, dominioMaximo);
    std::uniform_int_distribution<int> distribucionAncho(1, std::max(1, anchoRangoPromedio * 2));

    for (int i = 0; i < totalConsultas; ++i) {
        ConsultaSintetica consulta;
        if (esPuntual(rng)) {
            consulta.tipo = TipoFiltro::IGUALDAD;
            consulta.valorIgualdad = distribucionValor(rng);
        } else {
            consulta.tipo = TipoFiltro::RANGO;
            int inicio = distribucionValor(rng);
            int ancho = distribucionAncho(rng);
            consulta.valorDesde = inicio;
            consulta.valorHasta = std::min(dominioMaximo, inicio + ancho);
        }
        consultas.push_back(consulta);
    }
    return consultas;
}

}  // namespace minisgbd
