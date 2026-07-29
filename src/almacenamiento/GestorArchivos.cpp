#include "almacenamiento/GestorArchivos.h"

#include <stdexcept>

namespace minisgbd {

GestorArchivos::GestorArchivos(const std::string& rutaArchivo) : rutaArchivo_(rutaArchivo) {
    // Si el archivo no existe, se crea vacío antes de reabrirlo en modo
    // lectura/escritura binaria (fstream no crea archivos por sí solo).
    std::fstream prueba(rutaArchivo_, std::ios::in | std::ios::binary);
    if (!prueba.is_open()) {
        std::ofstream crear(rutaArchivo_, std::ios::binary);
    } else {
        prueba.close();
    }

    archivo_.open(rutaArchivo_, std::ios::in | std::ios::out | std::ios::binary);
    if (!archivo_.is_open()) {
        throw std::runtime_error("GestorArchivos: no se pudo abrir " + rutaArchivo_);
    }

    archivo_.seekg(0, std::ios::end);
    std::streampos tamano = archivo_.tellg();
    numeroPaginas_ = static_cast<uint32_t>(tamano / static_cast<std::streamoff>(Pagina::TAMANO_PAGINA));
}

GestorArchivos::~GestorArchivos() {
    if (archivo_.is_open()) archivo_.close();
}

uint32_t GestorArchivos::asignarNuevaPagina() {
    Pagina paginaVacia;
    uint32_t numero = numeroPaginas_;
    paginaVacia.inicializar(numero);
    escribirPagina(numero, paginaVacia);
    numeroPaginas_++;
    return numero;
}

void GestorArchivos::leerPagina(uint32_t numeroPagina, Pagina& destino) {
    if (numeroPagina >= numeroPaginas_) {
        throw std::out_of_range("GestorArchivos::leerPagina: numero de pagina fuera de rango");
    }
    std::streamoff offset = static_cast<std::streamoff>(numeroPagina) * Pagina::TAMANO_PAGINA;
    archivo_.seekg(offset, std::ios::beg);
    archivo_.read(reinterpret_cast<char*>(destino.bytesCrudos()), Pagina::TAMANO_PAGINA);
    if (!archivo_) {
        throw std::runtime_error("GestorArchivos::leerPagina: fallo de lectura");
    }
}

void GestorArchivos::escribirPagina(uint32_t numeroPagina, const Pagina& pagina) {
    std::streamoff offset = static_cast<std::streamoff>(numeroPagina) * Pagina::TAMANO_PAGINA;
    archivo_.seekp(offset, std::ios::beg);
    archivo_.write(reinterpret_cast<const char*>(pagina.bytesCrudos()), Pagina::TAMANO_PAGINA);
    if (!archivo_) {
        throw std::runtime_error("GestorArchivos::escribirPagina: fallo de escritura");
    }
    archivo_.flush();
}

uint32_t GestorArchivos::numeroPaginas() const { return numeroPaginas_; }

}  // namespace minisgbd
