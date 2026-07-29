#pragma once

#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "almacenamiento/GestorArchivos.h"
#include "buffer/Marco.h"

namespace minisgbd {

// Identifica una página de forma única entre todos los archivos abiertos
// (tabla de datos y cada archivo de índice comparten el mismo GestorBuffer,
// igual que en un SGBD real donde un único buffer pool sirve a todos los
// archivos de la base).
struct IdPaginaGlobal {
    GestorArchivos* archivo;
    uint32_t numeroPagina;

    bool operator==(const IdPaginaGlobal& otro) const {
        return archivo == otro.archivo && numeroPagina == otro.numeroPagina;
    }
};

struct HashIdPaginaGlobal {
    size_t operator()(const IdPaginaGlobal& id) const {
        size_t h1 = std::hash<void*>()(static_cast<void*>(id.archivo));
        size_t h2 = std::hash<uint32_t>()(id.numeroPagina);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

// Buffer pool con algoritmo de reemplazo LRU, compartido entre todos los
// archivos (datos e índices). Todo acceso a páginas debe pasar por aquí:
// nadie más lee/escribe directamente en un GestorArchivos.
class GestorBuffer {
public:
    explicit GestorBuffer(size_t capacidadMarcos);

    // "Fija" (pin) la página en un marco del buffer, trayéndola de disco si
    // hace falta, y devuelve una referencia a ella. Debe liberarse luego con
    // liberarPagina().
    Pagina& fijarPagina(GestorArchivos& archivo, uint32_t numeroPagina);

    // "Libera" (unpin) la página. sucia=true si el llamador la modificó.
    void liberarPagina(GestorArchivos& archivo, uint32_t numeroPagina, bool sucia);

    // Pide una página nueva al GestorArchivos indicado y la deja fijada
    // (pin=1) en el buffer, lista para que el llamador la llene y la libere.
    uint32_t asignarPaginaNueva(GestorArchivos& archivo);

    // Escribe a disco todos los marcos sucios y limpia el estado del pool
    // (usado entre corridas de benchmark para partir de un estado en frío).
    void vaciarTodo();

    // Escribe a disco (si estaban sucios) y libera todos los marcos que
    // pertenecen a `archivo`. Debe llamarse antes de destruir un
    // GestorArchivos cuyas páginas puedan seguir cacheadas en este pool
    // compartido — si no, quedarían marcos con un puntero colgante y la
    // próxima operación sobre ellos (p.ej. una evicción) sería undefined
    // behavior. Los índices (HashExtensible, ArbolBMas) llaman a esto desde
    // su propio destructor.
    void cerrarArchivo(GestorArchivos& archivo);

    size_t aciertos() const { return contadorAciertos_; }
    size_t fallos() const { return contadorFallos_; }
    double tasaAciertos() const;

private:
    size_t capacidadMarcos_;
    std::vector<Marco> marcos_;

    std::unordered_map<IdPaginaGlobal, size_t, HashIdPaginaGlobal> tablaPaginas_;  // id -> indice en marcos_
    std::list<size_t> listaLRU_;                                                    // frente = mas reciente
    std::unordered_map<size_t, std::list<size_t>::iterator> posicionesLRU_;

    size_t contadorAciertos_ = 0;
    size_t contadorFallos_ = 0;

    size_t obtenerMarcoLibre();
    void moverAlFrenteLRU(size_t indiceMarco);
    void quitarDeLRU(size_t indiceMarco);
};

}  // namespace minisgbd
