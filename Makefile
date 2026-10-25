# screensaver-service Makefile
# Provides production compilation and fuzz testing infrastructure.

CC         ?= gcc
CFLAGS     ?= -O2 -g -Wall -Wextra -DVERSION=\"0.2.1\" -Isrc -Iprotocol
LDFLAGS    ?=
PKG_CONFIG ?= pkg-config

WAYLAND_FLAGS := $(shell $(PKG_CONFIG) --cflags --libs wayland-client 2>/dev/null || echo "-lwayland-client")
X11_FLAGS     := $(shell $(PKG_CONFIG) --cflags --libs x11 xrandr 2>/dev/null || echo "-lX11 -lXrandr")

SRC_DIR    := src
BUILD_DIR  := build
FUZZ_DIR   := test/fuzz
CORPUS_DIR := test/fuzz_corpus
BUILD_FUZZ := $(BUILD_DIR)/fuzz

CONTAINER_IMG ?= messense/cargo-zigbuild:latest

# Detect clang with libfuzzer capability
HAS_CLANG := $(shell which clang 2>/dev/null)

ifeq ($(HAS_CLANG),)
  FUZZ_CC      := $(CC)
  FUZZ_CFLAGS  := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Isrc
  FUZZ_LDFLAGS := -fsanitize=address,undefined
else
  FUZZ_CC      := clang
  FUZZ_CFLAGS  := -O1 -g -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Isrc -DLIBFUZZER
  FUZZ_LDFLAGS := -fsanitize=fuzzer,address,undefined
endif

FUZZ_TARGETS := $(BUILD_FUZZ)/fuzz_split_exec \
                $(BUILD_FUZZ)/fuzz_config_load \
                $(BUILD_FUZZ)/fuzz_desktop_entry \
                $(BUILD_FUZZ)/fuzz_ctl_cli

.PHONY: all clean fuzz fuzz-check fuzz-campaign fuzz-container fuzz-clean

all: $(BUILD_DIR)/screensaverd $(BUILD_DIR)/screensaverctl

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_FUZZ):
	@mkdir -p $(BUILD_FUZZ)

$(BUILD_DIR)/screensaverd: src/daemon.c src/config.c src/launch.c protocol/ext-idle-notify-v1-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(WAYLAND_FLAGS) $(X11_FLAGS) $(LDFLAGS)

$(BUILD_DIR)/screensaverctl: src/ctl.c src/config.c src/launch.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(X11_FLAGS) $(LDFLAGS)

# Fuzz target compilations
$(BUILD_FUZZ)/fuzz_split_exec: $(FUZZ_DIR)/fuzz_split_exec.c src/config.c | $(BUILD_FUZZ)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^ $(FUZZ_LDFLAGS)

$(BUILD_FUZZ)/fuzz_config_load: $(FUZZ_DIR)/fuzz_config_load.c src/config.c | $(BUILD_FUZZ)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^ $(FUZZ_LDFLAGS)

$(BUILD_FUZZ)/fuzz_desktop_entry: $(FUZZ_DIR)/fuzz_desktop_entry.c src/config.c | $(BUILD_FUZZ)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^ $(FUZZ_LDFLAGS)

$(BUILD_FUZZ)/fuzz_ctl_cli: $(FUZZ_DIR)/fuzz_ctl_cli.c src/config.c | $(BUILD_FUZZ)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^ $(FUZZ_LDFLAGS)

fuzz: $(FUZZ_TARGETS)
	@echo "All screensaver-service fuzz targets built successfully."

fuzz-check: fuzz
	@echo "Running fuzz regression corpora checks under ASan & UBSan..."
	@./$(BUILD_FUZZ)/fuzz_split_exec $(CORPUS_DIR)/split_exec/ -runs=100
	@./$(BUILD_FUZZ)/fuzz_config_load $(CORPUS_DIR)/config/ -runs=100
	@./$(BUILD_FUZZ)/fuzz_desktop_entry $(CORPUS_DIR)/desktop/ -runs=100
	@./$(BUILD_FUZZ)/fuzz_ctl_cli $(CORPUS_DIR)/ctl/ -runs=100
	@echo "All fuzz regression corpora passed cleanly with 0 errors!"

fuzz-campaign: fuzz
	@echo "Launching 60s fuzz campaigns across all targets..."
	@./$(BUILD_FUZZ)/fuzz_split_exec   -max_total_time=60 $(CORPUS_DIR)/split_exec/
	@./$(BUILD_FUZZ)/fuzz_config_load  -max_total_time=60 $(CORPUS_DIR)/config/
	@./$(BUILD_FUZZ)/fuzz_desktop_entry -max_total_time=60 $(CORPUS_DIR)/desktop/
	@./$(BUILD_FUZZ)/fuzz_ctl_cli      -max_total_time=60 $(CORPUS_DIR)/ctl/
	@echo "Fuzz campaign completed successfully."

fuzz-container:
	docker run --rm -v $(CURDIR):/workspace -w /workspace $(CONTAINER_IMG) make fuzz-check

clean:
	rm -rf $(BUILD_FUZZ) $(BUILD_DIR)/screensaverd $(BUILD_DIR)/screensaverctl

fuzz-clean:
	rm -rf $(BUILD_FUZZ)
