CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
CPPFLAGS ?=
LDFLAGS ?=
BUILD_DIR := build
SRC := src/image/image.c src/image/bmp_decode.c src/image/png_decode.c src/image/jpeg_decode.c src/image/detect.c src/image/metadata.c src/image/qoi_decode.c src/image/ico_decode.c
include src/audio/sources.mk
SRC_AUDIO := $(addprefix src/audio/,$(CAPY_AUDIO_SOURCE_NAMES))
TEST_SRC := tests/image/test_image_contracts.c tests/image/test_image_common.c tests/image/test_image_abi.c tests/image/test_image_lifecycle.c tests/image/test_bmp.c tests/image/test_png.c tests/image/test_jpeg.c tests/image/test_golden.c tests/image/test_negative.c tests/image/test_alloc_failures.c tests/image/test_inflater_failures.c tests/image/test_limits.c tests/image/test_detect.c tests/image/test_metadata.c tests/image/test_qoi.c tests/image/test_ico.c
TEST_BIN := $(BUILD_DIR)/test_image_contracts
AUDIO_TEST_SRC := tests/audio/test_audio_contracts.c
AUDIO_TEST_BIN := $(BUILD_DIR)/test_audio_contracts
OGG_SRC := src/audio/ogg_reader.c
OGG_TEST_SRC := tests/audio/test_ogg_reader.c
OGG_TEST_BIN := $(BUILD_DIR)/test_ogg_reader
VORBIS_SRC := src/audio/vorbis_headers.c src/audio/vorbis_codebook.c src/audio/vorbis_setup.c src/audio/vorbis_huffman.c src/audio/vorbis_vq.c src/audio/vorbis_floor1.c src/audio/vorbis_floor1_packet.c
VORBIS_SRC += src/audio/vorbis_residue.c
VORBIS_SRC += src/audio/vorbis_coupling.c
VORBIS_SRC += src/audio/vorbis_floor1_gain.c
VORBIS_SRC += src/audio/vorbis_mapping.c
VORBIS_SRC += src/audio/vorbis_residue_decode.c
VORBIS_SRC += src/audio/vorbis_packet.c
VORBIS_SRC += src/audio/vorbis_window.c
VORBIS_SRC += src/audio/vorbis_mdct.c
VORBIS_SRC += src/audio/vorbis_synthesis.c
VORBIS_SRC += src/audio/vorbis_audio_packet.c
VORBIS_HEADERS := $(VORBIS_SRC:.c=.h) src/audio/capy_audio.h
VORBIS_TEST_SRC := tests/audio/test_vorbis_headers.c
VORBIS_TEST_BIN := $(BUILD_DIR)/test_vorbis_headers
BOOK_TEST_BIN := $(BUILD_DIR)/test_vorbis_codebook
SETUP_TEST_BIN := $(BUILD_DIR)/test_vorbis_setup
HUFFMAN_TEST_BIN := $(BUILD_DIR)/test_vorbis_huffman
VQ_TEST_BIN := $(BUILD_DIR)/test_vorbis_vq
FLOOR1_TEST_BIN := $(BUILD_DIR)/test_vorbis_floor1
FLOOR_PACKET_TEST_BIN := $(BUILD_DIR)/test_vorbis_floor1_packet
RESIDUE_TEST_BIN := $(BUILD_DIR)/test_vorbis_residue
COUPLING_TEST_BIN := $(BUILD_DIR)/test_vorbis_coupling
FLOOR_GAIN_TEST_BIN := $(BUILD_DIR)/test_vorbis_floor1_gain
MAPPING_TEST_BIN := $(BUILD_DIR)/test_vorbis_mapping
RESIDUE_DECODE_TEST_BIN := $(BUILD_DIR)/test_vorbis_residue_decode
VORBIS_PACKET_TEST_BIN := $(BUILD_DIR)/test_vorbis_packet
VORBIS_WINDOW_TEST_BIN := $(BUILD_DIR)/test_vorbis_window
VORBIS_MDCT_TEST_BIN := $(BUILD_DIR)/test_vorbis_mdct
VORBIS_SYNTHESIS_TEST_BIN := $(BUILD_DIR)/test_vorbis_synthesis
VORBIS_AUDIO_PACKET_TEST_BIN := $(BUILD_DIR)/test_vorbis_audio_packet

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
AUDIO_PKG_NAME := org.capyos.codecs.audio-wav
AUDIO_PKG_SUMMARY := CapyCodecs portable bounded WAV PCM decoder
AUDIO_PKG_INSTALL_ROOT := /var/capypkg/$(AUDIO_PKG_NAME)
AUDIO_PKG_PROVIDES_ABI := capy-codec-audio
AUDIO_PKG_ABI_VERSION := 1
AUDIO_PKG_BIN := $(CAPY_PKG_DIR)/$(AUDIO_PKG_NAME)-$(CAPY_PKG_VERSION).bin
AUDIO_PKG_MANIFEST := $(CAPY_PKG_DIR)/$(AUDIO_PKG_NAME).manifest

