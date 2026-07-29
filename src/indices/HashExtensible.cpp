#include "indices/HashExtensible.h"

#include <cstring>
#include <stdexcept>
#include <unordered_set>

#include "indices/UtilPagina.h"

namespace minisgbd {

using namespace util_pagina;

HashExtensible::HashExtensible(const std::string& rutaArchivo, GestorBuffer& buffer, size_t longitudClave)
    : archivo_(rutaArchivo), buffer_(buffer), longitudClave_(longitudClave) {
    longitudEntradaBucket_ = longitudClave_ + 6;  // clave + RID (numeroPagina u32 + numeroSlot u16)
    capacidadBucket_ = (Pagina::TAMANO_PAGINA - OFFSET_ENCABEZADO_BUCKET) / longitudEntradaBucket_;
    if (capacidadBucket_ == 0) {
        throw std::invalid_argument("HashExtensible: longitud de clave demasiado grande para una pagina");
    }

    if (archivo_.numeroPaginas() == 0) {
        inicializarArchivoNuevo();
    } else {
        leerEncabezado();
    }
}

HashExtensible::~HashExtensible() {
    // Antes de que archivo_ se destruya, hay que asegurarse de que el
    // GestorBuffer compartido no conserve marcos con un puntero colgante a
    // este archivo (ver GestorBuffer::cerrarArchivo).
    buffer_.cerrarArchivo(archivo_);
}

void HashExtensible::inicializarArchivoNuevo() {
    uint32_t paginaEncabezado = buffer_.asignarPaginaNueva(archivo_);  // pagina 0
    buffer_.liberarPagina(archivo_, paginaEncabezado, true);

    paginaDirectorioInicio_ = buffer_.asignarPaginaNueva(archivo_);  // pagina 1
    buffer_.liberarPagina(archivo_, paginaDirectorioInicio_, true);
    numeroPaginasDirectorio_ = 1;
    profundidadGlobal_ = 0;

    uint32_t paginaBucketInicial = buffer_.asignarPaginaNueva(archivo_);  // pagina 2
    buffer_.liberarPagina(archivo_, paginaBucketInicial, false);
    Pagina& bucket = buffer_.fijarPagina(archivo_, paginaBucketInicial);
    escribirU32(bucket, 0, 0);  // profundidadLocal
    escribirU32(bucket, 4, 0);  // numeroEntradas
    buffer_.liberarPagina(archivo_, paginaBucketInicial, true);

    escribirEntradaDirectorio(0, paginaBucketInicial);
    escribirEncabezado();
}

void HashExtensible::leerEncabezado() {
    Pagina& encabezado = buffer_.fijarPagina(archivo_, 0);
    profundidadGlobal_ = leerU32(encabezado, 0);
    uint32_t longitudGuardada = leerU32(encabezado, 4);
    paginaDirectorioInicio_ = leerU32(encabezado, 8);
    numeroPaginasDirectorio_ = leerU32(encabezado, 12);
    buffer_.liberarPagina(archivo_, 0, false);

    if (longitudGuardada != longitudClave_) {
        throw std::runtime_error("HashExtensible: la longitud de clave no coincide con el archivo existente");
    }
}

void HashExtensible::escribirEncabezado() {
    Pagina& encabezado = buffer_.fijarPagina(archivo_, 0);
    escribirU32(encabezado, 0, profundidadGlobal_);
    escribirU32(encabezado, 4, static_cast<uint32_t>(longitudClave_));
    escribirU32(encabezado, 8, paginaDirectorioInicio_);
    escribirU32(encabezado, 12, numeroPaginasDirectorio_);
    buffer_.liberarPagina(archivo_, 0, true);
}

size_t HashExtensible::hashBytes(const std::vector<uint8_t>& clave) const {
    return std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(clave.data()), clave.size()));
}

size_t HashExtensible::calcularIndiceDirectorio(const std::vector<uint8_t>& clave) const {
    size_t mascara = (profundidadGlobal_ == 0) ? 0 : ((size_t{1} << profundidadGlobal_) - 1);
    return hashBytes(clave) & mascara;
}

