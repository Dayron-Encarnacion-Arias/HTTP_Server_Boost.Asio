# Architecture

## Overview

The server uses asynchronous Boost.Asio operations and a bounded group of `io_context` workers. It does not create one operating-system thread per connection.

```text
Client
  |
  v
TCP acceptor (Server::async_accept)
  |
  v
Session (socket + buffer + timer + strand)
  |
  v
HttpRequest parser
  |
  v
Router -----> static file constrained to public/
  |          GET /, /about and other public assets
  |
  +--------> GET /api/health
  |
  v
HttpResponse::serialize
  |
  v
Session::async_write -> Client
```

## Class responsibilities

- `Server`: opens, binds and listens on the configured endpoint. It keeps one asynchronous accept pending and creates a `Session` for each accepted socket.
- `Session`: owns all per-client state. It performs incremental asynchronous reads, enforces the 16 KiB limit and ten-second timeout, invokes the parser/router, and keeps the serialized response alive until `async_write` completes.
- `HttpRequest`: parses the request line, HTTP version, case-insensitive header names, `Content-Length`, target path and optional body. HTTP/1.1 requires `Host`.
- `HttpResponse`: owns status, headers and body, then creates a valid HTTP/1.1 wire representation with `Content-Length` and `Connection: close`.
- `Router`: contains application routes and static-file policy. It does not own sockets or concurrency.
- `Logger`: serializes console writes so messages from different workers do not interleave.

## Concurrency model

`main` runs one `boost::asio::io_context` on between two and eight worker threads, based on the available hardware. Asynchronous accept/read/write operations allow those workers to serve many connections. A per-session `strand` guarantees that the read, timer and write handlers for one client do not race even though different sessions may run in parallel.

This model was selected over a detached `std::thread` for every client because it bounds resource usage and demonstrates the asynchronous execution model used by production networking systems. `std::jthread` is used only to host the fixed set of `io_context` workers.

## Request lifecycle

1. `Server` accepts a TCP connection.
2. `Session` starts a deadline timer and calls `async_read_some`.
3. Received bytes are accumulated up to 16 KiB.
4. `HttpRequest::parse` returns `incomplete`, `invalid` or `complete`.
5. A complete request is passed to `Router`.
6. The router returns an `HttpResponse` for an API route, public file, or error.
7. The session serializes and writes the response asynchronously, then closes the HTTP/1.1 connection.

## Basic security boundaries

- Only files below the configured `public` directory may be served.
- Percent-encoded paths are decoded before validation.
- `..`, backslashes, null bytes and malformed percent encodings are rejected.
- The canonical file path is checked against the canonical public root, which also prevents symlink escape.
- Requests larger than 16 KiB return `413 Payload Too Large`.
- Clients that do not complete a request within ten seconds receive `408 Request Timeout`.
- Unsupported transfer encoding and malformed or duplicate headers return `400 Bad Request`.

## Deliberate scope

This version supports one request per connection and the `GET` method. Static disk reads are synchronous but short; network operations are asynchronous. Keep-alive, chunked bodies, TLS, streaming large files and a dedicated file-I/O pool are appropriate second-version improvements.
