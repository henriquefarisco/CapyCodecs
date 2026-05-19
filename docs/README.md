# CapyCodecs documentation

CapyCodecs owns portable media codec cores for CapyOS and related projects.

## CapyOS reference version

Pinned for this release: `0.8.0-alpha.240+20260519`. Update this section together with `docs/10-contracts/compatibility.md` whenever the CapyOS core version, ABI or canonical manifest format changes.

Cross-repo authoritative references:

- `CapyOS/docs/reference/integration/compatibility-matrix.md`
- `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`
- `CapyOS/docs/operations/manual-module-deploy-runbook.md`

## Documentation map

Read the documentation in numeric order when onboarding or planning larger changes.

### 00 Overview

- [Project boundary](00-overview/project-boundary.md)
- [Migration status](00-overview/migration-status.md)

### 10 Contracts

- [Compatibility and integration contract](10-contracts/compatibility.md)
- [Image ABI contract](10-contracts/image-abi.md)
- [Dependency boundaries](10-contracts/dependency-boundaries.md)

### 20 Validation

- [Validation](20-validation/validation.md)
- [Test strategy](20-validation/test-strategy.md)
- [Fuzzing and security](20-validation/fuzzing-and-security.md)

### 30 Roadmap

- [CapyCodecs development plan](30-roadmap/codecs-development-plan.md)
- [Delivery sequence](30-roadmap/delivery-sequence.md)
- [Status tracking](30-roadmap/status-tracking.md)

### 40 Implementation

- [Image codec implementation guide](40-implementation/image-codecs.md)
- [Audio codec implementation guide](40-implementation/audio-codecs.md)
- [Video codec implementation guide](40-implementation/video-codecs.md)
- [Container codec implementation guide](40-implementation/container-codecs.md)
- [Release engineering](40-implementation/release-engineering.md)

## Core rule

CapyCodecs decodes encoded media bytes into pixels, samples, frames or packets. CapyOS owns IO, sandboxing, rendering, audio output, installation and rollback.

## Current checkpoint

The current image ABI hardening checkpoint covers:

- modular host-side image contract tests;
- golden BMP, PNG and JPEG baseline fixtures;
- fail-closed negative fixtures for BMP, PNG and JPEG;
- allocator failure matrix coverage for current golden fixtures;
- PNG inflater failure coverage;
- public dimension-limit rejection for BMP, PNG and JPEG.

## Current compatibility stubs

The legacy paths below remain available as compatibility stubs:

- [Legacy compatibility path](compatibility.md)
- [Legacy validation path](validation.md)
