CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
CPPFLAGS ?=
LDFLAGS ?=
BUILD_DIR := build
SRC := src/image/image.c src/image/bmp_decode.c src/image/png_decode.c src/image/jpeg_decode.c src/image/detect.c src/image/metadata.c src/image/qoi_decode.c src/image/ico_decode.c
TEST_SRC := tests/image/test_image_contracts.c tests/image/test_image_common.c tests/image/test_image_abi.c tests/image/test_image_lifecycle.c tests/image/test_bmp.c tests/image/test_png.c tests/image/test_jpeg.c tests/image/test_golden.c tests/image/test_negative.c tests/image/test_alloc_failures.c tests/image/test_inflater_failures.c tests/image/test_limits.c tests/image/test_detect.c tests/image/test_metadata.c tests/image/test_qoi.c tests/image/test_ico.c
TEST_BIN := $(BUILD_DIR)/test_image_contracts

# capypkg packaging (Etapa 9 alpha)
CAPY_PKG_NAME := org.capyos.codecs.image-basic
CAPY_PKG_VERSION := $(shell cat VERSION)
CAPY_PKG_SUMMARY := CapyCodecs portable BMP/PNG/JPEG decoders
CAPY_PKG_INSTALL_ROOT := /var/capypkg/$(CAPY_PKG_NAME)
CAPY_PKG_PROVIDES_ABI := capy-codec-image
CAPY_PKG_ABI_VERSION := 2
CAPY_PKG_CORE_ABI_MIN := 3
CAPY_PKG_CORE_ABI_MAX := 3
CAPY_PKG_KNOWN_GOOD := 1
CAPY_PKG_DEPENDS :=
PUBLISH_URL_BASE ?= https://github.com/henriquefarisco/CapyCodecs/releases/download/v$(CAPY_PKG_VERSION)
CAPY_PKG_DIR := $(BUILD_DIR)/capypkg
CAPY_PKG_BIN := $(CAPY_PKG_DIR)/$(CAPY_PKG_NAME)-$(CAPY_PKG_VERSION).bin
CAPY_PKG_MANIFEST := $(CAPY_PKG_DIR)/$(CAPY_PKG_NAME).manifest

.PHONY: all clean lint security test validate version-check package package-clean

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): $(SRC) $(TEST_SRC) tests/image/test_image_common.h tests/fixtures/image/golden_image_fixtures.h tests/fixtures/image/negative_image_fixtures.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/image $(SRC) $(TEST_SRC) $(LDFLAGS) -o $@
	chmod 755 $@

test: $(TEST_BIN)
	$(TEST_BIN)

lint:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(SRC) $(TEST_SRC)
	git -c core.whitespace=cr-at-eol diff --check
	test "$$(tr -d '\r\n' < VERSION)" = "0.0.12"

security:
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(SRC)

version-check:
	test "$$(tr -d '\r\n' < VERSION)" = "0.0.12"
	grep -q "Version: 0.0.12" README.md

validate: lint security test version-check

# package: build the artefact + manifest the in-tree CapyOS adapter
# consumes (see CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
package: $(CAPY_PKG_MANIFEST)

$(CAPY_PKG_BIN): $(SRC) | $(BUILD_DIR)
	@mkdir -p $(CAPY_PKG_DIR)
	@tar --format=ustar --owner=0 --group=0 --numeric-owner \
	     --mtime='@0' --sort=name \
	     -cf $@ src docs VERSION 2>/dev/null || \
	  tar -cf $@ src docs VERSION
	@echo "[package] $@"

$(CAPY_PKG_MANIFEST): $(CAPY_PKG_BIN)
	@SHA=$$(shasum -a 256 $(CAPY_PKG_BIN) 2>/dev/null | awk '{print $$1}' | tr 'A-F' 'a-f') ; \
	if [ -z "$$SHA" ]; then SHA=$$(sha256sum $(CAPY_PKG_BIN) | awk '{print $$1}' | tr 'A-F' 'a-f'); fi ; \
	SIZE=$$(wc -c < $(CAPY_PKG_BIN) | tr -d ' ') ; \
	URL="$(PUBLISH_URL_BASE)/$(CAPY_PKG_NAME)-$(CAPY_PKG_VERSION).bin" ; \
	{ \
	  echo "name=$(CAPY_PKG_NAME)" ; \
	  echo "version=$(CAPY_PKG_VERSION)" ; \
	  echo "summary=$(CAPY_PKG_SUMMARY)" ; \
	  echo "payload_url=$$URL" ; \
	  echo "payload_sha256=$$SHA" ; \
	  echo "payload_size=$$SIZE" ; \
	  echo "install_root=$(CAPY_PKG_INSTALL_ROOT)" ; \
	  echo "provides_abi=$(CAPY_PKG_PROVIDES_ABI)" ; \
	  echo "abi_version=$(CAPY_PKG_ABI_VERSION)" ; \
	  echo "core_abi_min=$(CAPY_PKG_CORE_ABI_MIN)" ; \
	  echo "core_abi_max=$(CAPY_PKG_CORE_ABI_MAX)" ; \
	  echo "known_good=$(CAPY_PKG_KNOWN_GOOD)" ; \
	  echo "depends=$(CAPY_PKG_DEPENDS)" ; \
	  echo "---" ; \
	} > $@
	@echo "[package] manifest: $@"

package-clean:
	rm -rf $(CAPY_PKG_DIR)

clean:
	rm -rf $(BUILD_DIR)
