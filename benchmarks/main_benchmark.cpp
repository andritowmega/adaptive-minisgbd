// Harness de evaluación: compara tiempos de ejecución entre (a) sin
// índices, (b) un solo tipo de índice fijo (Hash o B+Tree) y (c) selección
// automática de índice, bajo cargas sintéticas con distintas proporciones de
// consultas puntuales vs. de rango. Esto es el experimento central del
// artículo (sección 9).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "GeneradorCargas.h"
#include "almacenamiento/Registro.h"
#include "buffer/GestorBuffer.h"
#include "catalogo/Catalogo.h"
#include "consultas/Ejecutor.h"
#include "consultas/PlanConsulta.h"
#include "decision/GestorIndices.h"

using namespace minisgbd;
using Reloj = std::chrono::high_resolution_clock;

namespace {

const std::string NOMBRE_TABLA = "bench_productos";
const int TOTAL_FILAS = 20000;
const int TOTAL_CONSULTAS = 2000;
const size_t CAPACIDAD_BUFFER = 50;  // a proposito chica frente a ~20000 filas: fuerza E/S real

Esquema esquemaBenchmark() {
    Esquema esquema;
    esquema.nombreTabla = NOMBRE_TABLA;
    esquema.columnas = {{"id", TipoDato::ENTERO, 0}, {"valor", TipoDato::ENTERO, 0}};
    return esquema;
}

// Construye el archivo de datos UNA sola vez; todas las estrategias lo
// reabren de solo-lectura (el benchmark no hace INSERT/DELETE), así que
// puede compartirse entre corridas sin contaminar los resultados.
void crearYPoblarTabla() {
    std::string rutaDatos = "datos/" + NOMBRE_TABLA + ".dat";
    std::remove(rutaDatos.c_str());
    Esquema esquema = esquemaBenchmark();

    GestorArchivos archivo(rutaDatos);
    GestorBuffer buffer(CAPACIDAD_BUFFER);
    uint32_t paginaActual = buffer.asignarPaginaNueva(archivo);
    buffer.liberarPagina(archivo, paginaActual, true);

    for (int i = 0; i < TOTAL_FILAS; ++i) {
        std::vector<Valor> valores = {i, i * 3};
        std::vector<uint8_t> bytes = Registro::serializar(valores, esquema);

        Pagina& pagina = buffer.fijarPagina(archivo, paginaActual);
        auto slot = pagina.insertarRegistro(bytes);
        if (!slot.has_value()) {
            buffer.liberarPagina(archivo, paginaActual, false);
            paginaActual = buffer.asignarPaginaNueva(archivo);
            buffer.liberarPagina(archivo, paginaActual, false);
            Pagina& paginaNueva = buffer.fijarPagina(archivo, paginaActual);
            slot = paginaNueva.insertarRegistro(bytes);
            buffer.liberarPagina(archivo, paginaActual, true);
        } else {
            buffer.liberarPagina(archivo, paginaActual, true);
        }
    }
    buffer.vaciarTodo();  // asegura que las ~20000 filas queden realmente en disco
}

PlanConsulta construirPlanSeleccion(const ConsultaSintetica& consulta) {
    PlanConsulta plan;
    plan.operacion = TipoOperacion::SELECCIONAR;
    plan.tabla = NOMBRE_TABLA;
    plan.filtro.presente = true;
    plan.filtro.columna = "id";
    plan.filtro.tipo = consulta.tipo;
    if (consulta.tipo == TipoFiltro::IGUALDAD) {
        plan.filtro.valorIgualdad = Valor(consulta.valorIgualdad);
    } else {
        plan.filtro.valorDesde = Valor(consulta.valorDesde);
        plan.filtro.valorHasta = Valor(consulta.valorHasta);
    }
    return plan;
}

struct ResultadoCorrida {
    double proporcionPuntual;
    std::string estrategia;
    double tiempoTotalMs;
    double tiempoPromedioUs;
    double tasaAciertosBuffer;
};

ResultadoCorrida correrEstrategia(double proporcionPuntual, const std::string& nombreEstrategia,
                                   ModoGestorIndices modo, TipoIndice tipoFijo,
                                   const std::vector<ConsultaSintetica>& consultas) {
    std::string rutaCatalogo = "datos/bench_catalogo_" + nombreEstrategia + ".txt";
    std::remove(rutaCatalogo.c_str());
    std::remove(("datos/idx_" + NOMBRE_TABLA + "_id.idx").c_str());

    Catalogo catalogo(rutaCatalogo, "datos");
    catalogo.registrarTabla(esquemaBenchmark());  // reabre el archivo de datos ya poblado, sin recrearlo

    GestorBuffer buffer(CAPACIDAD_BUFFER);
    GestorIndices gestorIndices(catalogo, buffer, "datos", modo, tipoFijo);
    Ejecutor ejecutor(catalogo, buffer, gestorIndices);

    auto inicio = Reloj::now();
    for (const auto& consulta : consultas) {
        ejecutor.ejecutar(construirPlanSeleccion(consulta));
    }
    auto fin = Reloj::now();

    double tiempoTotalMs = std::chrono::duration<double, std::milli>(fin - inicio).count();

    ResultadoCorrida resultado;
    resultado.proporcionPuntual = proporcionPuntual;
    resultado.estrategia = nombreEstrategia;
    resultado.tiempoTotalMs = tiempoTotalMs;
    resultado.tiempoPromedioUs = (tiempoTotalMs * 1000.0) / static_cast<double>(consultas.size());
    resultado.tasaAciertosBuffer = buffer.tasaAciertos();
    return resultado;
}

}  // namespace

