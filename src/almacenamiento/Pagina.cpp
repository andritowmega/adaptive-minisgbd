#include "almacenamiento/Pagina.h"

#include <cstring>

namespace minisgbd {

Pagina::Pagina() {
    inicializar(0);
}

void Pagina::inicializar(uint32_t numeroPagina) {
    datos_.fill(0);
    escribirNumeroPaginaEncabezado(numeroPagina);
    escribirNumeroSlotsEncabezado(0);
    escribirFinDatosEncabezado(static_cast<uint16_t>(TAMANO_ENCABEZADO));
}

uint32_t Pagina::obtenerNumeroPagina() const { return leerNumeroPaginaEncabezado(); }
uint16_t Pagina::numeroSlots() const { return leerNumeroSlotsEncabezado(); }

size_t Pagina::espacioLibre() const {
    size_t finDatos = leerFinDatosEncabezado();
    size_t inicioDirectorio = TAMANO_PAGINA - static_cast<size_t>(numeroSlots()) * TAMANO_ENTRADA_SLOT;
    return inicioDirectorio > finDatos ? inicioDirectorio - finDatos : 0;
}

std::optional<uint16_t> Pagina::insertarRegistro(const std::vector<uint8_t>& datosRegistro) {
    uint16_t total = numeroSlots();
    size_t necesario = datosRegistro.size();
    size_t finDatos = leerFinDatosEncabezado();
    size_t inicioDirectorio = TAMANO_PAGINA - static_cast<size_t>(total) * TAMANO_ENTRADA_SLOT;

    // ¿Hay espacio para un slot nuevo (datos + entrada de directorio)?
    if (finDatos + necesario + TAMANO_ENTRADA_SLOT <= inicioDirectorio) {
        std::memcpy(datos_.data() + finDatos, datosRegistro.data(), necesario);
        EntradaSlot nueva{static_cast<uint16_t>(finDatos), static_cast<uint16_t>(necesario)};
        escribirEntradaSlot(total, nueva);
        escribirNumeroSlotsEncabezado(static_cast<uint16_t>(total + 1));
        escribirFinDatosEncabezado(static_cast<uint16_t>(finDatos + necesario));
        return total;
    }

    return std::nullopt;
}

bool Pagina::obtenerRegistro(uint16_t numeroSlot, std::vector<uint8_t>& out) const {
    if (numeroSlot >= numeroSlots()) return false;
    EntradaSlot entrada = leerEntradaSlot(numeroSlot);
    if (entrada.longitud == 0) return false;
    out.resize(entrada.longitud);
    std::memcpy(out.data(), datos_.data() + entrada.offset, entrada.longitud);
    return true;
}

bool Pagina::actualizarRegistro(uint16_t numeroSlot, const std::vector<uint8_t>& datosNuevos) {
    if (numeroSlot >= numeroSlots()) return false;
    EntradaSlot entrada = leerEntradaSlot(numeroSlot);
    if (entrada.longitud == 0) return false;
    if (datosNuevos.size() != entrada.longitud) return false;  // esquema de longitud fija
    std::memcpy(datos_.data() + entrada.offset, datosNuevos.data(), datosNuevos.size());
    return true;
}

bool Pagina::eliminarRegistro(uint16_t numeroSlot) {
    if (numeroSlot >= numeroSlots()) return false;
    EntradaSlot entrada = leerEntradaSlot(numeroSlot);
    if (entrada.longitud == 0) return false;
    entrada.longitud = 0;
    escribirEntradaSlot(numeroSlot, entrada);
    return true;
}

bool Pagina::estaOcupado(uint16_t numeroSlot) const {
    if (numeroSlot >= numeroSlots()) return false;
    return leerEntradaSlot(numeroSlot).longitud != 0;
}

uint8_t* Pagina::bytesCrudos() { return datos_.data(); }
const uint8_t* Pagina::bytesCrudos() const { return datos_.data(); }

uint32_t Pagina::leerNumeroPaginaEncabezado() const {
    uint32_t valor;
    std::memcpy(&valor, datos_.data() + 0, sizeof(valor));
    return valor;
}
void Pagina::escribirNumeroPaginaEncabezado(uint32_t valor) {
    std::memcpy(datos_.data() + 0, &valor, sizeof(valor));
}
uint16_t Pagina::leerNumeroSlotsEncabezado() const {
    uint16_t valor;
    std::memcpy(&valor, datos_.data() + 4, sizeof(valor));
    return valor;
}
void Pagina::escribirNumeroSlotsEncabezado(uint16_t valor) {
    std::memcpy(datos_.data() + 4, &valor, sizeof(valor));
}
uint16_t Pagina::leerFinDatosEncabezado() const {
    uint16_t valor;
    std::memcpy(&valor, datos_.data() + 6, sizeof(valor));
    return valor;
}
void Pagina::escribirFinDatosEncabezado(uint16_t valor) {
    std::memcpy(datos_.data() + 6, &valor, sizeof(valor));
}

size_t Pagina::offsetDirectorioSlots(uint16_t numeroSlot) const {
    return TAMANO_PAGINA - (static_cast<size_t>(numeroSlot) + 1) * TAMANO_ENTRADA_SLOT;
}

Pagina::EntradaSlot Pagina::leerEntradaSlot(uint16_t numeroSlot) const {
    EntradaSlot entrada;
    size_t offset = offsetDirectorioSlots(numeroSlot);
    std::memcpy(&entrada.offset, datos_.data() + offset, sizeof(entrada.offset));
    std::memcpy(&entrada.longitud, datos_.data() + offset + 2, sizeof(entrada.longitud));
    return entrada;
}

void Pagina::escribirEntradaSlot(uint16_t numeroSlot, EntradaSlot entrada) {
    size_t offset = offsetDirectorioSlots(numeroSlot);
    std::memcpy(datos_.data() + offset, &entrada.offset, sizeof(entrada.offset));
    std::memcpy(datos_.data() + offset + 2, &entrada.longitud, sizeof(entrada.longitud));
}

}  // namespace minisgbd
