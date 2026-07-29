// Prueba manual (no framework) del Motor de Decision de Indice + Catalogo:
// verifica que el patron de consultas observado dispare la construccion
// perezosa del indice correcto (Hash para punto, B+Tree para rango), que el
// patron mixto no dispare cambios, que el catalogo persista a disco, y que
// tras "reiniciar el proceso" el indice se reconstruya perezosamente.
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "almacenamiento/Registro.h"
#include "buffer/GestorBuffer.h"
#include "catalogo/Catalogo.h"
#include "comun/CodificadorClave.h"
#include "comun/Tipos.h"
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

static void insertarFilas(GestorBuffer& buffer, GestorArchivos& archivo, const Esquema& esquema, int totalFilas) {
    uint32_t paginaActual = buffer.asignarPaginaNueva(archivo);
    buffer.liberarPagina(archivo, paginaActual, true);

    for (int i = 0; i < totalFilas; ++i) {
        std::vector<Valor> valores = {i, i * 100};
        std::vector<uint8_t> bytes = Registro::serializar(valores, esquema);

        Pagina& pagina = buffer.fijarPagina(archivo, paginaActual);
        auto slot = pagina.insertarRegistro(bytes);
        if (!slot.has_value()) {
            buffer.liberarPagina(archivo, paginaActual, false);
            paginaActual = buffer.asignarPaginaNueva(archivo);
            buffer.liberarPagina(archivo, paginaActual, true);
            Pagina& paginaNueva = buffer.fijarPagina(archivo, paginaActual);
            slot = paginaNueva.insertarRegistro(bytes);
            buffer.liberarPagina(archivo, paginaActual, true);
        } else {
            buffer.liberarPagina(archivo, paginaActual, true);
        }
    }
}

