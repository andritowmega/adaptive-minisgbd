#pragma once

#include <string>
#include <vector>

#include "comun/Tipos.h"

namespace minisgbd {

enum class TipoOperacion {
    CREAR_TABLA,
    INSERTAR,
    SELECCIONAR,
    ELIMINAR
};

// Condición de un DONDE. tipo distingue punto vs. rango — es exactamente lo
// que el Motor de Decisión necesita observar de cada consulta. Los rangos
// abiertos (WHERE col > v, WHERE col < v) se representan con
// tieneCotaInferior/tieneCotaSuperior en false; el Ejecutor resuelve la cota
// faltante con CodificadorClave::minimo/maximo según el tipo de la columna
// (que el Parser no conoce — no depende del Catálogo).
struct CondicionFiltro {
    bool presente = false;
    std::string columna;
    TipoFiltro tipo = TipoFiltro::IGUALDAD;

    Valor valorIgualdad;  // válido si tipo == IGUALDAD

    bool tieneCotaInferior = true;  // false para "< v" / "<= v" (sin piso)
    bool tieneCotaSuperior = true;  // false para "> v" / ">= v" (sin techo)
    Valor valorDesde;                // válido si tipo == RANGO && tieneCotaInferior
    Valor valorHasta;                // válido si tipo == RANGO && tieneCotaSuperior
};

// AST de una sentencia ya parseada. Un único struct cubre las cuatro
// operaciones soportadas; los campos irrelevantes para cada una quedan
// vacíos (más simple que una jerarquía de clases para un lenguaje tan chico).
struct PlanConsulta {
    TipoOperacion operacion;
    std::string tabla;

    std::vector<Columna> columnasDefinicion;  // solo CREAR_TABLA
    std::vector<Valor> valoresInsertar;       // solo INSERTAR
    CondicionFiltro filtro;                   // SELECCIONAR (opcional) / ELIMINAR (obligatorio)
};

}  // namespace minisgbd