.PHONY: all clean lint security test validate version-check package package-clean

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): $(SRC) $(TEST_SRC) tests/image/test_image_common.h tests/fixtures/image/golden_image_fixtures.h tests/fixtures/image/negative_image_fixtures.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/image $(SRC) $(TEST_SRC) $(LDFLAGS) -o $@
	chmod 755 $@

$(AUDIO_TEST_BIN): $(SRC_AUDIO) $(AUDIO_TEST_SRC) src/audio/capy_audio.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(SRC_AUDIO) $(AUDIO_TEST_SRC) $(LDFLAGS) -o $@
	chmod 755 $@

$(OGG_TEST_BIN): $(OGG_SRC) $(OGG_TEST_SRC) src/audio/ogg_reader.h src/audio/capy_audio.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(OGG_SRC) $(OGG_TEST_SRC) $(LDFLAGS) -o $@
	chmod 755 $@

$(VORBIS_TEST_BIN): $(VORBIS_SRC) $(VORBIS_TEST_SRC) src/audio/vorbis_headers.h src/audio/capy_audio.h src/audio/audio.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/audio.c $(VORBIS_SRC) $(VORBIS_TEST_SRC) $(LDFLAGS) -o $@
	chmod 755 $@

$(BOOK_TEST_BIN): src/audio/vorbis_codebook.c src/audio/vorbis_codebook.h tests/audio/test_vorbis_codebook.c src/audio/capy_audio.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_codebook.c tests/audio/test_vorbis_codebook.c $(LDFLAGS) -o $@
	chmod 755 $@

$(SETUP_TEST_BIN): $(VORBIS_SRC) src/audio/vorbis_setup.h src/audio/vorbis_codebook.h tests/audio/test_vorbis_setup.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(VORBIS_SRC) tests/audio/test_vorbis_setup.c $(LDFLAGS) -o $@
	chmod 755 $@

$(HUFFMAN_TEST_BIN): src/audio/vorbis_codebook.c src/audio/vorbis_codebook.h src/audio/vorbis_huffman.c src/audio/vorbis_huffman.h tests/audio/test_vorbis_huffman.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_codebook.c src/audio/vorbis_huffman.c tests/audio/test_vorbis_huffman.c $(LDFLAGS) -o $@
	chmod 755 $@

$(VQ_TEST_BIN): src/audio/vorbis_codebook.c src/audio/vorbis_codebook.h src/audio/vorbis_vq.c src/audio/vorbis_vq.h tests/audio/test_vorbis_vq.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_codebook.c src/audio/vorbis_vq.c tests/audio/test_vorbis_vq.c $(LDFLAGS) -o $@
	chmod 755 $@

$(FLOOR1_TEST_BIN): src/audio/vorbis_floor1.c src/audio/vorbis_floor1.h src/audio/capy_audio.h tests/audio/test_vorbis_floor1.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_floor1.c tests/audio/test_vorbis_floor1.c $(LDFLAGS) -o $@
	chmod 755 $@

