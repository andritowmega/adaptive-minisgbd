#pragma once

#include <cstdint>
#include <string>

#include "almacenamiento/GestorArchivos.h"
#include "buffer/GestorBuffer.h"
#include "indices/IIndice.h"

namespace minisgbd {

// Índice B+ Tree, paginado en disco a través del GestorBuffer compartido.
// Óptimo para búsqueda por rango (recorrido secuencial de hojas enlazadas);
// buscarPuntual también es O(log n) vía descenso desde la raíz.
//
// Formato de archivo:
//   página 0: metadata {paginaRaiz, longitudClave}
//   nodo interno: {esHoja=0, numeroClaves, hijo0, (clave0,hijo1), (clave1,hijo2), ...}
//   nodo hoja:    {esHoja=1, numeroClaves, siguienteHoja, (clave0,RID0), (clave1,RID1), ...}
//
// Simplificación aceptada: eliminar() no fusiona ni redistribuye nodos bajo
// el mínimo de ocupación (solo se implementa el rebalanceo de inserción,
// que es el que garantiza la estructura O(log n) para búsqueda/inserción;
// common en implementaciones académicas de B+ Tree).
class ArbolBMas : public IIndice {
public:
    ArbolBMas(const std::string& rutaArchivo, GestorBuffer& buffer, size_t longitudClave);
    ~ArbolBMas() override;

    void insertar(const std::vector<uint8_t>& clave, RID rid) override;
    std::optional<RID> buscarPuntual(const std::vector<uint8_t>& clave) override;
    std::vector<RID> buscarRango(const std::vector<uint8_t>& desde, const std::vector<uint8_t>& hasta) override;
    bool eliminar(const std::vector<uint8_t>& clave, RID rid) override;

private:
    struct NodoMemoria {
        bool esHoja = true;
        std::vector<std::vector<uint8_t>> claves;
        std::vector<uint32_t> hijos;       // tamaño = claves.size()+1, solo si !esHoja
        std::vector<RID> rids;             // tamaño = claves.size(), solo si esHoja
        uint32_t siguienteHoja = 0;         // 0 == sin siguiente (pagina 0 es metadata, nunca una hoja)
    };

    static constexpr uint32_t SIN_HOJA_SIGUIENTE = 0;
    static constexpr size_t OFFSET_ES_HOJA = 0;
    static constexpr size_t OFFSET_NUM_CLAVES = 4;
    static constexpr size_t OFFSET_DATOS_INTERNO = 8;
    static constexpr size_t OFFSET_SIGUIENTE_HOJA = 8;
    static constexpr size_t OFFSET_DATOS_HOJA = 12;

    GestorArchivos archivo_;
    GestorBuffer& buffer_;
    size_t longitudClave_;
    size_t entradaInterno_;   // longitudClave_ + 4 (puntero a hijo)
    size_t entradaHoja_;      // longitudClave_ + 6 (RID)
    size_t capacidadClavesInterno_;
    size_t capacidadClavesHoja_;

    uint32_t paginaRaiz_ = 0;

    void inicializarArchivoNuevo();
    void leerMetadata();
    void escribirMetadata();

    NodoMemoria leerNodo(uint32_t numeroPagina);
    void escribirNodo(uint32_t numeroPagina, const NodoMemoria& nodo);

    static int compararClaves(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
};

}  // namespace minisgbd