uint32_t HashExtensible::leerEntradaDirectorio(size_t indice) const {
    uint32_t numeroPagina = paginaDirectorioInicio_ + static_cast<uint32_t>(indice / ENTRADAS_POR_PAGINA_DIRECTORIO);
    size_t offset = (indice % ENTRADAS_POR_PAGINA_DIRECTORIO) * 4;
    Pagina& pagina = buffer_.fijarPagina(archivo_, numeroPagina);
    uint32_t valor = leerU32(pagina, offset);
    buffer_.liberarPagina(archivo_, numeroPagina, false);
    return valor;
}

void HashExtensible::escribirEntradaDirectorio(size_t indice, uint32_t numeroPaginaBucket) {
    uint32_t numeroPagina = paginaDirectorioInicio_ + static_cast<uint32_t>(indice / ENTRADAS_POR_PAGINA_DIRECTORIO);
    size_t offset = (indice % ENTRADAS_POR_PAGINA_DIRECTORIO) * 4;
    Pagina& pagina = buffer_.fijarPagina(archivo_, numeroPagina);
    escribirU32(pagina, offset, numeroPaginaBucket);
    buffer_.liberarPagina(archivo_, numeroPagina, true);
}

void HashExtensible::escribirEntradaEnBucket(Pagina& pagina, size_t posicion, const std::vector<uint8_t>& clave,
                                              RID rid) {
    size_t offset = OFFSET_ENCABEZADO_BUCKET + posicion * longitudEntradaBucket_;
    std::memcpy(pagina.bytesCrudos() + offset, clave.data(), longitudClave_);
    escribirU32(pagina, offset + longitudClave_, rid.numeroPagina);
    escribirU16(pagina, offset + longitudClave_ + 4, rid.numeroSlot);
}

std::vector<uint8_t> HashExtensible::leerClaveEnBucket(const Pagina& pagina, size_t posicion) const {
    size_t offset = OFFSET_ENCABEZADO_BUCKET + posicion * longitudEntradaBucket_;
    std::vector<uint8_t> clave(longitudClave_);
    std::memcpy(clave.data(), pagina.bytesCrudos() + offset, longitudClave_);
    return clave;
}

RID HashExtensible::leerRidEnBucket(const Pagina& pagina, size_t posicion) const {
    size_t offset = OFFSET_ENCABEZADO_BUCKET + posicion * longitudEntradaBucket_;
    RID rid;
    rid.numeroPagina = leerU32(pagina, offset + longitudClave_);
    rid.numeroSlot = leerU16(pagina, offset + longitudClave_ + 4);
    return rid;
}

void HashExtensible::insertar(const std::vector<uint8_t>& clave, RID rid) {
    size_t idx = calcularIndiceDirectorio(clave);
    uint32_t numeroPaginaBucket = leerEntradaDirectorio(idx);

    Pagina& bucket = buffer_.fijarPagina(archivo_, numeroPaginaBucket);
    uint32_t numeroEntradas = leerU32(bucket, 4);

    if (numeroEntradas < capacidadBucket_) {
        escribirEntradaEnBucket(bucket, numeroEntradas, clave, rid);
        escribirU32(bucket, 4, numeroEntradas + 1);
        buffer_.liberarPagina(archivo_, numeroPaginaBucket, true);
        return;
    }

    buffer_.liberarPagina(archivo_, numeroPaginaBucket, false);
    dividirBucket(numeroPaginaBucket);
    insertar(clave, rid);  // reintenta tras el split (termina: el split reduce la ocupacion relativa)
}