$(VORBIS_TEST_BIN) $(SETUP_TEST_BIN): $(VORBIS_HEADERS)

$(FLOOR_PACKET_TEST_BIN): $(VORBIS_SRC) $(VORBIS_HEADERS) tests/audio/test_vorbis_floor1_packet.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(VORBIS_SRC) tests/audio/test_vorbis_floor1_packet.c $(LDFLAGS) -o $@
	chmod 755 $@

$(RESIDUE_TEST_BIN): $(VORBIS_SRC) $(VORBIS_HEADERS) tests/audio/test_vorbis_residue.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(VORBIS_SRC) tests/audio/test_vorbis_residue.c $(LDFLAGS) -o $@
	chmod 755 $@

$(COUPLING_TEST_BIN): src/audio/vorbis_coupling.c src/audio/vorbis_coupling.h src/audio/capy_audio.h tests/audio/test_vorbis_coupling.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_coupling.c tests/audio/test_vorbis_coupling.c $(LDFLAGS) -o $@
	chmod 755 $@

$(FLOOR_GAIN_TEST_BIN): src/audio/vorbis_floor1_gain.c src/audio/vorbis_floor1_gain.h src/audio/vorbis_floor1.h src/audio/capy_audio.h tests/audio/test_vorbis_floor1_gain.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_floor1_gain.c tests/audio/test_vorbis_floor1_gain.c $(LDFLAGS) -o $@
	chmod 755 $@

$(MAPPING_TEST_BIN): src/audio/vorbis_mapping.c src/audio/vorbis_mapping.h src/audio/vorbis_codebook.c src/audio/vorbis_codebook.h src/audio/capy_audio.h tests/audio/test_vorbis_mapping.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_codebook.c src/audio/vorbis_mapping.c tests/audio/test_vorbis_mapping.c $(LDFLAGS) -o $@
	chmod 755 $@

$(RESIDUE_DECODE_TEST_BIN): $(VORBIS_SRC) $(VORBIS_HEADERS) tests/audio/test_vorbis_residue_decode.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(VORBIS_SRC) tests/audio/test_vorbis_residue_decode.c $(LDFLAGS) -o $@
	chmod 755 $@

$(VORBIS_PACKET_TEST_BIN): src/audio/vorbis_packet.c src/audio/vorbis_packet.h src/audio/vorbis_codebook.c src/audio/vorbis_setup.h tests/audio/test_vorbis_packet.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_codebook.c src/audio/vorbis_packet.c tests/audio/test_vorbis_packet.c $(LDFLAGS) -o $@
	chmod 755 $@

$(VORBIS_WINDOW_TEST_BIN): src/audio/vorbis_window.c src/audio/vorbis_window.h src/audio/vorbis_packet.h tests/audio/test_vorbis_window.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_window.c tests/audio/test_vorbis_window.c $(LDFLAGS) -lm -o $@
	chmod 755 $@

$(VORBIS_MDCT_TEST_BIN): src/audio/vorbis_mdct.c src/audio/vorbis_mdct.h src/audio/vorbis_window.h tests/audio/test_vorbis_mdct.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_mdct.c tests/audio/test_vorbis_mdct.c $(LDFLAGS) -lm -o $@
	chmod 755 $@

$(VORBIS_SYNTHESIS_TEST_BIN): $(VORBIS_SRC) $(VORBIS_HEADERS) tests/audio/test_vorbis_synthesis.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(VORBIS_SRC) tests/audio/test_vorbis_synthesis.c $(LDFLAGS) -o $@
	chmod 755 $@

$(VORBIS_AUDIO_PACKET_TEST_BIN): $(VORBIS_SRC) $(VORBIS_HEADERS) tests/audio/test_vorbis_audio_packet.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(VORBIS_SRC) tests/audio/test_vorbis_audio_packet.c $(LDFLAGS) -o $@
	chmod 755 $@

