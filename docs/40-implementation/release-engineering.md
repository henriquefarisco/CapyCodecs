# Release engineering

CapyCodecs releases should be consumable as safe, optional CapyOS components.

## Version layers

Track versions separately:

- package version;
- ABI version;
- component manifest format version;
- fixture/corpus version where needed.

Package versions can follow SemVer, while ABI versions should describe compatibility of public contracts.

## Release metadata

Each release entry should include:

- component id;
- tag;
- artifact name;
- sha256;
- provided ABI versions;
- required CapyOS ABI versions;
- dependencies;
- permissions;
- resource-limit defaults;
- rollback metadata;
- known optional features.

## Component IDs

Recommended component IDs:

- `capy-codec-image`;
- `capy-codec-audio`;
- `capy-codec-container`;
- `capy-codec-video`;
- optional codec-specific IDs for large formats, such as `capy-codec-avif` or `capy-codec-av1`.

## Compatibility index

The compatibility index should let CapyOS decide whether a component can be installed or activated.

It should answer:

- which ABI is provided;
- which CapyOS ABI is required;
- which optional dependencies are needed;
- which permissions are requested;
- which artifact hash is expected;
- how rollback should proceed.

## Signature path

Early alpha releases may use tag and hash validation only.

When the CapyOS trust-store design is active, releases should add:

- signed manifests;
- trusted signing keys;
- signature verification policy;
- revocation strategy;
- emergency security release process.

## CI release gates

A release should eventually require:

- strict compile validation;
- hardened compile validation;
- host tests;
- sanitizer tests;
- fuzz smoke tests;
- dependency-boundary checks;
- version metadata checks;
- documentation updates for ABI/resource-limit changes.

## Rollback expectations

CapyOS owns rollback, but CapyCodecs must provide enough metadata to support it:

- previous compatible version;
- ABI compatibility classification;
- migration notes;
- known incompatibilities;
- release artifact hash.

## ABI break policy

Breaking changes require a major compatibility decision.

Examples of ABI breaks:

- changing public struct layout incompatibly;
- changing existing function signatures;
- changing pixel/sample/frame ownership rules;
- silently changing channel order or alpha policy;
- removing an existing decoder entry point.