int main() {
    crearYPoblarTabla();

    // Barrido de todo el espectro point/range (0%..100% en pasos de 10%): el
    // objetivo es mostrar que la seleccion automatica nunca es la peor
    // opcion en ningun punto del espectro, aunque no sea siempre la optima.
    std::vector<ResultadoCorrida> resultados;
    std::vector<std::pair<double, bool>> veredictoPorProporcion;  // (proporcion, ¿auto fue la peor?)

    for (int paso = 0; paso <= 10; ++paso) {
        double proporcion = paso / 10.0;
        auto consultas =
            GeneradorCargas::generar(TOTAL_CONSULTAS, proporcion, 0, TOTAL_FILAS - 1, /*anchoRangoPromedio=*/20,
                                      /*semilla=*/12345);

        ResultadoCorrida sinIndices =
            correrEstrategia(proporcion, "sin_indices", ModoGestorIndices::SIN_INDICES, TipoIndice::NINGUNO, consultas);
        ResultadoCorrida fijoHash =
            correrEstrategia(proporcion, "fijo_hash", ModoGestorIndices::TIPO_FIJO, TipoIndice::HASH, consultas);
        ResultadoCorrida fijoBMas =
            correrEstrategia(proporcion, "fijo_bmas", ModoGestorIndices::TIPO_FIJO, TipoIndice::BMAS, consultas);
        ResultadoCorrida automatica = correrEstrategia(proporcion, "seleccion_automatica",
                                                        ModoGestorIndices::SELECCION_AUTOMATICA, TipoIndice::NINGUNO,
                                                        consultas);

        double peorTiempo = std::max({sinIndices.tiempoTotalMs, fijoHash.tiempoTotalMs, fijoBMas.tiempoTotalMs,
                                       automatica.tiempoTotalMs});
        bool autoFuePeor = automatica.tiempoTotalMs >= peorTiempo;
        veredictoPorProporcion.push_back({proporcion, autoFuePeor});

        resultados.push_back(sinIndices);
        resultados.push_back(fijoHash);
        resultados.push_back(fijoBMas);
        resultados.push_back(automatica);
    }

    std::cout << "proporcion_puntual,estrategia,tiempo_total_ms,tiempo_promedio_us,tasa_aciertos_buffer\n";
    for (const auto& r : resultados) {
        std::cout << r.proporcionPuntual << "," << r.estrategia << "," << r.tiempoTotalMs << ","
                   << r.tiempoPromedioUs << "," << r.tasaAciertosBuffer << "\n";
    }

    std::cout << "\n--- Veredicto: ¿la seleccion automatica fue la PEOR estrategia en esta proporcion? ---\n";
    bool autoFuePeorAlgunaVez = false;
    for (const auto& [proporcion, fuePeor] : veredictoPorProporcion) {
        std::cout << "proporcion_puntual=" << proporcion << " -> " << (fuePeor ? "SI (peor caso)" : "no") << "\n";
        autoFuePeorAlgunaVez = autoFuePeorAlgunaVez || fuePeor;
    }
    std::cout << "\nConclusion: la seleccion automatica "
              << (autoFuePeorAlgunaVez ? "SI fue la peor opcion en al menos un punto del espectro."
                                        : "NUNCA fue la peor opcion en ningun punto del espectro.")
              << "\n";

    return 0;
}
