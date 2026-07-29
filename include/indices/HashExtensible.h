#pragma once

#include <cstdint>
#include <string>

#include "almacenamiento/GestorArchivos.h"
#include "buffer/GestorBuffer.h"
#include "indices/IIndice.h"

namespace minisgbd {

// Índice Hash Extensible, paginado en disco a través del GestorBuffer
// compartido con el resto del sistema. Óptimo para búsqueda puntual
// (O(1) esperado); buscarRango() funciona pero recorre todos los buckets —
// a propósito, para que el benchmark del artículo evidencie que esta
// estructura no conviene para consultas por rango.
//
// Formato de archivo:
//   página 0: encabezado {profundidadGlobal, longitudClave,
//             paginaDirectorioInicio, numeroPaginasDirectorio}
//   directorio: numeroPaginasDirectorio páginas contiguas de punteros
//             (uint32_t) a páginas de bucket, 1024 entradas por página
//   buckets: páginas con {profundidadLocal, numeroEntradas, entradas[]}
//
// Simplificaciones aceptadas: al duplicar el directorio se reserva un
// bloque de páginas nuevo y se abandona el anterior (no hay reciclado de
// páginas libres); al eliminar no se fusionan buckets.
class HashExtensible : public IIndice {
public:
    HashExtensible(const std::string& rutaArchivo, GestorBuffer& buffer, size_t longitudClave);
    ~HashExtensible() override;

    void insertar(const std::vector<uint8_t>& clave, RID rid) override;
    std::optional<RID> buscarPuntual(const std::vector<uint8_t>& clave) override;
    std::vector<RID> buscarRango(const std::vector<uint8_t>& desde, const std::vector<uint8_t>& hasta) override;
    bool eliminar(const std::vector<uint8_t>& clave, RID rid) override;

    uint32_t profundidadGlobal() const { return profundidadGlobal_; }

private:
    static constexpr size_t ENTRADAS_POR_PAGINA_DIRECTORIO = Pagina::TAMANO_PAGINA / 4;
    static constexpr size_t OFFSET_ENCABEZADO_BUCKET = 8;  // profundidadLocal(4) + numeroEntradas(4)

    mutable GestorArchivos archivo_;  // mutable: leerPagina() no es const, pero las lecturas del indice si lo son
    GestorBuffer& buffer_;
    size_t longitudClave_;
    size_t longitudEntradaBucket_;  // longitudClave_ + 6 (RID)
    size_t capacidadBucket_;

    uint32_t profundidadGlobal_ = 0;
    uint32_t paginaDirectorioInicio_ = 0;
    uint32_t numeroPaginasDirectorio_ = 0;

    void inicializarArchivoNuevo();
    void leerEncabezado();
    void escribirEncabezado();

    size_t calcularIndiceDirectorio(const std::vector<uint8_t>& clave) const;
    uint32_t leerEntradaDirectorio(size_t indice) const;
    void escribirEntradaDirectorio(size_t indice, uint32_t numeroPaginaBucket);

    void duplicarDirectorio();
    void dividirBucket(uint32_t numeroPaginaBucketLleno);

    void escribirEntradaEnBucket(Pagina& pagina, size_t posicion, const std::vector<uint8_t>& clave, RID rid);
    std::vector<uint8_t> leerClaveEnBucket(const Pagina& pagina, size_t posicion) const;
    RID leerRidEnBucket(const Pagina& pagina, size_t posicion) const;

    size_t hashBytes(const std::vector<uint8_t>& clave) const;
    static bool claveEnRango(const std::vector<uint8_t>& clave, const std::vector<uint8_t>& desde,
                              const std::vector<uint8_t>& hasta);
};

}  // namespace minisgbd
