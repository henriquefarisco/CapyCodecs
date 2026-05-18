# CapyCodecs validation

CapyCodecs validation is host-side and portable.

Current coverage:

- `tests/image/test_image_contracts.c`

The current image-contract test covers:

- allocator-injected free/reset behavior;
- fail-closed invalid input behavior for BMP, PNG and JPEG entry points;
- 1x1 BMP RGB decode through the portable API;
- 1x1 PNG RGB decode through a fake inflater callback.

The PNG test intentionally avoids zlib or CapyOS `tinf` wiring. Real compressed PNG fixtures belong in a later host-adapter slice.

No repository-level build harness is defined here yet. Until one exists, compile this test externally with the image decoder sources and a host C compiler.
