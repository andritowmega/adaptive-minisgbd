#include "buffer/GestorBuffer.h"

#include <stdexcept>

namespace minisgbd {

GestorBuffer::GestorBuffer(size_t capacidadMarcos) : capacidadMarcos_(capacidadMarcos) {
    marcos_.reserve(capacidadMarcos_);
}

Pagina& GestorBuffer::fijarPagina(GestorArchivos& archivo, uint32_t numeroPagina) {
    IdPaginaGlobal id{&archivo, numeroPagina};
    auto encontrado = tablaPaginas_.find(id);
    if (encontrado != tablaPaginas_.end()) {
        size_t indice = encontrado->second;
        marcos_[indice].contadorPines++;
        moverAlFrenteLRU(indice);
        contadorAciertos_++;
        return marcos_[indice].pagina;
    }

    contadorFallos_++;
    size_t indice = obtenerMarcoLibre();
    archivo.leerPagina(numeroPagina, marcos_[indice].pagina);
    marcos_[indice].archivo = &archivo;
    marcos_[indice].idPagina = numeroPagina;
    marcos_[indice].ocupado = true;
    marcos_[indice].sucio = false;
    marcos_[indice].contadorPines = 1;

    tablaPaginas_[id] = indice;
    moverAlFrenteLRU(indice);
    return marcos_[indice].pagina;
}

void GestorBuffer::liberarPagina(GestorArchivos& archivo, uint32_t numeroPagina, bool sucia) {
    IdPaginaGlobal id{&archivo, numeroPagina};
    auto encontrado = tablaPaginas_.find(id);
    if (encontrado == tablaPaginas_.end()) {
        throw std::logic_error("GestorBuffer::liberarPagina: la pagina no esta fijada");
    }
    Marco& marco = marcos_[encontrado->second];
    if (marco.contadorPines == 0) {
        throw std::logic_error("GestorBuffer::liberarPagina: contador de pines ya en cero");
    }
    marco.contadorPines--;
    if (sucia) marco.sucio = true;
}

uint32_t GestorBuffer::asignarPaginaNueva(GestorArchivos& archivo) {
    uint32_t numeroPagina = archivo.asignarNuevaPagina();

    size_t indice = obtenerMarcoLibre();
    marcos_[indice].pagina.inicializar(numeroPagina);
    marcos_[indice].archivo = &archivo;
    marcos_[indice].idPagina = numeroPagina;
    marcos_[indice].ocupado = true;
    marcos_[indice].sucio = true;  // recien creada: aun no persistida con su contenido final
    marcos_[indice].contadorPines = 1;

    tablaPaginas_[IdPaginaGlobal{&archivo, numeroPagina}] = indice;
    moverAlFrenteLRU(indice);
    return numeroPagina;
}

void GestorBuffer::vaciarTodo() {
    for (Marco& marco : marcos_) {
        if (marco.ocupado && marco.sucio) {
            marco.archivo->escribirPagina(marco.idPagina, marco.pagina);
        }
        marco.ocupado = false;
        marco.sucio = false;
        marco.contadorPines = 0;
        marco.archivo = nullptr;
    }
    tablaPaginas_.clear();
    listaLRU_.clear();
    posicionesLRU_.clear();
}

void GestorBuffer::cerrarArchivo(GestorArchivos& archivo) {
    for (size_t indice = 0; indice < marcos_.size(); ++indice) {
        Marco& marco = marcos_[indice];
        if (!marco.ocupado || marco.archivo != &archivo) continue;
        if (marco.contadorPines != 0) {
            throw std::logic_error("GestorBuffer::cerrarArchivo: hay paginas de este archivo todavia fijadas");
        }
        if (marco.sucio) {
            archivo.escribirPagina(marco.idPagina, marco.pagina);
        }
        tablaPaginas_.erase(IdPaginaGlobal{marco.archivo, marco.idPagina});
        quitarDeLRU(indice);
        marco.ocupado = false;
        marco.sucio = false;
        marco.archivo = nullptr;
    }
}

double GestorBuffer::tasaAciertos() const {
    size_t total = contadorAciertos_ + contadorFallos_;
    return total == 0 ? 0.0 : static_cast<double>(contadorAciertos_) / static_cast<double>(total);
}

size_t GestorBuffer::obtenerMarcoLibre() {
    if (marcos_.size() < capacidadMarcos_) {
        marcos_.push_back(Marco{});
        return marcos_.size() - 1;
    }

    // Un marco puede existir en marcos_ (ya alcanzamos la capacidad) pero
    // estar libre sin figurar en la lista LRU — por ejemplo justo después de
    // vaciarTodo(), que limpia la lista LRU pero no encoge marcos_.
    for (size_t indice = 0; indice < marcos_.size(); ++indice) {
        if (!marcos_[indice].ocupado) return indice;
    }

    // Recorre la lista LRU desde el menos usado (final) buscando un marco
    // sin pines activos.
    for (auto it = listaLRU_.rbegin(); it != listaLRU_.rend(); ++it) {
        size_t indice = *it;
        if (marcos_[indice].contadorPines == 0) {
            if (marcos_[indice].sucio) {
                marcos_[indice].archivo->escribirPagina(marcos_[indice].idPagina, marcos_[indice].pagina);
            }
            tablaPaginas_.erase(IdPaginaGlobal{marcos_[indice].archivo, marcos_[indice].idPagina});
            quitarDeLRU(indice);
            return indice;
        }
    }

    throw std::runtime_error("GestorBuffer: buffer pool lleno, todos los marcos estan fijados");
}

void GestorBuffer::moverAlFrenteLRU(size_t indiceMarco) {
    quitarDeLRU(indiceMarco);
    listaLRU_.push_front(indiceMarco);
    posicionesLRU_[indiceMarco] = listaLRU_.begin();
}

void GestorBuffer::quitarDeLRU(size_t indiceMarco) {
    auto it = posicionesLRU_.find(indiceMarco);
    if (it != posicionesLRU_.end()) {
        listaLRU_.erase(it->second);
        posicionesLRU_.erase(it);
    }
}

}  // namespace minisgbd