void HashExtensible::duplicarDirectorio() {
    uint32_t profundidadVieja = profundidadGlobal_;
    size_t numeroEntradasViejas = size_t{1} << profundidadVieja;

    std::vector<uint32_t> entradasViejas(numeroEntradasViejas);
    for (size_t i = 0; i < numeroEntradasViejas; ++i) entradasViejas[i] = leerEntradaDirectorio(i);

    uint32_t profundidadNueva = profundidadVieja + 1;
    size_t numeroEntradasNuevas = size_t{1} << profundidadNueva;
    uint32_t numeroPaginasNuevo =
        static_cast<uint32_t>((numeroEntradasNuevas * 4 + Pagina::TAMANO_PAGINA - 1) / Pagina::TAMANO_PAGINA);

    uint32_t primeraPaginaNueva = buffer_.asignarPaginaNueva(archivo_);
    buffer_.liberarPagina(archivo_, primeraPaginaNueva, true);
    for (uint32_t i = 1; i < numeroPaginasNuevo; ++i) {
        uint32_t p = buffer_.asignarPaginaNueva(archivo_);
        buffer_.liberarPagina(archivo_, p, true);
    }

    uint32_t paginaDirectorioViejo = paginaDirectorioInicio_;
    uint32_t numeroPaginasDirectorioViejo = numeroPaginasDirectorio_;

    paginaDirectorioInicio_ = primeraPaginaNueva;
    numeroPaginasDirectorio_ = numeroPaginasNuevo;
    profundidadGlobal_ = profundidadNueva;

    for (size_t i = 0; i < numeroEntradasNuevas; ++i) {
        escribirEntradaDirectorio(i, entradasViejas[i % numeroEntradasViejas]);
    }

    escribirEncabezado();
    (void)paginaDirectorioViejo;
    (void)numeroPaginasDirectorioViejo;  // paginas abandonadas a proposito (ver nota de diseno)
}

void HashExtensible::dividirBucket(uint32_t numeroPaginaBucketLleno) {
    Pagina& bucketViejo = buffer_.fijarPagina(archivo_, numeroPaginaBucketLleno);
    uint32_t profundidadLocal = leerU32(bucketViejo, 0);
    buffer_.liberarPagina(archivo_, numeroPaginaBucketLleno, false);

    if (profundidadLocal == profundidadGlobal_) {
        duplicarDirectorio();
    }

    uint32_t nuevaProfundidadLocal = profundidadLocal + 1;
    uint32_t bitDistintivo = nuevaProfundidadLocal - 1;

    // Copiar las entradas actuales del bucket lleno antes de reescribirlo.
    Pagina& bucketLleno = buffer_.fijarPagina(archivo_, numeroPaginaBucketLleno);
    uint32_t numeroEntradasViejas = leerU32(bucketLleno, 4);
    std::vector<std::vector<uint8_t>> clavesViejas(numeroEntradasViejas);
    std::vector<RID> ridsViejos(numeroEntradasViejas);
    for (uint32_t i = 0; i < numeroEntradasViejas; ++i) {
        clavesViejas[i] = leerClaveEnBucket(bucketLleno, i);
        ridsViejos[i] = leerRidEnBucket(bucketLleno, i);
    }

    uint32_t numeroPaginaNuevo = buffer_.asignarPaginaNueva(archivo_);
    buffer_.liberarPagina(archivo_, numeroPaginaNuevo, false);
    Pagina& bucketNuevo = buffer_.fijarPagina(archivo_, numeroPaginaNuevo);
    escribirU32(bucketNuevo, 0, nuevaProfundidadLocal);
    escribirU32(bucketNuevo, 4, 0);

    escribirU32(bucketLleno, 0, nuevaProfundidadLocal);
    escribirU32(bucketLleno, 4, 0);

    uint32_t contadorViejo = 0;
    uint32_t contadorNuevo = 0;
    for (uint32_t i = 0; i < numeroEntradasViejas; ++i) {
        size_t h = hashBytes(clavesViejas[i]);
        bool vaAlNuevo = ((h >> bitDistintivo) & 1) != 0;
        if (vaAlNuevo) {
            escribirEntradaEnBucket(bucketNuevo, contadorNuevo, clavesViejas[i], ridsViejos[i]);
            contadorNuevo++;
        } else {
            escribirEntradaEnBucket(bucketLleno, contadorViejo, clavesViejas[i], ridsViejos[i]);
            contadorViejo++;
        }
    }
    escribirU32(bucketLleno, 4, contadorViejo);
    escribirU32(bucketNuevo, 4, contadorNuevo);

    buffer_.liberarPagina(archivo_, numeroPaginaBucketLleno, true);
    buffer_.liberarPagina(archivo_, numeroPaginaNuevo, true);

    size_t totalEntradasDirectorio = size_t{1} << profundidadGlobal_;
    for (size_t i = 0; i < totalEntradasDirectorio; ++i) {
        if (leerEntradaDirectorio(i) != numeroPaginaBucketLleno) continue;
        bool debeApuntarAlNuevo = ((i >> bitDistintivo) & 1) != 0;
        if (debeApuntarAlNuevo) escribirEntradaDirectorio(i, numeroPaginaNuevo);
    }
}

