// Prueba manual (no framework) de HashExtensible y ArbolBMas detras de la
// interfaz comun IIndice: insercion masiva forzando splits/duplicacion de
// directorio, busqueda puntual, busqueda por rango y eliminacion.
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "buffer/GestorBuffer.h"
#include "comun/CodificadorClave.h"
#include "comun/Tipos.h"
#include "indices/ArbolBMas.h"
#include "indices/HashExtensible.h"
#include "indices/IIndice.h"

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

// Ejercita las operaciones comunes de IIndice sobre una implementacion dada,
// usando enteros [0, N) como clave y RID{i+100, 0} como carga util.
static void probarIndice(IIndice& indice, const std::string& nombre, int totalClaves) {
    Columna columnaEntero{"id", TipoDato::ENTERO, 0};

    std::vector<int> orden(totalClaves);
    for (int i = 0; i < totalClaves; ++i) orden[i] = i;
    std::mt19937 rng(42);
    std::shuffle(orden.begin(), orden.end(), rng);

    for (int i : orden) {
        std::vector<uint8_t> clave = CodificadorClave::codificar(Valor(i), columnaEntero);
        indice.insertar(clave, RID{static_cast<uint32_t>(i + 100), 0});
    }

    bool todasEncontradas = true;
    for (int i = 0; i < totalClaves; ++i) {
        std::vector<uint8_t> clave = CodificadorClave::codificar(Valor(i), columnaEntero);
        auto resultado = indice.buscarPuntual(clave);
        if (!resultado.has_value() || resultado->numeroPagina != static_cast<uint32_t>(i + 100)) {
            todasEncontradas = false;
            break;
        }
    }
    VERIFICAR(todasEncontradas, nombre + ": buscarPuntual encuentra las " + std::to_string(totalClaves) + " claves insertadas");

    {
        std::vector<uint8_t> claveInexistente = CodificadorClave::codificar(Valor(totalClaves + 500), columnaEntero);
        VERIFICAR(!indice.buscarPuntual(claveInexistente).has_value(), nombre + ": buscarPuntual no encuentra una clave inexistente");
    }

    // --- Busqueda por rango, comparada contra una referencia por fuerza bruta ---
    auto verificarRango = [&](int desde, int hasta) {
        std::vector<uint8_t> claveDesde = CodificadorClave::codificar(Valor(desde), columnaEntero);
        std::vector<uint8_t> claveHasta = CodificadorClave::codificar(Valor(hasta), columnaEntero);
        std::vector<RID> resultado = indice.buscarRango(claveDesde, claveHasta);

        std::set<uint32_t> obtenidos;
        for (RID r : resultado) obtenidos.insert(r.numeroPagina);

        std::set<uint32_t> esperados;
        for (int i = std::max(0, desde); i <= std::min(totalClaves - 1, hasta); ++i) esperados.insert(i + 100);

        bool ok = (obtenidos == esperados);
        VERIFICAR(ok, nombre + ": buscarRango(" + std::to_string(desde) + "," + std::to_string(hasta) + ") coincide con la referencia");
    };
    verificarRango(0, 10);
    verificarRango(totalClaves / 2 - 50, totalClaves / 2 + 50);
    verificarRango(-100, totalClaves + 100);
    verificarRango(totalClaves - 5, totalClaves + 500);

    // --- Eliminacion ---
    {
        std::vector<uint8_t> clave7 = CodificadorClave::codificar(Valor(7), columnaEntero);
        bool eliminado = indice.eliminar(clave7, RID{107, 0});
        VERIFICAR(eliminado, nombre + ": eliminar devuelve true para una clave existente");
        VERIFICAR(!indice.buscarPuntual(clave7).has_value(), nombre + ": la clave eliminada ya no se encuentra");

        bool eliminadoDeNuevo = indice.eliminar(clave7, RID{107, 0});
        VERIFICAR(!eliminadoDeNuevo, nombre + ": eliminar devuelve false si la clave ya no existe");
    }
}

int main() {
    std::remove("datos/test_hash.idx");
    std::remove("datos/test_btree.idx");

    // Pool de buffer pequeno y COMPARTIDO entre ambos indices, a proposito,
    // para forzar evicciones reales mientras ambas estructuras compiten por marcos.
    GestorBuffer bufferCompartido(5);

    const int totalClaves = 3000;

    {
        HashExtensible hash("datos/test_hash.idx", bufferCompartido, CodificadorClave::longitudClave({"id", TipoDato::ENTERO, 0}));
        probarIndice(hash, "HashExtensible", totalClaves);
        VERIFICAR(hash.profundidadGlobal() > 0, "HashExtensible: el directorio se duplico al menos una vez (profundidadGlobal > 0)");
    }

    {
        ArbolBMas arbol("datos/test_btree.idx", bufferCompartido, CodificadorClave::longitudClave({"id", TipoDato::ENTERO, 0}));
        probarIndice(arbol, "ArbolBMas", totalClaves);
    }

    if (fallas > 0) {
        std::cerr << "\n" << fallas << " verificacion(es) fallaron.\n";
        return 1;
    }
    std::cout << "\nTodas las verificaciones pasaron.\n";
    return 0;
}
