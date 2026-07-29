// Prueba manual (no framework) del Gestor de Almacenamiento + Buffer Manager.
// Compilar y correr con el Makefile: `make test` (o ver README de build).
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "almacenamiento/GestorArchivos.h"
#include "almacenamiento/Registro.h"
#include "buffer/GestorBuffer.h"
#include "comun/Tipos.h"

using namespace minisgbd;

static int fallas = 0;

#define VERIFICAR(condicion, mensaje)                                            \
    do {                                                                         \
        if (!(condicion)) {                                                      \
            std::cerr << "[FALLO] " << mensaje << " (" << __LINE__ << ")\n";      \
            fallas++;                                                            \
        } else {                                                                 \
            std::cout << "[ok]    " << mensaje << "\n";                          \
        }                                                                        \
    } while (0)

int main() {
    const std::string rutaArchivo = "datos/test_personas.dat";
    std::remove(rutaArchivo.c_str());

    Esquema esquema;
    esquema.nombreTabla = "personas";
    esquema.columnas = {
        {"id", TipoDato::ENTERO, 0},
        {"nombre", TipoDato::TEXTO, 20},
    };

    GestorArchivos gestorArchivos(rutaArchivo);
    // Buffer chico a propósito (3 marcos) para forzar reemplazo LRU con pocas páginas.
    GestorBuffer gestorBuffer(3);

    // --- Insertar registros suficientes para varias páginas ---
    std::vector<RID> ridsInsertados;
    uint32_t paginaActual = gestorBuffer.asignarPaginaNueva(gestorArchivos);
    gestorBuffer.liberarPagina(gestorArchivos, paginaActual, true);

    // Con ~146 registros por página de 4KB, 1000 registros ocupan ~7 páginas,
    // muchas más que los 3 marcos del buffer: esto fuerza evicciones LRU reales.
    const int totalRegistros = 1000;
    for (int i = 0; i < totalRegistros; ++i) {
        std::vector<Valor> valores = {i, std::string("persona_") + std::to_string(i)};
        std::vector<uint8_t> bytes = Registro::serializar(valores, esquema);

        Pagina& pagina = gestorBuffer.fijarPagina(gestorArchivos, paginaActual);
        auto slot = pagina.insertarRegistro(bytes);
        if (!slot.has_value()) {
            gestorBuffer.liberarPagina(gestorArchivos, paginaActual, false);
            paginaActual = gestorBuffer.asignarPaginaNueva(gestorArchivos);
            gestorBuffer.liberarPagina(gestorArchivos, paginaActual, true);
            Pagina& paginaNueva = gestorBuffer.fijarPagina(gestorArchivos, paginaActual);
            slot = paginaNueva.insertarRegistro(bytes);
            gestorBuffer.liberarPagina(gestorArchivos, paginaActual, true);
        } else {
            gestorBuffer.liberarPagina(gestorArchivos, paginaActual, true);
        }
        ridsInsertados.push_back(RID{paginaActual, *slot});
    }
    VERIFICAR(static_cast<int>(ridsInsertados.size()) == totalRegistros, "se insertaron todos los registros esperados");
    VERIFICAR(gestorArchivos.numeroPaginas() > 1, "los registros ocuparon mas de una pagina de 4KB");

    // --- Leer de vuelta y verificar contenido, forzando evicciones LRU ---
    bool todosCorrectos = true;
    for (int i = 0; i < totalRegistros; ++i) {
        RID rid = ridsInsertados[i];
        Pagina& pagina = gestorBuffer.fijarPagina(gestorArchivos, rid.numeroPagina);
        std::vector<uint8_t> bytes;
        bool ok = pagina.obtenerRegistro(rid.numeroSlot, bytes);
        gestorBuffer.liberarPagina(gestorArchivos, rid.numeroPagina, false);
        if (!ok) { todosCorrectos = false; break; }
        std::vector<Valor> valores = Registro::deserializar(bytes, esquema);
        if (std::get<int>(valores[0]) != i || std::get<std::string>(valores[1]) != ("persona_" + std::to_string(i))) {
            todosCorrectos = false;
            break;
        }
    }
    VERIFICAR(todosCorrectos, "todos los registros se leen identicos tras posibles evicciones LRU");
    VERIFICAR(gestorBuffer.fallos() > 0, "hubo al menos un fallo de buffer (esperado con pool de 3 marcos)");

    // --- Actualizar un registro ---
    {
        RID rid = ridsInsertados[10];
        std::vector<Valor> nuevoValor = {10, std::string("actualizado")};
        std::vector<uint8_t> bytes = Registro::serializar(nuevoValor, esquema);
        Pagina& pagina = gestorBuffer.fijarPagina(gestorArchivos, rid.numeroPagina);
        bool actualizado = pagina.actualizarRegistro(rid.numeroSlot, bytes);
        gestorBuffer.liberarPagina(gestorArchivos, rid.numeroPagina, true);
        VERIFICAR(actualizado, "actualizarRegistro devuelve true para slot valido");
    }
    gestorBuffer.vaciarTodo();  // fuerza a que la actualizacion se persista a disco
    {
        RID rid = ridsInsertados[10];
        Pagina& pagina = gestorBuffer.fijarPagina(gestorArchivos, rid.numeroPagina);
        std::vector<uint8_t> bytes;
        pagina.obtenerRegistro(rid.numeroSlot, bytes);
        gestorBuffer.liberarPagina(gestorArchivos, rid.numeroPagina, false);
        std::vector<Valor> valores = Registro::deserializar(bytes, esquema);
        VERIFICAR(std::get<std::string>(valores[1]) == "actualizado",
                  "la actualizacion persiste a disco tras vaciarTodo()");
    }

    // --- Eliminar un registro ---
    {
        RID rid = ridsInsertados[20];
        Pagina& pagina = gestorBuffer.fijarPagina(gestorArchivos, rid.numeroPagina);
        bool eliminado = pagina.eliminarRegistro(rid.numeroSlot);
        gestorBuffer.liberarPagina(gestorArchivos, rid.numeroPagina, true);
        VERIFICAR(eliminado, "eliminarRegistro devuelve true para slot valido");
    }
    {
        RID rid = ridsInsertados[20];
        Pagina& pagina = gestorBuffer.fijarPagina(gestorArchivos, rid.numeroPagina);
        VERIFICAR(!pagina.estaOcupado(rid.numeroSlot), "el slot eliminado ya no esta ocupado");
        gestorBuffer.liberarPagina(gestorArchivos, rid.numeroPagina, false);
    }

    std::cout << "\nAciertos de buffer: " << gestorBuffer.aciertos()
              << ", fallos: " << gestorBuffer.fallos()
              << ", tasa de aciertos: " << gestorBuffer.tasaAciertos() << "\n";

    if (fallas > 0) {
        std::cerr << "\n" << fallas << " verificacion(es) fallaron.\n";
        return 1;
    }
    std::cout << "\nTodas las verificaciones pasaron.\n";
    return 0;
}