int main() {
    std::remove("datos/test_catalogo.txt");
    std::remove("datos/test_motor_empleados.dat");
    std::remove("datos/idx_test_motor_empleados_id.idx");
    std::remove("datos/idx_test_motor_empleados_salario.idx");

    Esquema esquema;
    esquema.nombreTabla = "test_motor_empleados";
    esquema.columnas = {
        {"id", TipoDato::ENTERO, 0},
        {"salario", TipoDato::ENTERO, 0},
    };
    Columna columnaId = esquema.columnas[0];

    const int totalFilas = 500;

    std::vector<uint8_t> claveDesde = CodificadorClave::codificar(Valor(100), columnaId);
    std::vector<uint8_t> claveHasta = CodificadorClave::codificar(Valor(110), columnaId);

    {
        Catalogo catalogo("datos/test_catalogo.txt", "datos");
        catalogo.registrarTabla(esquema);
        Tabla& tabla = catalogo.obtenerTabla("test_motor_empleados");

        GestorBuffer buffer(10);
        insertarFilas(buffer, tabla.archivoDatos, esquema, totalFilas);

        GestorIndices gestorIndices(catalogo, buffer, "datos");

        VERIFICAR(gestorIndices.obtenerIndiceSiExiste("test_motor_empleados", "id") == nullptr,
                  "sin consultas previas, la columna 'id' no tiene indice");

        // --- Fase 1: 20 consultas puntuales sobre 'id' (100% igualdad) -> deberia elegir Hash ---
        for (int i = 0; i < 20; ++i) gestorIndices.registrarAcceso("test_motor_empleados", "id", TipoFiltro::IGUALDAD);

        VERIFICAR(catalogo.tipoIndiceActual("test_motor_empleados", "id") == TipoIndice::HASH,
                  "tras 20 consultas puntuales, el catalogo elige HASH para 'id'");

        IIndice* indiceHash = gestorIndices.obtenerIndiceSiExiste("test_motor_empleados", "id");
        VERIFICAR(indiceHash != nullptr, "obtenerIndiceSiExiste devuelve el indice Hash recien construido");

        if (indiceHash != nullptr) {
            std::vector<uint8_t> clave250 = CodificadorClave::codificar(Valor(250), columnaId);
            auto resultado = indiceHash->buscarPuntual(clave250);
            VERIFICAR(resultado.has_value(), "el indice Hash encuentra la clave 250");
            if (resultado.has_value()) {
                Pagina& pagina = buffer.fijarPagina(tabla.archivoDatos, resultado->numeroPagina);
                std::vector<uint8_t> bytesRegistro;
                pagina.obtenerRegistro(resultado->numeroSlot, bytesRegistro);
                buffer.liberarPagina(tabla.archivoDatos, resultado->numeroPagina, false);
                std::vector<Valor> valores = Registro::deserializar(bytesRegistro, esquema);
                VERIFICAR(std::get<int>(valores[0]) == 250,
                          "el RID que devuelve el indice apunta al registro real con id=250");
            }
        }

        // --- Fase 2: 20 consultas de rango sobre 'id' (100% rango) -> deberia cambiar a B+ Tree ---
        for (int i = 0; i < 20; ++i) gestorIndices.registrarAcceso("test_motor_empleados", "id", TipoFiltro::RANGO);

        VERIFICAR(catalogo.tipoIndiceActual("test_motor_empleados", "id") == TipoIndice::BMAS,
                  "tras 20 consultas de rango sostenidas, el catalogo cambia a BMAS para 'id'");

        IIndice* indiceArbol = gestorIndices.obtenerIndiceSiExiste("test_motor_empleados", "id");
        // No se compara indiceArbol != indiceHash: el Hash anterior ya fue
        // liberado (indicesActivos_.erase) antes de construir el B+Tree, asi
        // que el asignador de memoria bien podria reutilizar la misma
        // direccion — comparar punteros aqui no seria una prueba confiable.
        VERIFICAR(indiceArbol != nullptr, "se construyo un indice nuevo tras el cambio de politica");

        if (indiceArbol != nullptr) {
            auto rango = indiceArbol->buscarRango(claveDesde, claveHasta);
            VERIFICAR(rango.size() == 11, "el B+Tree recien construido responde correctamente buscarRango(100,110)");
        }

        // --- Fase 3: patron mixto sobre 'salario' (no supera el umbral) ---
        // Como 'salario' parte de NINGUNO, la politica por defecto en zona
        // mixta elige B+Tree (peor caso menos costoso que Hash sin indice
        // en absoluto para rangos) en vez de dejarla sin indice.
        for (int i = 0; i < 10; ++i) gestorIndices.registrarAcceso("test_motor_empleados", "salario", TipoFiltro::IGUALDAD);
        for (int i = 0; i < 10; ++i) gestorIndices.registrarAcceso("test_motor_empleados", "salario", TipoFiltro::RANGO);

        VERIFICAR(catalogo.tipoIndiceActual("test_motor_empleados", "salario") == TipoIndice::BMAS,
                  "con patron mixto 50/50 partiendo de NINGUNO, 'salario' recibe B+Tree por defecto");
        VERIFICAR(gestorIndices.obtenerIndiceSiExiste("test_motor_empleados", "salario") != nullptr,
                  "obtenerIndiceSiExiste ya no devuelve nullptr para 'salario'");

        // --- Fase 4: persistencia del catalogo (solo lectura del archivo de texto) ---
        {
            Catalogo catalogoSoloLectura("datos/test_catalogo.txt", "datos");
            VERIFICAR(catalogoSoloLectura.tipoIndiceActual("test_motor_empleados", "id") == TipoIndice::BMAS,
                      "un Catalogo nuevo recarga desde disco el tipo de indice vigente (BMAS)");
            auto [contadorIgualdad, contadorRango] = catalogoSoloLectura.contadoresActuales("test_motor_empleados", "id");
            VERIFICAR(contadorIgualdad == 0 && contadorRango == 0,
                      "los contadores quedan en 0 tras la ultima ventana de decision");
        }
        // El buffer pool es un cache de escritura diferida: las paginas de
        // datos insertadas siguen "sucias" en memoria si nunca fueron
        // desalojadas. Antes de simular un reinicio del proceso hay que
        // vaciarlo explicitamente (equivalente a un checkpoint/apagado
        // ordenado) o el archivo en disco no reflejaria los datos reales.
        buffer.vaciarTodo();
    }  // el Catalogo/Tabla/GestorArchivos originales se destruyen aqui, cerrando el archivo de datos

    // --- Fase 5: simular reinicio del proceso -> reconstruccion perezosa del indice ---
    {
        Catalogo catalogoReiniciado("datos/test_catalogo.txt", "datos");
        catalogoReiniciado.registrarTabla(esquema);  // reabre el mismo archivo de datos existente
        GestorBuffer bufferReiniciado(10);
        GestorIndices gestorReiniciado(catalogoReiniciado, bufferReiniciado, "datos");

        IIndice* indiceRecargado = gestorReiniciado.obtenerIndiceSiExiste("test_motor_empleados", "id");
        VERIFICAR(indiceRecargado != nullptr,
                  "tras 'reiniciar el proceso', el indice se reconstruye de forma perezosa al primer uso");
        if (indiceRecargado != nullptr) {
            auto rango = indiceRecargado->buscarRango(claveDesde, claveHasta);
            VERIFICAR(rango.size() == 11, "el indice reconstruido tras el 'reinicio' responde correctamente");
        }
    }

    if (fallas > 0) {
        std::cerr << "\n" << fallas << " verificacion(es) fallaron.\n";
        return 1;
    }
    std::cout << "\nTodas las verificaciones pasaron.\n";
    return 0;
}
