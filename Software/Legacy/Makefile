SHELL := /bin/sh

FRAMEWORK_DIR := test_framework
ALL_MODULES := nvidia_jetson esp32_joystick stm_nucleo

RUN_TESTS ?= true
BUILD_ESP ?= true
BUILD_NUCLEO ?= true
BUILD_NVIDIA ?= true
SHOW_BUILD_OUTPUT ?= false

FALSE_VALUES := false FALSE 0 no NO off OFF
is-false = $(filter $(FALSE_VALUES),$(strip $(1)))
SOFTWARE_ROOT := $(abspath ..)
AGGREGATE_RUNNER := $(SOFTWARE_ROOT)/build_output/production/run_all_tests.sh

REQUESTED_MODULES :=
ifeq ($(call is-false,$(BUILD_NVIDIA)),)
REQUESTED_MODULES += nvidia_jetson
endif
ifeq ($(call is-false,$(BUILD_ESP)),)
REQUESTED_MODULES += esp32_joystick
endif
ifeq ($(call is-false,$(BUILD_NUCLEO)),)
REQUESTED_MODULES += stm_nucleo
endif

ifdef MODULE
SELECTED_MODULES := $(filter $(MODULE),$(REQUESTED_MODULES))
ifeq ($(strip $(SELECTED_MODULES)),)
$(error Unknown or disabled module '$(MODULE)'. Available and enabled modules: $(REQUESTED_MODULES))
endif
else
SELECTED_MODULES := $(REQUESTED_MODULES)
endif

TESTABLE_MODULES := $(foreach m,$(SELECTED_MODULES),$(if $(wildcard $(m)/Makefile),$(m)))
NON_TESTABLE_MODULES := $(filter-out $(TESTABLE_MODULES),$(SELECTED_MODULES))

.PHONY: help modules build-tests test test-no-build clean-tests refresh-runner

help:
	@echo "Central Production test entrypoints"
	@echo ""
	@echo "  make modules                  List production modules"
	@echo "  make build-tests              Build all unit tests"
	@echo "  make test                     Build and run all unit tests"
	@echo "  make test-no-build            Run previously built test executables only"
	@echo "  make clean-tests              Remove all module unit-test artifacts"
	@echo ""
	@echo "Optional filters:"
	@echo "  make test MODULE=stm_nucleo   Run one module"
	@echo ""
	@echo "Boolean flags (default true):"
	@echo "  RUN_TESTS=true|false"
	@echo "  BUILD_ESP=true|false"
	@echo "  BUILD_NUCLEO=true|false"
	@echo "  BUILD_NVIDIA=true|false"
	@echo ""
	@echo "Build output visibility (default false):"
	@echo "  SHOW_BUILD_OUTPUT=true|false"

modules:
	@printf '%s\n' $(SELECTED_MODULES)

build-tests:
	$(call run-module-target,unit-test-build,build tests)
	@$(MAKE) --no-print-directory refresh-runner

test:
ifneq ($(call is-false,$(RUN_TESTS)),)
	$(call run-module-target,unit-test-build,build tests)
else
	$(call run-module-target,unit-test,run tests)
endif
	@$(MAKE) --no-print-directory refresh-runner

test-no-build:
	$(call run-module-target,unit-test-run-only,run tests without rebuild)
	@$(MAKE) --no-print-directory refresh-runner

clean-tests:
	$(call run-module-target,unit-test-clean,clean tests)
	@rm -f "$(AGGREGATE_RUNNER)"

refresh-runner:
	@mkdir -p "$(dir $(AGGREGATE_RUNNER))"; \
	printf '%s\n' '#!/bin/sh' 'set +e' 'total_tests=0' 'total_passed=0' 'modules_ran=0' 'overall_status=0' > "$(AGGREGATE_RUNNER)"; \
	if [ -z "$(TESTABLE_MODULES)" ]; then \
		printf '%s\n' 'echo "No enabled test modules found."' >> "$(AGGREGATE_RUNNER)"; \
	else \
		for module in $(TESTABLE_MODULES); do \
			runner="$(SOFTWARE_ROOT)/build_output/production/$$module/unit_tests/run_tests.sh"; \
			summary="$(SOFTWARE_ROOT)/build_output/production/$$module/unit_tests/summary.env"; \
			printf '%s\n' "if [ -x \"$$runner\" ]; then" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  echo \"==> $$module\"" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  modules_ran=\$$((modules_ran + 1))" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  \"$$runner\" || overall_status=\$$?" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  if [ -f \"$$summary\" ]; then" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "    module_total=\$$(sed -n 's/^TOTAL_TESTS=//p' \"$$summary\")" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "    module_passed=\$$(sed -n 's/^PASSED_TESTS=//p' \"$$summary\")" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "    [ -z \"\$$module_total\" ] && module_total=0" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "    [ -z \"\$$module_passed\" ] && module_passed=0" >> "$(AGGREGATE_RUNNER)"; \
				printf '%s\n' '    total_tests=$$((total_tests + module_total))' >> "$(AGGREGATE_RUNNER)"; \
				printf '%s\n' '    total_passed=$$((total_passed + module_passed))' >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  fi" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "else" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  echo \"==> $$module\"" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  echo \"[$$module] Missing $$runner. Build first.\"" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "  overall_status=1" >> "$(AGGREGATE_RUNNER)"; \
			printf '%s\n' "fi" >> "$(AGGREGATE_RUNNER)"; \
		done; \
	fi; \
	printf '%s\n' 'printf "[Ran %d tests from %d modules. %d/%d Passed.]\n" "$$total_tests" "$$modules_ran" "$$total_passed" "$$total_tests"' >> "$(AGGREGATE_RUNNER)"; \
	printf '%s\n' 'exit "$$overall_status"' >> "$(AGGREGATE_RUNNER)"; \
	chmod +x "$(AGGREGATE_RUNNER)"

define run-module-target
	@status=0; \
	if [ -n "$(NON_TESTABLE_MODULES)" ]; then \
		echo "Skipping modules without Makefile: $(NON_TESTABLE_MODULES)"; \
	fi; \
	if [ -z "$(TESTABLE_MODULES)" ]; then \
		echo "No testable modules found."; \
		exit 0; \
	fi; \
		for module in $(TESTABLE_MODULES); do \
			echo "==> $$module: $(2)"; \
			$(MAKE) -C $$module $(1) SHOW_BUILD_OUTPUT="$(SHOW_BUILD_OUTPUT)" || status=$$?; \
		done; \
	if [ $$status -ne 0 ]; then \
		echo "One or more modules failed while attempting to $(2)."; \
		exit $$status; \
	fi
endef
