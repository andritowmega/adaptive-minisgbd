// Prueba manual (no framework) del Parser + Ejecutor: primero unidades del
// parser (AST correcto, errores de sintaxis), luego un flujo completo de
// extremo a extremo vía comandos de texto reales, verificando que el motor
// de decision se dispare correctamente desde consultas parseadas y que los
// indices se mantengan sincronizados tras INSERTAR/ELIMINAR.
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "catalogo/Catalogo.h"
#include "buffer/GestorBuffer.h"
#include "comun/Tipos.h"
#include "consultas/Ejecutor.h"
#include "consultas/Parser.h"
#include "decision/GestorIndices.h"

using namespace minisgbd;

static int fallas = 0;

#define VERIFICAR(condicion, mensaje)                                       \
    do {                                                                    \
        if (!(condicion)) {                                                 \
            std::cerr << "[FALLO] " << mensaje << " (" << __LINE__ << ")\n"; \
            fallas++;                                                       \
        } else {                                                            \
            std::cout << "[ok]    " << mensaje << "\n";                     \
        }                                                                   \
    } while (0)

#define VERIFICAR_LANZA(expresion, mensaje)                                          \
    do {                                                                             \
        bool lanzo = false;                                                          \
        try {                                                                        \
            expresion;                                                              \
        } catch (const std::exception&) {                                           \
            lanzo = true;                                                            \
        }                                                                            \
        VERIFICAR(lanzo, mensaje);                                                   \
    } while (0)

static void probarParser() {
    PlanConsulta plan = Parser::parsear("CREAR TABLA productos (id:ENTERO, nombre:TEXTO(15), precio:ENTERO)");
    VERIFICAR(plan.operacion == TipoOperacion::CREAR_TABLA, "parsea CREAR TABLA");
    VERIFICAR(plan.tabla == "productos", "CREAR TABLA: nombre de tabla correcto");
    VERIFICAR(plan.columnasDefinicion.size() == 3, "CREAR TABLA: numero de columnas correcto");
    VERIFICAR(plan.columnasDefinicion[1].tipo == TipoDato::TEXTO && plan.columnasDefinicion[1].longitudTexto == 15,
              "CREAR TABLA: columna TEXTO(15) parseada correctamente");

    PlanConsulta planInsertar = Parser::parsear("INSERTAR EN productos VALORES (7, 'martillo', 1500)");
    VERIFICAR(planInsertar.operacion == TipoOperacion::INSERTAR, "parsea INSERTAR");
    VERIFICAR(planInsertar.valoresInsertar.size() == 3, "INSERTAR: numero de valores correcto");
    VERIFICAR(std::get<std::string>(planInsertar.valoresInsertar[1]) == "martillo",
              "INSERTAR: valor de cadena parseado sin comillas");

    PlanConsulta planIgualdad = Parser::parsear("SELECCIONAR * DE productos DONDE id = 7");
    VERIFICAR(planIgualdad.filtro.presente && planIgualdad.filtro.tipo == TipoFiltro::IGUALDAD,
              "SELECCIONAR DONDE = parsea como filtro de igualdad");

    PlanConsulta planRango = Parser::parsear("SELECCIONAR * DE productos DONDE precio ENTRE 100 Y 2000");
    VERIFICAR(planRango.filtro.tipo == TipoFiltro::RANGO && planRango.filtro.tieneCotaInferior &&
                  planRango.filtro.tieneCotaSuperior,
              "SELECCIONAR DONDE ENTRE parsea como rango con ambas cotas");

    PlanConsulta planMayor = Parser::parsear("SELECCIONAR * DE productos DONDE precio > 500");
    VERIFICAR(planMayor.filtro.tipo == TipoFiltro::RANGO && !planMayor.filtro.tieneCotaSuperior,
              "SELECCIONAR DONDE > parsea como rango abierto sin cota superior");

    PlanConsulta planSinFiltro = Parser::parsear("SELECCIONAR * DE productos");
    VERIFICAR(!planSinFiltro.filtro.presente, "SELECCIONAR sin DONDE no tiene filtro");

    PlanConsulta planEliminar = Parser::parsear("ELIMINAR DE productos DONDE id = 7");
    VERIFICAR(planEliminar.operacion == TipoOperacion::ELIMINAR, "parsea ELIMINAR");

    VERIFICAR_LANZA(Parser::parsear("SELECCIONAR * productos"), "falta 'DE' lanza error de sintaxis");
    VERIFICAR_LANZA(Parser::parsear("ELIMINAR DE productos"), "ELIMINAR sin DONDE lanza error de sintaxis");
    VERIFICAR_LANZA(Parser::parsear("CREAR TABLA productos (id:ENTERO"), "parentesis sin cerrar lanza error");
    VERIFICAR_LANZA(Parser::parsear("ALGOCOMANDODESCONOCIDO productos"), "comando desconocido lanza error");
}

