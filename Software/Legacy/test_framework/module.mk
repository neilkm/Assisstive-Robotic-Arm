SHELL := /bin/sh

MODULE_NAME ?= unknown_module
MODULE_ROOT ?= $(CURDIR)
SOFTWARE_ROOT ?= $(abspath $(MODULE_ROOT)/../..)

SRC_DIR ?= $(MODULE_ROOT)/src
TEST_DIR ?= $(MODULE_ROOT)/tests
BUILD_DIR ?= $(SOFTWARE_ROOT)/build_output/production/$(MODULE_NAME)/unit_tests

APP_SRCS ?= $(shell find "$(SRC_DIR)" -type f -name '*.c' 2>/dev/null)
TEST_SRCS ?= $(shell find "$(TEST_DIR)" -type f -name '*_test.c' 2>/dev/null)
INCLUDE_DIRS ?= $(MODULE_ROOT)/include $(SRC_DIR)

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O0 -g
LDFLAGS ?=
LDLIBS ?=

RUNNER_SRC := $(MODULE_ROOT)/../test_framework/test_main.c
INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(dir)) -I$(MODULE_ROOT)/../test_framework
TEST_BIN_NAMES := $(notdir $(patsubst %.c,%,$(TEST_SRCS)))
TEST_BINS := $(addprefix $(BUILD_DIR)/,$(TEST_BIN_NAMES))
RUST_MANIFEST ?=
RUST_MANIFESTS ?=
ifneq ($(strip $(RUST_MANIFESTS)),)
ACTIVE_RUST_MANIFESTS := $(RUST_MANIFESTS)
else ifneq ($(strip $(RUST_MANIFEST)),)
ACTIVE_RUST_MANIFESTS := $(RUST_MANIFEST)
else
ACTIVE_RUST_MANIFESTS :=
endif
RUST_TARGET_DIR ?= $(SOFTWARE_ROOT)/build_output/production/$(MODULE_NAME)/rust_target
RUNNER_SCRIPT := $(BUILD_DIR)/run_tests.sh
RUST_EXEC_LIST := $(BUILD_DIR)/rust_test_executables.txt
SUMMARY_FILE := $(BUILD_DIR)/summary.env

SHOW_BUILD_OUTPUT ?= false
FALSE_VALUES := false FALSE 0 no NO off OFF
is-false = $(filter $(FALSE_VALUES),$(strip $(1)))

ifneq ($(call is-false,$(SHOW_BUILD_OUTPUT)),)
Q := @
SHOW_BUILD_OUTPUT_ENABLED := 0
else
Q :=
SHOW_BUILD_OUTPUT_ENABLED := 1
endif

.PHONY: unit-test-build unit-test unit-test-run-only unit-test-clean

