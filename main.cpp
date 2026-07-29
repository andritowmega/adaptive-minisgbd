// REPL interactivo del mini-SGBD: lee comandos de la consola, linea por
// linea, y los ejecuta contra el almacenamiento persistente en datos/.
// Pensado para probar el sistema a mano; los tests automatizados
// (tests/*.cpp) y el benchmark (benchmarks/main_benchmark.cpp) son la forma
// "seria" de verificacion del proyecto.
#include <cctype>
#include <iostream>
#include <string>

#include "buffer/GestorBuffer.h"
#include "catalogo/Catalogo.h"
#include "consultas/Ejecutor.h"
#include "consultas/Parser.h"
#include "decision/GestorIndices.h"

using namespace minisgbd;

namespace {

void imprimirFila(const std::vector<Valor>& fila) {
    std::cout << "(";
    for (size_t i = 0; i < fila.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::visit([](const auto& v) { std::cout << v; }, fila[i]);
    }
    std::cout << ")\n";
}

void imprimirAyuda() {
    std::cout << "Comandos disponibles:\n"
                 "  CREAR TABLA nombre (col1:ENTERO, col2:TEXTO(20), ...)\n"
                 "  INSERTAR EN nombre VALORES (v1, v2, ...)\n"
                 "  SELECCIONAR * DE nombre [DONDE columna (= v | ENTRE v1 Y v2 | > v | >= v | < v | <= v)]\n"
                 "  ELIMINAR DE nombre DONDE columna (= v | ENTRE v1 Y v2 | > v | >= v | < v | <= v)\n"
                 "  TABLAS            (lista las tablas creadas en esta sesion)\n"
                 "  DESCRIBIR nombre  (muestra las columnas de una tabla)\n"
                 "  AYUDA             (muestra este mensaje)\n"
                 "  SALIR             (termina el programa)\n";
}

std::string aMayusculas(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) r += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

std::string recortar(const std::string& s) {
    size_t inicio = s.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) return "";
    size_t fin = s.find_last_not_of(" \t\r\n");
    return s.substr(inicio, fin - inicio + 1);
}

std::string nombreColumnaTexto(const Columna& columna) {
    if (columna.tipo == TipoDato::ENTERO) return "ENTERO";
    return "TEXTO(" + std::to_string(columna.longitudTexto) + ")";
}

std::string nombreTipoIndice(TipoIndice tipo) {
    switch (tipo) {
        case TipoIndice::HASH:
            return "indice: Hash Extensible";
        case TipoIndice::BMAS:
            return "indice: B+ Tree";
        default:
            return "sin indice";
    }
}

void listarTablas(const Catalogo& catalogo) {
    std::vector<std::string> nombres = catalogo.nombresTablas();
    if (nombres.empty()) {
        std::cout << "(no hay tablas creadas en esta sesion)\n";
        return;
    }
    for (const auto& nombre : nombres) std::cout << "  " << nombre << "\n";
}

void describirTabla(Catalogo& catalogo, const std::string& nombreTabla) {
    Tabla& tabla = catalogo.obtenerTabla(nombreTabla);  // lanza si no existe
    for (const Columna& columna : tabla.esquema.columnas) {
        TipoIndice tipo = catalogo.tipoIndiceActual(nombreTabla, columna.nombre);
        std::cout << "  " << columna.nombre << ": " << nombreColumnaTexto(columna) << " ["
                   << nombreTipoIndice(tipo) << "]\n";
    }
}

}  // namespace

int main() {
    Catalogo catalogo("datos/catalogo.txt", "datos");
    GestorBuffer buffer(100);
    GestorIndices gestorIndices(catalogo, buffer, "datos");
    Ejecutor ejecutor(catalogo, buffer, gestorIndices);

    std::cout << "mini-SGBD -- escribe AYUDA para ver los comandos, SALIR para terminar.\n";

    std::string linea;
    while (true) {
        std::cout << "sgbd> ";
        if (!std::getline(std::cin, linea)) break;  // EOF (Ctrl+D / Ctrl+Z)

        size_t inicio = linea.find_first_not_of(" \t\r\n");
        if (inicio == std::string::npos) continue;  // linea vacia
        size_t fin = linea.find_last_not_of(" \t\r\n");
        std::string comando = linea.substr(inicio, fin - inicio + 1);
        std::string comandoMayus = aMayusculas(comando);

        if (comandoMayus == "SALIR" || comandoMayus == "EXIT" || comandoMayus == "QUIT") break;
        if (comandoMayus == "AYUDA" || comandoMayus == "HELP") {
            imprimirAyuda();
            continue;
        }
        if (comandoMayus == "TABLAS") {
            listarTablas(catalogo);
            continue;
        }
        if (comandoMayus.rfind("DESCRIBIR ", 0) == 0) {
            std::string nombreTabla = recortar(comando.substr(std::string("DESCRIBIR ").size()));
            try {
                describirTabla(catalogo, nombreTabla);
            } catch (const std::exception& error) {
                std::cout << "Error: " << error.what() << "\n";
            }
            continue;
        }

        try {
            PlanConsulta plan = Parser::parsear(comando);
            auto resultado = ejecutor.ejecutar(plan);
            if (plan.operacion == TipoOperacion::SELECCIONAR) {
                for (const auto& fila : resultado) imprimirFila(fila);
                std::cout << resultado.size() << " fila(s).\n";
            } else {
                std::cout << "OK.\n";
            }
        } catch (const std::exception& error) {
            std::cout << "Error: " << error.what() << "\n";
        }
    }

    buffer.vaciarTodo();  // asegura que quede todo persistido en disco al salir
    std::cout << "Hasta luego.\n";
    return 0;
}