static std::vector<std::vector<Valor>> correr(Ejecutor& ejecutor, const std::string& comando) {
    return ejecutor.ejecutar(Parser::parsear(comando));
}

static void probarEjecutorEndToEnd() {
    std::remove("datos/test_catalogo_consultas.txt");
    std::remove("datos/test_consultas_productos.dat");
    std::remove("datos/idx_test_consultas_productos_id.idx");

    Catalogo catalogo("datos/test_catalogo_consultas.txt", "datos");
    GestorBuffer buffer(15);
    GestorIndices gestorIndices(catalogo, buffer, "datos");
    Ejecutor ejecutor(catalogo, buffer, gestorIndices);

    correr(ejecutor, "CREAR TABLA test_consultas_productos (id:ENTERO, nombre:TEXTO(15), precio:ENTERO)");

    const int totalFilas = 500;
    for (int i = 0; i < totalFilas; ++i) {
        std::string comando = "INSERTAR EN test_consultas_productos VALORES (" + std::to_string(i) + ", 'prod_" +
                               std::to_string(i) + "', " + std::to_string(i * 10) + ")";
        correr(ejecutor, comando);
    }

    // --- Antes de que exista indice: filtro por igualdad via recorrido completo ---
    auto resultado = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id = 250");
    VERIFICAR(resultado.size() == 1 && std::get<int>(resultado[0][0]) == 250,
              "SELECCIONAR DONDE id = 250 encuentra la fila correcta (recorrido completo, sin indice aun)");

    auto resultadoTexto = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE nombre = 'prod_250'");
    VERIFICAR(resultadoTexto.size() == 1 && std::get<int>(resultadoTexto[0][0]) == 250,
              "SELECCIONAR sobre columna TEXTO tambien funciona por recorrido completo");

    // --- Disparar 20 consultas puntuales sobre 'id' -> deberia construirse un Hash ---
    for (int i = 0; i < 19; ++i) {
        correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id = " + std::to_string(i));
    }
    auto resultadoConIndice = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id = 250");
    VERIFICAR(resultadoConIndice.size() == 1 && std::get<int>(resultadoConIndice[0][0]) == 250,
              "tras 20 consultas puntuales, SELECCIONAR DONDE id = 250 sigue devolviendo el resultado correcto");

    // --- 20 consultas de rango sobre 'id' -> deberia cambiar a B+Tree ---
    for (int i = 0; i < 20; ++i) {
        correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id ENTRE 0 Y 5");
    }
    auto resultadoRango = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id ENTRE 100 Y 110");
    VERIFICAR(resultadoRango.size() == 11, "tras el cambio a B+Tree, DONDE id ENTRE 100 Y 110 devuelve 11 filas");

    // Nota: '>' y '<' se tratan igual que '>=' y '<=' (simplificacion documentada
    // en docs/diseno_sistema.md), por eso el limite queda incluido.
    auto resultadoMayor = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id > 495");
    VERIFICAR(resultadoMayor.size() == 5, "DONDE id > 495 (rango abierto, limite incluido) devuelve 495..499");

    auto resultadoMenor = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id < 3");
    VERIFICAR(resultadoMenor.size() == 4, "DONDE id < 3 (rango abierto, limite incluido) devuelve 0..3");

    // --- Insertar una fila nueva DESPUES de que el indice existe: debe quedar sincronizado ---
    correr(ejecutor, "INSERTAR EN test_consultas_productos VALORES (999, 'prod_999', 9990)");
    auto resultadoNuevo = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id = 999");
    VERIFICAR(resultadoNuevo.size() == 1, "una fila insertada despues de construido el indice se encuentra vía el indice");

    // --- Eliminar esa fila: el indice debe dejar de encontrarla ---
    correr(ejecutor, "ELIMINAR DE test_consultas_productos DONDE id = 999");
    auto resultadoEliminado = correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id = 999");
    VERIFICAR(resultadoEliminado.empty(), "tras ELIMINAR, la fila ya no aparece en el indice");

    auto resultadoRangoTrasEliminar =
        correr(ejecutor, "SELECCIONAR * DE test_consultas_productos DONDE id ENTRE 990 Y 1000");
    VERIFICAR(resultadoRangoTrasEliminar.empty(),
              "tras ELIMINAR, un rango que la incluiria tampoco la devuelve");
}

int main() {
    probarParser();
    probarEjecutorEndToEnd();

    if (fallas > 0) {
        std::cerr << "\n" << fallas << " verificacion(es) fallaron.\n";
        return 1;
    }
    std::cout << "\nTodas las verificaciones pasaron.\n";
    return 0;
}