std::optional<RID> HashExtensible::buscarPuntual(const std::vector<uint8_t>& clave) {
    size_t idx = calcularIndiceDirectorio(clave);
    uint32_t numeroPaginaBucket = leerEntradaDirectorio(idx);

    Pagina& bucket = buffer_.fijarPagina(archivo_, numeroPaginaBucket);
    uint32_t numeroEntradas = leerU32(bucket, 4);
    std::optional<RID> resultado;
    for (uint32_t i = 0; i < numeroEntradas; ++i) {
        std::vector<uint8_t> claveGuardada = leerClaveEnBucket(bucket, i);
        if (claveGuardada == clave) {
            resultado = leerRidEnBucket(bucket, i);
            break;
        }
    }
    buffer_.liberarPagina(archivo_, numeroPaginaBucket, false);
    return resultado;
}

bool HashExtensible::claveEnRango(const std::vector<uint8_t>& clave, const std::vector<uint8_t>& desde,
                                   const std::vector<uint8_t>& hasta) {
    return std::memcmp(clave.data(), desde.data(), clave.size()) >= 0 &&
           std::memcmp(clave.data(), hasta.data(), clave.size()) <= 0;
}

std::vector<RID> HashExtensible::buscarRango(const std::vector<uint8_t>& desde, const std::vector<uint8_t>& hasta) {
    // Recorrido completo de todos los buckets: el Hash Extensible no ofrece
    // orden, por lo que no hay forma de acotar la busqueda por rango.
    std::vector<RID> resultado;
    std::unordered_set<uint32_t> paginasVisitadas;
    size_t totalEntradasDirectorio = size_t{1} << profundidadGlobal_;

    for (size_t i = 0; i < totalEntradasDirectorio; ++i) {
        uint32_t numeroPaginaBucket = leerEntradaDirectorio(i);
        if (!paginasVisitadas.insert(numeroPaginaBucket).second) continue;

        Pagina& bucket = buffer_.fijarPagina(archivo_, numeroPaginaBucket);
        uint32_t numeroEntradas = leerU32(bucket, 4);
        for (uint32_t j = 0; j < numeroEntradas; ++j) {
            std::vector<uint8_t> claveGuardada = leerClaveEnBucket(bucket, j);
            if (claveEnRango(claveGuardada, desde, hasta)) {
                resultado.push_back(leerRidEnBucket(bucket, j));
            }
        }
        buffer_.liberarPagina(archivo_, numeroPaginaBucket, false);
    }
    return resultado;
}

bool HashExtensible::eliminar(const std::vector<uint8_t>& clave, RID rid) {
    size_t idx = calcularIndiceDirectorio(clave);
    uint32_t numeroPaginaBucket = leerEntradaDirectorio(idx);

    Pagina& bucket = buffer_.fijarPagina(archivo_, numeroPaginaBucket);
    uint32_t numeroEntradas = leerU32(bucket, 4);
    int posicionEncontrada = -1;
    for (uint32_t i = 0; i < numeroEntradas; ++i) {
        if (leerClaveEnBucket(bucket, i) == clave && leerRidEnBucket(bucket, i) == rid) {
            posicionEncontrada = static_cast<int>(i);
            break;
        }
    }

    if (posicionEncontrada < 0) {
        buffer_.liberarPagina(archivo_, numeroPaginaBucket, false);
        return false;
    }

    // Compactar: mover la ultima entrada a la posicion eliminada (no se preserva orden, no importa en un hash).
    if (static_cast<uint32_t>(posicionEncontrada) != numeroEntradas - 1) {
        std::vector<uint8_t> ultimaClave = leerClaveEnBucket(bucket, numeroEntradas - 1);
        RID ultimoRid = leerRidEnBucket(bucket, numeroEntradas - 1);
        escribirEntradaEnBucket(bucket, posicionEncontrada, ultimaClave, ultimoRid);
    }
    escribirU32(bucket, 4, numeroEntradas - 1);
    buffer_.liberarPagina(archivo_, numeroPaginaBucket, true);
    return true;
}

}  // namespace minisgbd
