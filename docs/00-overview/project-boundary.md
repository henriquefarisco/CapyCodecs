# Project boundary

CapyCodecs owns portable codec cores that transform encoded media bytes into decoded pixels, samples or frames for CapyOS and related hosts.

## CapyCodecs responsibilities

- **Decode logic:** Parse and decode media payloads into well-defined output buffers.
- **Deterministic errors:** Return stable error codes for invalid input, unsupported features and resource failures.
- **Resource limits:** Enforce explicit maximum dimensions, output sizes, temporary memory and stream complexity.
- **Injected ownership:** Allocate and release memory only through host-provided allocators.
- **Portable adapters:** Receive compression, IO or platform services through callbacks instead of direct runtime dependencies.
- **Validation assets:** Maintain golden, corrupt, truncated and fuzz fixtures for each supported codec.

## CapyOS responsibilities

- **IO:** Own file, network, package and VFS access.
- **Kernel allocation wrappers:** Adapt CapyOS allocation policy into CapyCodecs allocator callbacks.
- **Rendering and audio:** Consume decoded pixels, samples and frames through CapyOS backends.
- **Sandbox policy:** Decide process isolation, permissions and execution boundaries.
- **Installation:** Stage, activate, update and roll back codec components.
- **User reporting:** Convert codec errors into UI or system diagnostics.

## Boundary invariant

CapyCodecs must never include CapyOS kernel/runtime headers, call CapyOS allocators directly, perform platform IO, or depend on renderer/audio internals.

## Adapter pattern

Hosts integrate CapyCodecs through narrow adapters:

1. The host reads encoded bytes.
2. The host prepares allocator and service callbacks.
3. CapyCodecs validates and decodes the memory buffer.
4. The host consumes the decoded output.
5. The host frees output through the allocator captured by the output object.

This keeps codecs testable on a normal host compiler and safe to consume as optional CapyOS components.
