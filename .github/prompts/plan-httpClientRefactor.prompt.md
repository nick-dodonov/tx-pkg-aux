# Plan: Refactor Beast HTTP Client Implementation

Refactoring the current Beast-based HTTP client to separate concerns, eliminate duplication between TCP/SSL paths, enable connection pooling, expand beyond GET-only operations, and provide better configuration options while maintaining the clean coroutine-based async API.

## Steps

1. **Extract connection layer** - Create Http/Connection abstraction separating TCP/SSL establishment from HTTP protocol, eliminating duplication in AsioLiteClient.cpp between TCP and SSL code paths.

2. **Implement connection pooling** - Add `ConnectionPool` class managing reusable connections per-host with keep-alive support, replacing the current per-request `BeastRequestContext` throwaway pattern.

3. **Expand HTTP interface** - Extend ILiteClient with `Request()` method accepting HTTP method, headers, body, and configuration options beyond the current GET-only limitation.

4. **Add configuration types** - Create `RequestOptions` and `ClientConfig` structures for headers, timeouts, SSL verification settings (fixing hardcoded `verify_none` in SslConnection.cpp), and response customization.

5. **Decouple from Beast types** - Replace Beast-specific TcpConnection and SslConnection with protocol-agnostic `IConnection` interface enabling future backend alternatives.

6. **Refactor response handling** - Add streaming support and header access to `Response` struct, replacing the current string-only body approach that loads entire responses in memory.

## Further Considerations

1. **Connection lifetime management** - Should pooled connections use shared_ptr or unique_ptr ownership? Recommend shared_ptr with weak_ptr in pool for automatic cleanup.

2. **Backward compatibility** - Keep existing `Get(url, callback)` method as convenience wrapper over new `Request()` API, or introduce versioned namespace?

3. **Testing strategy** - No tests found in codebase. Should we add unit tests for new components (connection pool, parsers) and integration tests with mock servers before refactoring?