test: $(TEST_BIN) $(AUDIO_TEST_BIN) $(OGG_TEST_BIN) $(VORBIS_TEST_BIN) $(BOOK_TEST_BIN) $(SETUP_TEST_BIN) $(HUFFMAN_TEST_BIN) $(VQ_TEST_BIN) $(FLOOR1_TEST_BIN) $(FLOOR_PACKET_TEST_BIN) $(RESIDUE_TEST_BIN) $(COUPLING_TEST_BIN) $(FLOOR_GAIN_TEST_BIN) $(MAPPING_TEST_BIN) $(RESIDUE_DECODE_TEST_BIN) $(VORBIS_PACKET_TEST_BIN) $(VORBIS_WINDOW_TEST_BIN) $(VORBIS_MDCT_TEST_BIN) $(VORBIS_SYNTHESIS_TEST_BIN) $(VORBIS_AUDIO_PACKET_TEST_BIN)
	$(TEST_BIN)
	$(AUDIO_TEST_BIN)
	$(OGG_TEST_BIN)
	$(VORBIS_TEST_BIN)
	$(BOOK_TEST_BIN)
	$(SETUP_TEST_BIN)
	$(HUFFMAN_TEST_BIN)
	$(VQ_TEST_BIN)
	$(FLOOR1_TEST_BIN)
	$(FLOOR_PACKET_TEST_BIN)
	$(RESIDUE_TEST_BIN)
	$(COUPLING_TEST_BIN)
	$(FLOOR_GAIN_TEST_BIN)
	$(MAPPING_TEST_BIN)
	$(RESIDUE_DECODE_TEST_BIN)
	$(VORBIS_PACKET_TEST_BIN)
	$(VORBIS_WINDOW_TEST_BIN)
	$(VORBIS_MDCT_TEST_BIN)
	$(VORBIS_SYNTHESIS_TEST_BIN)
	$(VORBIS_AUDIO_PACKET_TEST_BIN)

