# HTTP Server C++

Servidor HTTP/1.1 asíncrono de portafolio construido en C++20 con Boost.Asio. El proyecto parte de un socket TCP, implementa su propio parser HTTP, routing, archivos estáticos, manejo de errores y concurrencia acotada sin utilizar un framework HTTP.

## Features

- Servidor TCP configurable con `--host` y `--port`.
- `async_accept`, `async_read_some` y `async_write` con Boost.Asio.
- Sesión independiente por cliente, administrada mediante RAII y `std::shared_ptr`.
- Grupo de 2 a 8 workers para un único `io_context`; no crea un hilo por conexión.
- Parser de request line, versión, headers, `Content-Length` y body.
- Respuestas `200`, `400`, `404`, `405`, `408`, `413` y `500`.
- Routing separado de la capa de red.
- Archivos estáticos y MIME types para HTML, texto, JSON, CSS y JavaScript.
- Logger sincronizado y cierre controlado con `Ctrl+C`.
- Timeout de diez segundos y límite de request de 16 KiB.
- Protección contra directory traversal, incluidos paths codificados y symlinks.
- Interfaz web responsive con paleta roja y negra.
- 15 pruebas automatizadas con GoogleTest y CTest.

## Technologies

- C++20
- Boost.Asio / Boost.System
- CMake
- vcpkg manifest mode
- MSVC / Visual Studio Community 2026
- GoogleTest

## Architecture

```text
Client
   ↓
TCP Socket
   ↓
Boost.Asio / Server
   ↓
Session
   ↓
HTTP Parser
   ↓
Router
   ↓
HTTP Response
   ↓
Client
```

`Server` mantiene un `async_accept` pendiente. Cada conexión pertenece a una `Session` con socket, buffer, timer y strand propios. Cuando el parser obtiene una request completa, el `Router` decide entre API, archivo estático o error. La explicación detallada está en [docs/architecture.md](docs/architecture.md).

## Project Structure

```text
HTTP_Servidor_Boost.Asio/
├── include/             # Interfaces .hpp orientadas a objetos
├── src/                 # Implementaciones del servidor
├── public/              # HTML, CSS y JavaScript servidos al cliente
├── tests/               # Pruebas GoogleTest
├── docs/                # Decisiones de arquitectura
├── Main.cpp             # Configuración y arranque
├── CMakeLists.txt       # Build reproducible y CTest
├── vcpkg.json           # Dependencias declarativas
└── HTTP_Servidor_Boost.Asio.vcxproj
```

## Requirements

- Windows 10/11
- Visual Studio Community 2026 con **Desktop development with C++**
- CMake 3.24 o posterior
- vcpkg con integración habilitada
- Git

No se configuran rutas manuales a Boost. `vcpkg.json` instala únicamente Boost.Asio, Boost.System y GoogleTest.

## Build

Desde PowerShell, entra en la carpeta que contiene este README y configura vcpkg mediante su toolchain:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Debug
```

Para Release:

```powershell
cmake --build build --config Release
```

También puedes abrir `HTTP_Servidor_Boost.Asio.slnx` desde la carpeta superior y compilar `Debug | x64` o `Release | x64`. El `.vcxproj` utiliza el mismo manifiesto vcpkg.

## Run

Ejecuta desde esta carpeta para que `public/` sea detectado automáticamente:

```powershell
./build/Debug/http-server.exe --host 0.0.0.0 --port 8080
```

El ejecutable creado por la solución Visual Studio se inicia así:

```powershell
./x64/Debug/HTTP_Servidor_Boost.Asio.exe --host 0.0.0.0 --port 8080
```

Puede indicarse otro directorio público con `--public-root PATH`. Usa `--help` para ver las opciones.

## Available Routes

| Method | Route | Response |
|---|---|---|
| GET | `/` | `public/index.html` |
| GET | `/about` | `public/about.html` |
| GET | `/api/health` | JSON `{"status":"ok"}` |
| GET | `/index.html`, `/styles.css`, etc. | Static file under `public/` |

Los demás métodos devuelven `405 Method Not Allowed`; un recurso inexistente devuelve la página `404` personalizada.

## Testing

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Las pruebas cubren requests válidas e inválidas, body por `Content-Length`, responses, rutas, 404, métodos, MIME types y traversal normal/codificado.

## Example Requests

```powershell
curl.exe http://localhost:8080/
curl.exe http://localhost:8080/about
curl.exe http://localhost:8080/api/health
curl.exe http://localhost:8080/styles.css
```

## Error Handling

Los errores de un cliente no detienen el proceso. La sesión maneja EOF, connection reset, socket errors, requests incompletas o malformadas, timeout, payload excesivo, ruta inválida y fallos al leer archivos. Los errores del acceptor se registran y el ciclo de aceptación continúa.

## Future Improvements

- HTTP `POST`, `PUT` y `DELETE`
- Keep-Alive y HTTP pipelining
- Chunked transfer encoding
- Streaming de archivos grandes y pool dedicado de file I/O
- HTTPS/TLS
- WebSocket
- Parser HTTP más completo
- Archivo de configuración
- Métricas y tracing
- Rate limiting
- Pruebas de carga y fuzzing del parser
