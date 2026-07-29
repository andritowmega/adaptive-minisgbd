#pragma once

#include <string>

#include "consultas/PlanConsulta.h"

namespace minisgbd {

// Parser recursivo-descendente de un lenguaje de comandos propio, muy chico
// y sin ambigüedad, suficiente para distinguir el tipo de filtro de cada
// consulta (que es todo lo que el resto del sistema necesita del Parser):
//
//   CREAR TABLA nombre (col1:ENTERO, col2:TEXTO(20), ...)
//   INSERTAR EN nombre VALORES (v1, v2, ...)
//   SELECCIONAR * DE nombre [DONDE columna (= v | ENTRE v1 Y v2 | > v | >= v | < v | <= v)]
//   ELIMINAR DE nombre DONDE columna (= v | ENTRE v1 Y v2 | > v | >= v | < v | <= v)
//
// Palabras clave insensibles a mayúsculas/minúsculas; nombres de tabla/columna
// y contenido de cadenas conservan su capitalización original. No conoce el
// Catálogo ni valida tipos: eso es responsabilidad del Ejecutor.
class Parser {
public:
    static PlanConsulta parsear(const std::string& linea);
};

}  // namespace minisgbd
