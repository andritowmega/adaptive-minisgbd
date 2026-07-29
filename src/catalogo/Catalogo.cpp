#include "catalogo/Catalogo.h"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace minisgbd {

namespace {

std::vector<std::string> dividirPorComas(const std::string& linea) {
    std::vector<std::string> campos;
    size_t inicio = 0;
    while (true) {
        size_t coma = linea.find(',', inicio);
        if (coma == std::string::npos) {
            campos.push_back(linea.substr(inicio));
            break;
        }
        campos.push_back(linea.substr(inicio, coma - inicio));
        inicio = coma + 1;
    }
    return campos;
}

}  // namespace

Catalogo::Catalogo(std::string rutaArchivoCatalogo, std::string directorioDatos)
    : rutaArchivoCatalogo_(std::move(rutaArchivoCatalogo)), directorioDatos_(std::move(directorioDatos)) {
    cargarDesdeDisco();
}

void Catalogo::registrarTablaSinPersistir(const Esquema& esquema) {
    if (tablas_.find(esquema.nombreTabla) != tablas_.end()) return;  // idempotente
    std::string ruta = directorioDatos_ + "/" + esquema.nombreTabla + ".dat";
    tablas_.try_emplace(esquema.nombreTabla, esquema, ruta);
}

void Catalogo::registrarTabla(const Esquema& esquema) {
    if (tablas_.find(esquema.nombreTabla) != tablas_.end()) return;  // idempotente
    registrarTablaSinPersistir(esquema);
    guardarADisco();  // persiste el esquema nuevo para que sobreviva a un reinicio
}

bool Catalogo::existeTabla(const std::string& nombreTabla) const {
    return tablas_.find(nombreTabla) != tablas_.end();
}

Tabla& Catalogo::obtenerTabla(const std::string& nombreTabla) {
    auto it = tablas_.find(nombreTabla);
    if (it == tablas_.end()) throw std::runtime_error("Catalogo: tabla no registrada: " + nombreTabla);
    return it->second;
}

std::vector<std::string> Catalogo::nombresTablas() const {
    std::vector<std::string> nombres;
    nombres.reserve(tablas_.size());
    for (const auto& [nombre, tabla] : tablas_) nombres.push_back(nombre);
    return nombres;
}

void Catalogo::registrarAcceso(const std::string& tabla, const std::string& columna, TipoFiltro tipo) {
    EstadoColumna& estado = estadoColumnas_[{tabla, columna}];
    if (tipo == TipoFiltro::IGUALDAD) {
        estado.contadorIgualdad++;
    } else {
        estado.contadorRango++;
    }
}

TipoIndice Catalogo::tipoIndiceActual(const std::string& tabla, const std::string& columna) const {
    auto it = estadoColumnas_.find({tabla, columna});
    return it == estadoColumnas_.end() ? TipoIndice::NINGUNO : it->second.tipo;
}

std::pair<uint64_t, uint64_t> Catalogo::contadoresActuales(const std::string& tabla, const std::string& columna) const {
    auto it = estadoColumnas_.find({tabla, columna});
    if (it == estadoColumnas_.end()) return {0, 0};
    return {it->second.contadorIgualdad, it->second.contadorRango};
}

void Catalogo::actualizarTipoIndiceYResetear(const std::string& tabla, const std::string& columna,
                                              TipoIndice nuevoTipo) {
    EstadoColumna& estado = estadoColumnas_[{tabla, columna}];
    estado.tipo = nuevoTipo;
    estado.contadorIgualdad = 0;
    estado.contadorRango = 0;
    guardarADisco();
}

std::string Catalogo::tipoIndiceATexto(TipoIndice tipo) {
    switch (tipo) {
        case TipoIndice::HASH:
            return "HASH";
        case TipoIndice::BMAS:
            return "BMAS";
        default:
            return "NINGUNO";
    }
}

TipoIndice Catalogo::textoATipoIndice(const std::string& texto) {
    if (texto == "HASH") return TipoIndice::HASH;
    if (texto == "BMAS") return TipoIndice::BMAS;
    return TipoIndice::NINGUNO;
}

void Catalogo::guardarADisco() const {
    std::ofstream salida(rutaArchivoCatalogo_, std::ios::trunc);

    for (const auto& [nombre, tabla] : tablas_) {
        salida << "TABLA," << nombre << "," << tabla.esquema.columnas.size();
        for (const Columna& columna : tabla.esquema.columnas) {
            salida << "," << columna.nombre << "," << (columna.tipo == TipoDato::ENTERO ? "ENTERO" : "TEXTO") << ","
                   << columna.longitudTexto;
        }
        salida << "\n";
    }

    for (const auto& [clave, estado] : estadoColumnas_) {
        salida << "COLUMNA," << clave.first << "," << clave.second << "," << tipoIndiceATexto(estado.tipo) << ","
               << estado.contadorIgualdad << "," << estado.contadorRango << "\n";
    }
}

void Catalogo::cargarDesdeDisco() {
    std::ifstream entrada(rutaArchivoCatalogo_);
    if (!entrada.is_open()) return;  // primera corrida: no hay catalogo previo

    std::string linea;
    while (std::getline(entrada, linea)) {
        if (linea.empty()) continue;
        std::vector<std::string> campos = dividirPorComas(linea);
        if (campos.empty()) continue;

        if (campos[0] == "TABLA") {
            if (campos.size() < 3) continue;
            try {
                size_t numColumnas = std::stoul(campos[2]);
                if (campos.size() != 3 + numColumnas * 3) continue;  // linea malformada: se ignora

                Esquema esquema;
                esquema.nombreTabla = campos[1];
                for (size_t i = 0; i < numColumnas; ++i) {
                    Columna columna;
                    columna.nombre = campos[3 + i * 3];
                    columna.tipo = (campos[3 + i * 3 + 1] == "ENTERO") ? TipoDato::ENTERO : TipoDato::TEXTO;
                    columna.longitudTexto = std::stoul(campos[3 + i * 3 + 2]);
                    esquema.columnas.push_back(columna);
                }
                registrarTablaSinPersistir(esquema);
            } catch (const std::exception&) {
                continue;  // campos numericos invalidos: se ignora la linea
            }
        } else if (campos[0] == "COLUMNA") {
            if (campos.size() != 6) continue;  // linea malformada: se ignora
            EstadoColumna estado;
            estado.tipo = textoATipoIndice(campos[3]);
            try {
                estado.contadorIgualdad = std::stoull(campos[4]);
                estado.contadorRango = std::stoull(campos[5]);
            } catch (const std::exception&) {
                continue;  // campos numericos invalidos: se ignora la linea
            }
            estadoColumnas_[{campos[1], campos[2]}] = estado;
        }
        // prefijo desconocido: se ignora (sin manejo de corrupcion elaborado)
    }
}

}  // namespace minisgbd