unit-test-build:
	@status=0; found=0; \
	mkdir -p "$(BUILD_DIR)"; \
	if [ -n "$(strip $(TEST_SRCS))" ]; then \
		$(MAKE) --no-print-directory -s $(TEST_BINS) SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)" || status=$$?; \
		found=1; \
	fi; \
	if [ -n "$(strip $(ACTIVE_RUST_MANIFESTS))" ]; then \
		: > "$(RUST_EXEC_LIST)"; \
		for manifest in $(ACTIVE_RUST_MANIFESTS); do \
			tmp_log="$(BUILD_DIR)/cargo_test_no_run.$$(basename "$$manifest").log"; \
			if [ "$(SHOW_BUILD_OUTPUT_ENABLED)" = "1" ]; then \
				echo "[$(MODULE_NAME)] building Rust tests from $$manifest"; \
			fi; \
			CARGO_TARGET_DIR="$(RUST_TARGET_DIR)" cargo test --manifest-path "$$manifest" --no-run > "$$tmp_log" 2>&1; \
			rust_status=$$?; \
			if [ "$(SHOW_BUILD_OUTPUT_ENABLED)" = "1" ] || [ $$rust_status -ne 0 ]; then \
				cat "$$tmp_log"; \
			fi; \
			if [ $$rust_status -ne 0 ]; then \
				status=$$rust_status; \
			fi; \
			sed -n 's/^  Executable .* (\(.*\))$$/\1/p' "$$tmp_log" >> "$(RUST_EXEC_LIST)"; \
			found=1; \
		done; \
	else \
		: > "$(RUST_EXEC_LIST)"; \
	fi; \
	printf '%s\n' '#!/bin/sh' 'set +e' > "$(RUNNER_SCRIPT)"; \
	printf '%s\n' "MODULE_TOTAL=0" "MODULE_PASSED=0" >> "$(RUNNER_SCRIPT)"; \
	printf '%s\n' "SUMMARY_FILE=\"$(SUMMARY_FILE)\"" >> "$(RUNNER_SCRIPT)"; \
	if [ -n "$(strip $(TEST_BINS))" ]; then \
		for test_bin in $(TEST_BINS); do \
			printf '%s\n' "echo \"[$(MODULE_NAME)] running $$test_bin\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "output=\"\$$("$$test_bin" 2>&1)\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "rc=\$$?" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "printf '%s\n' \"\$$output\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "ok_count=\$$(printf '%s\n' \"\$$output\" | grep -c '\\[OK \\]')" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "fail_count=\$$(printf '%s\n' \"\$$output\" | grep -c '\\[FAIL\\]')" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "if [ \"\$$rc\" -ne 0 ] && [ \"\$$fail_count\" -eq 0 ]; then fail_count=1; fi" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "MODULE_TOTAL=\$$((MODULE_TOTAL + ok_count + fail_count))" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "MODULE_PASSED=\$$((MODULE_PASSED + ok_count))" >> "$(RUNNER_SCRIPT)"; \
		done; \
	fi; \
	if [ -s "$(RUST_EXEC_LIST)" ]; then \
		while IFS= read -r rust_exe; do \
			printf '%s\n' "echo \"[$(MODULE_NAME)] running $$rust_exe\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "cases_file=\"$(BUILD_DIR)/.rust_cases.\$$(basename \"$$rust_exe\")\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "\"$$rust_exe\" --list 2>/dev/null | sed -n 's/^\\(.*\\): test$$/\\1/p' > \"\$$cases_file\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "passed_count=0" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "failed_count=0" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "while IFS= read -r case_name; do" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "  [ -z \"\$$case_name\" ] && continue" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "  printf '  [RUN] Case: [%s]\n' \"\$$case_name\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "  \"$$rust_exe\" --exact \"\$$case_name\" >/dev/null 2>&1" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "  if [ \"\$$?\" -eq 0 ]; then" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "    passed_count=\$$((passed_count + 1))" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "    printf '  \033[32m[OK ]\033[0m Case: [%s]\n' \"\$$case_name\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "  else" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "    failed_count=\$$((failed_count + 1))" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "    printf '  \033[31m[FAIL]\033[0m Case: [%s]\n' \"\$$case_name\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "  fi" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "done < \"\$$cases_file\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "rm -f \"\$$cases_file\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "[ -z \"\$$passed_count\" ] && passed_count=0" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "[ -z \"\$$failed_count\" ] && failed_count=0" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "printf 'Ran %s test(s)\n' \"\$$((passed_count + failed_count))\"" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "MODULE_TOTAL=\$$((MODULE_TOTAL + passed_count + failed_count))" >> "$(RUNNER_SCRIPT)"; \
			printf '%s\n' "MODULE_PASSED=\$$((MODULE_PASSED + passed_count))" >> "$(RUNNER_SCRIPT)"; \
		done < "$(RUST_EXEC_LIST)"; \
	fi; \
	if [ $$found -eq 0 ]; then \
		printf '%s\n' "echo \"[$(MODULE_NAME)] No compiled unit-test executables available.\"" >> "$(RUNNER_SCRIPT)"; \
		printf '%s\n' "MODULE_TOTAL=0" "MODULE_PASSED=0" >> "$(RUNNER_SCRIPT)"; \
	fi; \
	printf '%s\n' "printf 'TOTAL_TESTS=%s\n' \"\$$MODULE_TOTAL\" > \"\$$SUMMARY_FILE\"" >> "$(RUNNER_SCRIPT)"; \
	printf '%s\n' "printf 'PASSED_TESTS=%s\n' \"\$$MODULE_PASSED\" >> \"\$$SUMMARY_FILE\"" >> "$(RUNNER_SCRIPT)"; \
	printf '%s\n' "if [ \"\$$MODULE_PASSED\" -eq \"\$$MODULE_TOTAL\" ]; then exit 0; else exit 1; fi" >> "$(RUNNER_SCRIPT)"; \
	chmod +x "$(RUNNER_SCRIPT)"; \
	if [ $$found -eq 0 ]; then \
		echo "[$(MODULE_NAME)] No unit tests found at $(TEST_DIR)."; \
	fi; \
	exit $$status

unit-test: unit-test-build
	@$(MAKE) --no-print-directory unit-test-run-only SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)"

unit-test-run-only:
	@status=0; \
	if [ ! -x "$(RUNNER_SCRIPT)" ]; then \
		echo "[$(MODULE_NAME)] Missing $(RUNNER_SCRIPT). Build first with unit-test-build."; \
		exit 1; \
	fi; \
	"$(RUNNER_SCRIPT)" || status=$$?; \
	exit $$status

unit-test-clean:
	@rm -rf "$(BUILD_DIR)"
	@rm -rf "$(RUST_TARGET_DIR)"
	@echo "[$(MODULE_NAME)] removed $(BUILD_DIR)"
	@echo "[$(MODULE_NAME)] removed $(RUST_TARGET_DIR)"

$(BUILD_DIR):
	@mkdir -p "$@"

define build-test-rule
$(BUILD_DIR)/$(notdir $(patsubst %.c,%,$(1))): $(1) $(APP_SRCS) $(RUNNER_SRC) | $(BUILD_DIR)
	$$(Q)$$(CC) $$(CFLAGS) $$(INCLUDE_FLAGS) $$(APP_SRCS) $(1) $$(RUNNER_SRC) -o $$@ $$(LDFLAGS) $$(LDLIBS)
endef

$(foreach test_src,$(TEST_SRCS),$(eval $(call build-test-rule,$(test_src))))
