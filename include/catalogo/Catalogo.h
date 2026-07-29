#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "almacenamiento/GestorArchivos.h"
#include "comun/Tipos.h"

namespace minisgbd {

// Una tabla registrada: su esquema y el archivo de páginas donde viven sus
// registros. No es copiable (GestorArchivos no lo es) — vive dentro de
// Catalogo::tablas_, construida in-place con try_emplace.
struct Tabla {
    Esquema esquema;
    GestorArchivos archivoDatos;

    Tabla(Esquema esquemaTabla, const std::string& rutaArchivoDatos)
        : esquema(std::move(esquemaTabla)), archivoDatos(rutaArchivoDatos) {}
};

// Metadata del sistema: esquemas de tabla y, por columna, el tipo de índice
// vigente más los contadores de acceso que alimentan al Motor de Decisión.
// Todo persiste en el mismo archivo de texto plano, con dos formatos de
// línea distinguidos por un prefijo:
//   TABLA,nombre,numColumnas,(col,TIPO,longitud)*    -- esquema de una tabla
//   COLUMNA,tabla,columna,tipo_indice,contador_igualdad,contador_rango
// Los esquemas se reescriben apenas se registra una tabla nueva; el estado
// de índice se reescribe cada vez que se cierra una ventana de decisión (no
// en cada consulta individual, para no dominar el tiempo de los benchmarks
// con E/S del catálogo). Sin versionado ni manejo de corrupción: una línea
// con un prefijo desconocido o un formato inesperado simplemente se ignora.
class Catalogo {
public:
    Catalogo(std::string rutaArchivoCatalogo, std::string directorioDatos);

    void registrarTabla(const Esquema& esquema);
    bool existeTabla(const std::string& nombreTabla) const;
    Tabla& obtenerTabla(const std::string& nombreTabla);

    // Nombres de las tablas registradas en esta sesión (orden alfabético,
    // por ser std::map) — pensado para un comando "listar tablas" en el REPL.
    std::vector<std::string> nombresTablas() const;

    // Contadores de acceso por columna, usados por el Motor de Decisión.
    void registrarAcceso(const std::string& tabla, const std::string& columna, TipoFiltro tipo);
    TipoIndice tipoIndiceActual(const std::string& tabla, const std::string& columna) const;
    std::pair<uint64_t, uint64_t> contadoresActuales(const std::string& tabla, const std::string& columna) const;

    // Fija el nuevo tipo de índice vigente, resetea los contadores de la
    // ventana y persiste el catálogo completo a disco.
    void actualizarTipoIndiceYResetear(const std::string& tabla, const std::string& columna, TipoIndice nuevoTipo);

private:
    struct EstadoColumna {
        TipoIndice tipo = TipoIndice::NINGUNO;
        uint64_t contadorIgualdad = 0;
        uint64_t contadorRango = 0;
    };

    std::string rutaArchivoCatalogo_;
    std::string directorioDatos_;
    std::map<std::string, Tabla> tablas_;
    std::map<std::pair<std::string, std::string>, EstadoColumna> estadoColumnas_;

    void cargarDesdeDisco();
    void guardarADisco() const;
    void registrarTablaSinPersistir(const Esquema& esquema);

    static std::string tipoIndiceATexto(TipoIndice tipo);
    static TipoIndice textoATipoIndice(const std::string& texto);
};

}  // namespace minisgbd
