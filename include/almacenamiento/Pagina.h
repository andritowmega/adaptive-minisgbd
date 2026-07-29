#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace minisgbd {

// Página de tamaño fijo con organización "slotted page": los registros crecen
// desde el inicio (después del encabezado) y el directorio de slots crece
// desde el final, en sentido contrario. Cada slot guarda {offset, longitud};
// longitud == 0 indica un slot borrado (tumba). Las tumbas no se reutilizan
// en esta versión (simplificación aceptada: el espacio se recupera solo si
// se reconstruye la página; no hay compactación).
class Pagina {
public:
    static constexpr size_t TAMANO_PAGINA = 4096;

    Pagina();

    // Reinicializa la página como una página vacía con el número dado.
    void inicializar(uint32_t numeroPagina);

    uint32_t obtenerNumeroPagina() const;
    uint16_t numeroSlots() const;
    size_t espacioLibre() const;

    // Inserta el registro (longitud fija dentro de una misma tabla) y
    // devuelve el número de slot asignado, o nullopt si no hay espacio.
    std::optional<uint16_t> insertarRegistro(const std::vector<uint8_t>& datos);

    // Copia los bytes del registro en `out`. Devuelve false si el slot no
    // existe o fue borrado.
    bool obtenerRegistro(uint16_t numeroSlot, std::vector<uint8_t>& out) const;

    // Sobrescribe en el lugar; requiere que datosNuevos tenga la misma
    // longitud que el registro original (los esquemas son de longitud fija).
    bool actualizarRegistro(uint16_t numeroSlot, const std::vector<uint8_t>& datosNuevos);

    bool eliminarRegistro(uint16_t numeroSlot);

    bool estaOcupado(uint16_t numeroSlot) const;

    // Acceso a los bytes crudos, para que GestorArchivos los lea/escriba tal cual.
    uint8_t* bytesCrudos();
    const uint8_t* bytesCrudos() const;

private:
    struct EntradaSlot {
        uint16_t offset;
        uint16_t longitud;  // 0 == slot borrado
    };

    static constexpr size_t TAMANO_ENCABEZADO = 8;   // numeroPagina(4) + numeroSlots(2) + finDatos(2)
    static constexpr size_t TAMANO_ENTRADA_SLOT = 4;  // offset(2) + longitud(2)

    std::array<uint8_t, TAMANO_PAGINA> datos_;

    uint32_t leerNumeroPaginaEncabezado() const;
    void escribirNumeroPaginaEncabezado(uint32_t valor);
    uint16_t leerNumeroSlotsEncabezado() const;
    void escribirNumeroSlotsEncabezado(uint16_t valor);
    uint16_t leerFinDatosEncabezado() const;
    void escribirFinDatosEncabezado(uint16_t valor);

    EntradaSlot leerEntradaSlot(uint16_t numeroSlot) const;
    void escribirEntradaSlot(uint16_t numeroSlot, EntradaSlot entrada);

    size_t offsetDirectorioSlots(uint16_t numeroSlot) const;
};

}  // namespace minisgbd
