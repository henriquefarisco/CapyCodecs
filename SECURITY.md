# Security Policy

CapyCodecs 0.0.2 is an early service release. Report security issues privately to the repository owner before opening public issues.

## Release gate

- `make validate` must pass before release tags.
- Image decoders must fail closed on invalid or truncated input.
- Build gates use strict C warnings and hardened compile flags.
