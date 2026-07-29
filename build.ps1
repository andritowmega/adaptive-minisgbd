# Script de build para PowerShell (alternativa a `make`, que no esta
# instalado en este entorno). Uso: .\build.ps1 [-Correr]
param(
    [switch]$Correr,
    [switch]$Benchmark
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path bin | Out-Null
New-Item -ItemType Directory -Force -Path datos | Out-Null

$fuentesComunes = @(
    "src/almacenamiento/Pagina.cpp",
    "src/almacenamiento/Registro.cpp",
    "src/almacenamiento/GestorArchivos.cpp",
    "src/buffer/GestorBuffer.cpp",
    "src/comun/CodificadorClave.cpp",
    "src/indices/HashExtensible.cpp",
    "src/indices/ArbolBMas.cpp",
    "src/catalogo/Catalogo.cpp",
    "src/decision/PoliticaDecision.cpp",
    "src/decision/GestorIndices.cpp",
    "src/consultas/Parser.cpp",
    "src/consultas/Ejecutor.cpp"
)

function Compilar($nombreTest) {
    $fuentes = @("tests/$nombreTest.cpp") + $fuentesComunes
    g++ -std=c++17 -Wall -Wextra -Iinclude -O2 $fuentes -o "bin/$nombreTest.exe"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Fallo la compilacion de $nombreTest (exit $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
    Write-Output "Compilacion OK: bin/$nombreTest.exe"
}

Compilar "test_almacenamiento_buffer"
Compilar "test_indices"
Compilar "test_motor_decision"
Compilar "test_consultas"

$fuentesBenchmark = @("benchmarks/main_benchmark.cpp", "benchmarks/GeneradorCargas.cpp") + $fuentesComunes
g++ -std=c++17 -Wall -Wextra -Iinclude -O2 $fuentesBenchmark -o "bin/main_benchmark.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Fallo la compilacion de main_benchmark (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
}
Write-Output "Compilacion OK: bin/main_benchmark.exe"

$fuentesRepl = @("main.cpp") + $fuentesComunes
g++ -std=c++17 -Wall -Wextra -Iinclude -O2 $fuentesRepl -o "bin/minisgbd.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Fallo la compilacion del REPL (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
}
Write-Output "Compilacion OK: bin/minisgbd.exe (REPL interactivo)"

if ($Correr) {
    & .\bin\test_almacenamiento_buffer.exe
    $codigo1 = $LASTEXITCODE
    & .\bin\test_indices.exe
    $codigo2 = $LASTEXITCODE
    & .\bin\test_motor_decision.exe
    $codigo3 = $LASTEXITCODE
    & .\bin\test_consultas.exe
    $codigo4 = $LASTEXITCODE
    $peor = [Math]::Max([Math]::Max($codigo1, $codigo2), [Math]::Max($codigo3, $codigo4))
    if ($peor -ne 0) { exit $peor }
}

if ($Benchmark) {
    # Tarda ~1-2 minutos: 11 proporciones x 4 estrategias x 2000 consultas.
    & .\bin\main_benchmark.exe | Tee-Object -FilePath "benchmarks/resultados_benchmark.csv"
}
