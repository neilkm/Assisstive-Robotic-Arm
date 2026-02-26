MODULE_NAME := esp32_joystick
MODULE_ROOT := $(CURDIR)
LIB_DIRS := $(wildcard $(MODULE_ROOT)/template_library)

APP_SRCS := $(foreach lib,$(LIB_DIRS),$(wildcard $(lib)/src/*.c))
TEST_SRCS := $(foreach lib,$(LIB_DIRS),$(wildcard $(lib)/test/*_test.c))
INCLUDE_DIRS := $(foreach lib,$(LIB_DIRS),$(lib)/inc $(lib)/src)

include ../test_framework/module.mk