lint:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(SRC) $(TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only $(SRC_AUDIO) $(AUDIO_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only $(OGG_SRC) $(OGG_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only $(VORBIS_SRC) $(VORBIS_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_codebook.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_setup.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_huffman.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_vq.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_floor1.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_floor1_packet.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_residue.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_coupling.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_floor1_gain.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_mapping.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_residue_decode.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_packet.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_window.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_mdct.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_synthesis.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio -fsyntax-only tests/audio/test_vorbis_audio_packet.c
	git -c core.whitespace=cr-at-eol diff --check
	test "$$(tr -d '\r\n' < VERSION)" = "0.1.1"

security:
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fsyntax-only $(SRC) $(SRC_AUDIO) $(OGG_SRC) $(VORBIS_SRC)

version-check:
	test "$$(tr -d '\r\n' < VERSION)" = "0.1.1"
	grep -q "Version: 0.1.1" README.md

validate: lint security test version-check

# Optional independent host oracle. Requires Python 3 + installed libvorbis.
.PHONY: vorbis-reference-test
.PHONY: vorbis-benchmark
.PHONY: vorbis-pcm-reference-test
.PHONY: vorbis-decode-test
$(BUILD_DIR)/test_vorbis_decode: $(SRC_AUDIO) $(VORBIS_HEADERS) src/audio/vorbis_decode.h tests/audio/test_vorbis_decode.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(SRC_AUDIO) tests/audio/test_vorbis_decode.c $(LDFLAGS) -o $@

vorbis-decode-test: $(BUILD_DIR)/test_vorbis_decode
	python3 -B tests/audio/test_vorbis_decode_reference.py $(BUILD_DIR)/test_vorbis_decode
vorbis-benchmark: $(BUILD_DIR)/bench_vorbis_primitives

$(BUILD_DIR)/bench_vorbis_primitives: src/audio/vorbis_codebook.c src/audio/vorbis_vq.c src/audio/vorbis_floor1.c src/audio/vorbis_window.c src/audio/vorbis_mdct.c src/audio/vorbis_codebook.h src/audio/vorbis_vq.h src/audio/vorbis_floor1.h src/audio/vorbis_window.h src/audio/vorbis_mdct.h tests/audio/bench_vorbis_primitives.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio src/audio/vorbis_codebook.c src/audio/vorbis_vq.c src/audio/vorbis_floor1.c src/audio/vorbis_window.c src/audio/vorbis_mdct.c tests/audio/bench_vorbis_primitives.c $(LDFLAGS) -o $@

vorbis-reference-test: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -fPIC -Isrc/audio src/audio/audio.c $(VORBIS_SRC) -o $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_codebook_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_setup_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_huffman_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_vq_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_floor1_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_floor1_packet_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_residue_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_coupling_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_floor1_gain_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_mapping_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_residue_decode_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_window_reference.py $(BUILD_DIR)/vorbis_headers_host.so
	python3 -B tests/audio/test_vorbis_mdct_reference.py $(BUILD_DIR)/vorbis_headers_host.so

$(BUILD_DIR)/decode_vorbis_fixture: $(OGG_SRC) $(VORBIS_SRC) $(VORBIS_HEADERS) tests/audio/decode_vorbis_fixture.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc/audio $(OGG_SRC) $(VORBIS_SRC) tests/audio/decode_vorbis_fixture.c $(LDFLAGS) -o $@

vorbis-pcm-reference-test: $(BUILD_DIR)/decode_vorbis_fixture
	python3 -B tests/audio/test_vorbis_pcm_reference.py $(BUILD_DIR)/decode_vorbis_fixture

# package: build the artefact + manifest the in-tree CapyOS adapter
# consumes (see CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
package: $(CAPY_PKG_MANIFEST) $(AUDIO_PKG_MANIFEST)

$(CAPY_PKG_BIN): $(SRC) | $(BUILD_DIR)
	@mkdir -p $(CAPY_PKG_DIR)
	@tar --format=ustar --owner=0 --group=0 --numeric-owner \
	     --mtime='@0' --sort=name \
	     -cf $@ src/image docs VERSION 2>/dev/null || \
	  tar -cf $@ src/image docs VERSION
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

$(AUDIO_PKG_BIN): $(SRC_AUDIO) | $(BUILD_DIR)
	@mkdir -p $(CAPY_PKG_DIR)
	@tar --format=ustar --owner=0 --group=0 --numeric-owner \
	     --mtime='@0' --sort=name \
	     -cf $@ src/audio docs VERSION 2>/dev/null || \
	  tar -cf $@ src/audio docs VERSION
	@echo "[package] $@"

$(AUDIO_PKG_MANIFEST): $(AUDIO_PKG_BIN)
	@SHA=$$(shasum -a 256 $(AUDIO_PKG_BIN) 2>/dev/null | awk '{print $$1}' | tr 'A-F' 'a-f') ; \
	if [ -z "$$SHA" ]; then SHA=$$(sha256sum $(AUDIO_PKG_BIN) | awk '{print $$1}' | tr 'A-F' 'a-f'); fi ; \
	SIZE=$$(wc -c < $(AUDIO_PKG_BIN) | tr -d ' ') ; \
	URL="$(PUBLISH_URL_BASE)/$(AUDIO_PKG_NAME)-$(CAPY_PKG_VERSION).bin" ; \
	{ \
	  echo "name=$(AUDIO_PKG_NAME)" ; \
	  echo "version=$(CAPY_PKG_VERSION)" ; \
	  echo "summary=$(AUDIO_PKG_SUMMARY)" ; \
	  echo "payload_url=$$URL" ; \
	  echo "payload_sha256=$$SHA" ; \
	  echo "payload_size=$$SIZE" ; \
	  echo "install_root=$(AUDIO_PKG_INSTALL_ROOT)" ; \
	  echo "provides_abi=$(AUDIO_PKG_PROVIDES_ABI)" ; \
	  echo "abi_version=$(AUDIO_PKG_ABI_VERSION)" ; \
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
