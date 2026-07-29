#include "indices/ArbolBMas.h"

#include <cstring>
#include <stdexcept>

#include "indices/UtilPagina.h"

namespace minisgbd {

using namespace util_pagina;

ArbolBMas::ArbolBMas(const std::string& rutaArchivo, GestorBuffer& buffer, size_t longitudClave)
    : archivo_(rutaArchivo), buffer_(buffer), longitudClave_(longitudClave) {
    entradaInterno_ = longitudClave_ + 4;
    entradaHoja_ = longitudClave_ + 6;
    capacidadClavesInterno_ = (Pagina::TAMANO_PAGINA - OFFSET_DATOS_INTERNO - 4) / entradaInterno_;
    capacidadClavesHoja_ = (Pagina::TAMANO_PAGINA - OFFSET_DATOS_HOJA) / entradaHoja_;
    if (capacidadClavesInterno_ < 2 || capacidadClavesHoja_ < 1) {
        throw std::invalid_argument("ArbolBMas: longitud de clave demasiado grande para una pagina");
    }

    if (archivo_.numeroPaginas() == 0) {
        inicializarArchivoNuevo();
    } else {
        leerMetadata();
    }
}

ArbolBMas::~ArbolBMas() {
    // Ver HashExtensible::~HashExtensible: evita dejar marcos con un
    // puntero colgante en el GestorBuffer compartido tras destruir archivo_.
    buffer_.cerrarArchivo(archivo_);
}

void ArbolBMas::inicializarArchivoNuevo() {
    uint32_t paginaMetadata = buffer_.asignarPaginaNueva(archivo_);  // pagina 0
    buffer_.liberarPagina(archivo_, paginaMetadata, true);

    uint32_t paginaRaizInicial = buffer_.asignarPaginaNueva(archivo_);  // pagina 1
    buffer_.liberarPagina(archivo_, paginaRaizInicial, false);
    NodoMemoria raizVacia;
    raizVacia.esHoja = true;
    escribirNodo(paginaRaizInicial, raizVacia);

    paginaRaiz_ = paginaRaizInicial;
    escribirMetadata();
}

void ArbolBMas::leerMetadata() {
    Pagina& metadata = buffer_.fijarPagina(archivo_, 0);
    paginaRaiz_ = leerU32(metadata, 0);
    uint32_t longitudGuardada = leerU32(metadata, 4);
    buffer_.liberarPagina(archivo_, 0, false);

    if (longitudGuardada != longitudClave_) {
        throw std::runtime_error("ArbolBMas: la longitud de clave no coincide con el archivo existente");
    }
}

void ArbolBMas::escribirMetadata() {
    Pagina& metadata = buffer_.fijarPagina(archivo_, 0);
    escribirU32(metadata, 0, paginaRaiz_);
    escribirU32(metadata, 4, static_cast<uint32_t>(longitudClave_));
    buffer_.liberarPagina(archivo_, 0, true);
}

int ArbolBMas::compararClaves(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return std::memcmp(a.data(), b.data(), a.size());
}

ArbolBMas::NodoMemoria ArbolBMas::leerNodo(uint32_t numeroPagina) {
    Pagina& pagina = buffer_.fijarPagina(archivo_, numeroPagina);
    NodoMemoria nodo;
    nodo.esHoja = leerU32(pagina, OFFSET_ES_HOJA) != 0;
    uint32_t numeroClaves = leerU32(pagina, OFFSET_NUM_CLAVES);
    nodo.claves.resize(numeroClaves);

    if (nodo.esHoja) {
        nodo.siguienteHoja = leerU32(pagina, OFFSET_SIGUIENTE_HOJA);
        nodo.rids.resize(numeroClaves);
        for (uint32_t i = 0; i < numeroClaves; ++i) {
            size_t offset = OFFSET_DATOS_HOJA + i * entradaHoja_;
            nodo.claves[i].resize(longitudClave_);
            std::memcpy(nodo.claves[i].data(), pagina.bytesCrudos() + offset, longitudClave_);
            nodo.rids[i].numeroPagina = leerU32(pagina, offset + longitudClave_);
            nodo.rids[i].numeroSlot = leerU16(pagina, offset + longitudClave_ + 4);
        }
    } else {
        nodo.hijos.resize(numeroClaves + 1);
        nodo.hijos[0] = leerU32(pagina, OFFSET_DATOS_INTERNO);
        for (uint32_t i = 0; i < numeroClaves; ++i) {
            size_t offset = OFFSET_DATOS_INTERNO + i * entradaInterno_ + 4;
            nodo.claves[i].resize(longitudClave_);
            std::memcpy(nodo.claves[i].data(), pagina.bytesCrudos() + offset, longitudClave_);
            nodo.hijos[i + 1] = leerU32(pagina, offset + longitudClave_);
        }
    }

    buffer_.liberarPagina(archivo_, numeroPagina, false);
    return nodo;
}

void ArbolBMas::escribirNodo(uint32_t numeroPagina, const NodoMemoria& nodo) {
    Pagina& pagina = buffer_.fijarPagina(archivo_, numeroPagina);
    std::memset(pagina.bytesCrudos(), 0, Pagina::TAMANO_PAGINA);

    escribirU32(pagina, OFFSET_ES_HOJA, nodo.esHoja ? 1 : 0);
    escribirU32(pagina, OFFSET_NUM_CLAVES, static_cast<uint32_t>(nodo.claves.size()));

    if (nodo.esHoja) {
        escribirU32(pagina, OFFSET_SIGUIENTE_HOJA, nodo.siguienteHoja);
        for (size_t i = 0; i < nodo.claves.size(); ++i) {
            size_t offset = OFFSET_DATOS_HOJA + i * entradaHoja_;
            std::memcpy(pagina.bytesCrudos() + offset, nodo.claves[i].data(), longitudClave_);
            escribirU32(pagina, offset + longitudClave_, nodo.rids[i].numeroPagina);
            escribirU16(pagina, offset + longitudClave_ + 4, nodo.rids[i].numeroSlot);
        }
    } else {
        escribirU32(pagina, OFFSET_DATOS_INTERNO, nodo.hijos[0]);
        for (size_t i = 0; i < nodo.claves.size(); ++i) {
            size_t offset = OFFSET_DATOS_INTERNO + i * entradaInterno_ + 4;
            std::memcpy(pagina.bytesCrudos() + offset, nodo.claves[i].data(), longitudClave_);
            escribirU32(pagina, offset + longitudClave_, nodo.hijos[i + 1]);
        }
    }

    buffer_.liberarPagina(archivo_, numeroPagina, true);
}

void ArbolBMas::insertar(const std::vector<uint8_t>& clave, RID rid) {
    std::vector<uint32_t> camino;  // paginas de nodos internos visitados, de raiz a padre de la hoja
    uint32_t numeroPagina = paginaRaiz_;
    NodoMemoria nodo = leerNodo(numeroPagina);

    while (!nodo.esHoja) {
        camino.push_back(numeroPagina);
        size_t i = 0;
        while (i < nodo.claves.size() && compararClaves(clave, nodo.claves[i]) >= 0) i++;
        numeroPagina = nodo.hijos[i];
        nodo = leerNodo(numeroPagina);
    }

    size_t pos = 0;
    while (pos < nodo.claves.size() && compararClaves(nodo.claves[pos], clave) < 0) pos++;
    nodo.claves.insert(nodo.claves.begin() + pos, clave);
    nodo.rids.insert(nodo.rids.begin() + pos, rid);

    if (nodo.claves.size() <= capacidadClavesHoja_) {
        escribirNodo(numeroPagina, nodo);
        return;
    }

    // Split de hoja: la mitad superior se muda a una hoja nueva; se enlazan
    // mediante siguienteHoja y se promueve (copia) la primera clave de la
    // hoja derecha como separador para el padre.
    size_t mid = (nodo.claves.size() + 1) / 2;
    NodoMemoria izquierda, derecha;
    izquierda.esHoja = derecha.esHoja = true;
    izquierda.claves.assign(nodo.claves.begin(), nodo.claves.begin() + mid);
    izquierda.rids.assign(nodo.rids.begin(), nodo.rids.begin() + mid);
    derecha.claves.assign(nodo.claves.begin() + mid, nodo.claves.end());
    derecha.rids.assign(nodo.rids.begin() + mid, nodo.rids.end());
    derecha.siguienteHoja = nodo.siguienteHoja;

    uint32_t paginaNuevaHoja = buffer_.asignarPaginaNueva(archivo_);
    buffer_.liberarPagina(archivo_, paginaNuevaHoja, false);
    izquierda.siguienteHoja = paginaNuevaHoja;

    escribirNodo(numeroPagina, izquierda);
    escribirNodo(paginaNuevaHoja, derecha);

    std::vector<uint8_t> claveAPromover = derecha.claves.front();
    uint32_t paginaIzquierda = numeroPagina;
    uint32_t paginaDerecha = paginaNuevaHoja;

    for (auto it = camino.rbegin(); it != camino.rend(); ++it) {
        uint32_t paginaPadre = *it;
        NodoMemoria padre = leerNodo(paginaPadre);

        size_t pos2 = 0;
        while (pos2 < padre.claves.size() && compararClaves(padre.claves[pos2], claveAPromover) < 0) pos2++;
        padre.claves.insert(padre.claves.begin() + pos2, claveAPromover);
        padre.hijos.insert(padre.hijos.begin() + pos2 + 1, paginaDerecha);

        if (padre.claves.size() <= capacidadClavesInterno_) {
            escribirNodo(paginaPadre, padre);
            return;
        }

        // Split de nodo interno: la clave del medio sube al padre (se
        // elimina de ambos hijos, a diferencia del split de hoja).
        size_t midI = padre.claves.size() / 2;
        NodoMemoria izqI, derI;
        izqI.esHoja = derI.esHoja = false;
        izqI.claves.assign(padre.claves.begin(), padre.claves.begin() + midI);
        izqI.hijos.assign(padre.hijos.begin(), padre.hijos.begin() + midI + 1);
        derI.claves.assign(padre.claves.begin() + midI + 1, padre.claves.end());
        derI.hijos.assign(padre.hijos.begin() + midI + 1, padre.hijos.end());
        std::vector<uint8_t> claveSube = padre.claves[midI];

        uint32_t paginaNuevaInterno = buffer_.asignarPaginaNueva(archivo_);
        buffer_.liberarPagina(archivo_, paginaNuevaInterno, false);
        escribirNodo(paginaPadre, izqI);
        escribirNodo(paginaNuevaInterno, derI);

        claveAPromover = claveSube;
        paginaIzquierda = paginaPadre;
        paginaDerecha = paginaNuevaInterno;
    }

    // Se dividio hasta la raiz: crear una nueva raiz interna.
    NodoMemoria nuevaRaiz;
    nuevaRaiz.esHoja = false;
    nuevaRaiz.claves = {claveAPromover};
    nuevaRaiz.hijos = {paginaIzquierda, paginaDerecha};
    uint32_t paginaNuevaRaiz = buffer_.asignarPaginaNueva(archivo_);
    buffer_.liberarPagina(archivo_, paginaNuevaRaiz, false);
    escribirNodo(paginaNuevaRaiz, nuevaRaiz);

    paginaRaiz_ = paginaNuevaRaiz;
    escribirMetadata();
}

std::optional<RID> ArbolBMas::buscarPuntual(const std::vector<uint8_t>& clave) {
    NodoMemoria nodo = leerNodo(paginaRaiz_);
    while (!nodo.esHoja) {
        size_t i = 0;
        while (i < nodo.claves.size() && compararClaves(clave, nodo.claves[i]) >= 0) i++;
        nodo = leerNodo(nodo.hijos[i]);
    }
    for (size_t i = 0; i < nodo.claves.size(); ++i) {
        if (nodo.claves[i] == clave) return nodo.rids[i];
    }
    return std::nullopt;
}

std::vector<RID> ArbolBMas::buscarRango(const std::vector<uint8_t>& desde, const std::vector<uint8_t>& hasta) {
    std::vector<RID> resultado;

    NodoMemoria nodo = leerNodo(paginaRaiz_);
    while (!nodo.esHoja) {
        size_t i = 0;
        while (i < nodo.claves.size() && compararClaves(desde, nodo.claves[i]) >= 0) i++;
        nodo = leerNodo(nodo.hijos[i]);
    }

    while (true) {
        for (size_t i = 0; i < nodo.claves.size(); ++i) {
            if (compararClaves(nodo.claves[i], hasta) > 0) return resultado;
            if (compararClaves(nodo.claves[i], desde) >= 0) resultado.push_back(nodo.rids[i]);
        }
        if (nodo.siguienteHoja == SIN_HOJA_SIGUIENTE) break;
        nodo = leerNodo(nodo.siguienteHoja);
    }
    return resultado;
}

bool ArbolBMas::eliminar(const std::vector<uint8_t>& clave, RID rid) {
    uint32_t numeroPagina = paginaRaiz_;
    NodoMemoria nodo = leerNodo(numeroPagina);
    while (!nodo.esHoja) {
        size_t i = 0;
        while (i < nodo.claves.size() && compararClaves(clave, nodo.claves[i]) >= 0) i++;
        numeroPagina = nodo.hijos[i];
        nodo = leerNodo(numeroPagina);
    }

    for (size_t i = 0; i < nodo.claves.size(); ++i) {
        if (nodo.claves[i] == clave && nodo.rids[i] == rid) {
            nodo.claves.erase(nodo.claves.begin() + i);
            nodo.rids.erase(nodo.rids.begin() + i);
            escribirNodo(numeroPagina, nodo);
            return true;
        }
    }
    return false;
}

}  // namespace minisgbd
