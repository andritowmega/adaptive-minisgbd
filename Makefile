CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -O2

SRC_COMUNES = src/almacenamiento/Pagina.cpp src/almacenamiento/Registro.cpp src/almacenamiento/GestorArchivos.cpp \
              src/buffer/GestorBuffer.cpp src/comun/CodificadorClave.cpp \
              src/indices/HashExtensible.cpp src/indices/ArbolBMas.cpp \
              src/catalogo/Catalogo.cpp src/decision/PoliticaDecision.cpp src/decision/GestorIndices.cpp \
              src/consultas/Parser.cpp src/consultas/Ejecutor.cpp

.PHONY: all test bench clean

all: test bin/minisgbd.exe

test: bin/test_almacenamiento_buffer.exe bin/test_indices.exe bin/test_motor_decision.exe bin/test_consultas.exe
	./bin/test_almacenamiento_buffer.exe
	./bin/test_indices.exe
	./bin/test_motor_decision.exe
	./bin/test_consultas.exe

bench: bin/main_benchmark.exe
	./bin/main_benchmark.exe | tee benchmarks/resultados_benchmark.csv

bin/test_almacenamiento_buffer.exe: tests/test_almacenamiento_buffer.cpp $(SRC_COMUNES)
	mkdir -p bin datos
	$(CXX) $(CXXFLAGS) $^ -o $@

bin/test_indices.exe: tests/test_indices.cpp $(SRC_COMUNES)
	mkdir -p bin datos
	$(CXX) $(CXXFLAGS) $^ -o $@

bin/test_motor_decision.exe: tests/test_motor_decision.cpp $(SRC_COMUNES)
	mkdir -p bin datos
	$(CXX) $(CXXFLAGS) $^ -o $@

bin/test_consultas.exe: tests/test_consultas.cpp $(SRC_COMUNES)
	mkdir -p bin datos
	$(CXX) $(CXXFLAGS) $^ -o $@

bin/main_benchmark.exe: benchmarks/main_benchmark.cpp benchmarks/GeneradorCargas.cpp $(SRC_COMUNES)
	mkdir -p bin datos
	$(CXX) $(CXXFLAGS) $^ -o $@

bin/minisgbd.exe: main.cpp $(SRC_COMUNES)
	mkdir -p bin datos
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf bin/*.exe bin/*.o datos/*.dat datos/*.idx datos/*.txt
