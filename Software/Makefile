SHELL := /bin/sh

RUN_TESTS ?= true
BUILD_ESP ?= true
BUILD_NUCLEO ?= true
BUILD_NVIDIA ?= true
SHOW_BUILD_OUTPUT ?= false

.PHONY: help modules build-tests test test-no-build clean-tests

help:
	@echo "Software test entrypoints"
	@echo ""
	@echo "  make test"
	@echo "  make test-no-build"
	@echo "  make build-tests"
	@echo "  make clean-tests"
	@echo "  make modules"
	@echo ""
	@echo "Flags (default true):"
	@echo "  RUN_TESTS=true|false"
	@echo "  BUILD_ESP=true|false"
	@echo "  BUILD_NUCLEO=true|false"
	@echo "  BUILD_NVIDIA=true|false"
	@echo ""
	@echo "Build output visibility (default false):"
	@echo "  SHOW_BUILD_OUTPUT=true|false"

modules:
	@$(MAKE) -C Production modules \
		BUILD_ESP="$(BUILD_ESP)" \
		BUILD_NUCLEO="$(BUILD_NUCLEO)" \
		BUILD_NVIDIA="$(BUILD_NVIDIA)" \
		SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)"

build-tests:
	@$(MAKE) -C Production build-tests \
		BUILD_ESP="$(BUILD_ESP)" \
		BUILD_NUCLEO="$(BUILD_NUCLEO)" \
		BUILD_NVIDIA="$(BUILD_NVIDIA)" \
		SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)"

test:
	@$(MAKE) -C Production test \
		RUN_TESTS="$(RUN_TESTS)" \
		BUILD_ESP="$(BUILD_ESP)" \
		BUILD_NUCLEO="$(BUILD_NUCLEO)" \
		BUILD_NVIDIA="$(BUILD_NVIDIA)" \
		SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)"

test-no-build:
	@$(MAKE) -C Production test-no-build \
		BUILD_ESP="$(BUILD_ESP)" \
		BUILD_NUCLEO="$(BUILD_NUCLEO)" \
		BUILD_NVIDIA="$(BUILD_NVIDIA)" \
		SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)"

clean-tests:
	@$(MAKE) -C Production clean-tests \
		BUILD_ESP="$(BUILD_ESP)" \
		BUILD_NUCLEO="$(BUILD_NUCLEO)" \
		BUILD_NVIDIA="$(BUILD_NVIDIA)" \
		SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)"
