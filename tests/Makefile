# Chromatron host unit tests (Phase 1)
#
#   make                build + run all suites (incl. FX4) + coverage + gate
#   make test           same as above
#   make fx4            build & run just the FX4 language test suite
#   make ASAN=1         build & run unit suites with ASan/UBSan (separate
#                       build dir; FX4 runs with its own default flags)
#   make cov-baseline   refresh baseline.json (run deliberately, after review)
#   make clean          remove build/ and coverage/
#
# C flags are kept in sync with targets/sim/settings.json. If you change
# one, change both: struct packing and enum widths must match the firmware.

CC      ?= gcc
PYTHON  ?= python3
# gcov must match the compiler that produced the instrumentation
GCVR    ?= $(shell $(CC) -print-prog-name=gcov)

.DEFAULT_GOAL := test

TESTS   := $(CURDIR)
REPO    := $(abspath $(CURDIR)/..)
COVDIR  := $(TESTS)/coverage
BASELINE := $(TESTS)/baseline.json

ASAN ?= 0
ifeq ($(ASAN),1)
BUILD    := $(TESTS)/build_asan
SANFLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
else
BUILD    := $(TESTS)/build
SANFLAGS :=
endif

# --- kept in sync with targets/sim/settings.json ---
SIMFLAGS := -funsigned-char -fpack-struct -fshort-enums -ffunction-sections \
            -fno-strict-aliasing -Wall -Wundef -std=gnu99 -O0 -gdwarf-2 \
            -Wno-uninitialized -Wno-unused-but-set-variable \
            -Wno-sign-compare -Wno-packed-not-aligned \
            -Wno-address-of-packed-member -Wno-int-to-pointer-cast \
            -Wno-pointer-to-int-cast -Wno-format
DEFS     := -D__SIM__ -DF_CPU=16000000UL
INC      := -I$(REPO)/src/sapphireos -I$(REPO)/src/lib_gfx \
            -I$(REPO)/src/hal/sim -I$(REPO)/src/FX4 -I$(BUILD) \
            -I$(REPO)/src/lib_chromatron -I$(REPO)/src/lib_battery
COVFLAGS := -fprofile-arcs -ftest-coverage

CFLAGS     := $(SIMFLAGS) $(DEFS) $(INC) $(SANFLAGS) -MMD -MP
# SUTs also need sapphiremake's generated keyvalue defines (gen_kv_defs.py)

# --- suites ---------------------------------------------------------------
# SUITE_<name>   sources under test, coverage-instrumented. Restrict to
#                src/sapphireos and src/lib_gfx: those are the paths the
#                coverage report includes.
# DEP_<name>     real sources the suite links but does not instrument
#                (closure fill-in: gfx_lib, sim hal_pixel, ...)
# HAL_*          sim HAL files a suite links (file-backed, host-native)
# STUB_*         harness stubs, see stubs/README.md
# LDFLAGS_*      per-suite link flags (e.g. the kv section script)
# TESTDEP_*      extra prerequisites for the test source (e.g. generated
#                vectors.h)

SUITES := hash ffs_smoke kvdb keyvalue catbus_types catbus_common list \
          util datetime hsv trig array pixelarray

SUITE_hash      := src/sapphireos/hash.c
TESTDEP_hash    := $(BUILD)/vectors.h

SUITE_ffs_smoke := src/sapphireos/ffs_page.c \
                        src/sapphireos/ffs_block.c \
                        src/sapphireos/flash25.c \
                        src/sapphireos/memory.c
HAL_ffs_smoke   := hal_flash25.c crc.c
STUB_ffs_smoke  := stub_ffs.c

# --- kv family -------------------------------------------------------------
# kvdb.c and keyvalue.c call into each other, so each suite instruments
# only its own module and links the other as an uninstrumented DEP.  The
# test file registers a .kv_meta section (keyvalue.c walks it with
# pointer arithmetic, hence kv_sections.ld).

KV_COMMON_DEP   := src/sapphireos/list.c \
                   src/sapphireos/hash.c \
                   src/sapphireos/catbus_types.c \
                   src/sapphireos/catbus_common.c

SUITE_kvdb      := src/sapphireos/kvdb.c
DEP_kvdb        := src/sapphireos/keyvalue.c $(KV_COMMON_DEP)
STUB_kvdb       := stub_common.c
LDFLAGS_kvdb    := -T $(TESTS)/kv_sections.ld

SUITE_keyvalue  := src/sapphireos/keyvalue.c
DEP_keyvalue    := src/sapphireos/kvdb.c $(KV_COMMON_DEP)
STUB_keyvalue   := stub_common.c
LDFLAGS_keyvalue := -T $(TESTS)/kv_sections.ld

SUITE_catbus_types := src/sapphireos/catbus_types.c

SUITE_catbus_common := src/sapphireos/catbus_common.c
DEP_catbus_common   := src/sapphireos/catbus_types.c

SUITE_list      := src/sapphireos/list.c
STUB_list       := stub_common.c

# --- math / gfx family -------------------------------------------------------

SUITE_util      := src/sapphireos/util.c

SUITE_datetime  := src/sapphireos/datetime.c
STUB_datetime   := stub_common.c

SUITE_hsv       := src/lib_gfx/hsv_to_rgb.c

SUITE_trig      := src/lib_gfx/trig.c

SUITE_array     := src/lib_gfx/array.c
STUB_array      := stub_common.c

# pixelarray pulls in the rest of the gfx closure (uninstrumented)
SUITE_pixelarray := src/lib_gfx/pixelarray.c
DEP_pixelarray   := src/lib_gfx/array.c \
                    src/sapphireos/hash.c \
                    src/sapphireos/util.c \
                    src/lib_gfx/hsv_to_rgb.c \
                    src/lib_gfx/trig.c \
                    src/lib_gfx/gfx_lib.c \
                    src/lib_gfx/pixel_mapper.c \
                    src/lib_chromatron/pixel.c \
                    src/sapphireos/random.c \
                    src/hal/sim/hal_pixel.c
STUB_pixelarray  := stub_common.c
LDFLAGS_pixelarray := -T $(TESTS)/kv_sections.ld

# keyvalue.c walks the .kv_meta section with pointer arithmetic:
# kv_start is a 1-element marker array and the real entries are placed
# directly behind it by the linker (same layout as the firmware image).
# UBSan's array-bounds check (ASAN build) flags the walk, so it is
# disabled for the suites that link keyvalue.c.
SANEXEMPT_kvdb     := -fno-sanitize=bounds
SANEXEMPT_keyvalue := -fno-sanitize=bounds

BINS :=

define suite_rules

$(eval KVDEFS_$(1) := $(shell $(PYTHON) $(TESTS)/gen_kv_defs.py $(REPO) $(SUITE_$(1)) $(DEP_$(1)) $(SUITE_$(1):.c=.h)))
$(eval SUOBJS_$(1) := $(addprefix $(BUILD)/$(1)/obj/,$(addsuffix .o,$(SUITE_$(1):.c=))))
$(eval DEPOBJS_$(1) := $(addprefix $(BUILD)/$(1)/obj/dep/,$(addsuffix .o,$(DEP_$(1):.c=) $(HAL_$(1):.c=) $(STUB_$(1):.c=))) $(BUILD)/$(1)/obj/dep/unity.o $(BUILD)/$(1)/obj/test_$(1).o)
$(eval BINS += $(BUILD)/$(1)/bin/$(1))

$(BUILD)/$(1)/bin/$(1): $(SUOBJS_$(1)) $(DEPOBJS_$(1)) | $(BUILD)/$(1)/run $(BUILD)/$(1)/bin
	$(CC) $(CFLAGS) $(COVFLAGS) $(SUOBJS_$(1)) $(DEPOBJS_$(1)) -lm $(LDFLAGS_$(1)) -o $$@

# instrumented sources under test (paths relative to repo root)
$(BUILD)/$(1)/obj/%.o: $(REPO)/%.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) $(COVFLAGS) $(SANEXEMPT_$(1)) $(KVDEFS_$(1)) -c $$< -o $$@

# harness: sim HAL files
$(BUILD)/$(1)/obj/dep/%.o: $(REPO)/src/hal/sim/%.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -c $$< -o $$@

# harness: other real sources in the suite's closure (linked, uninstrumented)
$(BUILD)/$(1)/obj/dep/%.o: $(REPO)/%.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) $(SANEXEMPT_$(1)) $(KVDEFS_$(1)) -c $$< -o $$@

# harness: stubs
$(BUILD)/$(1)/obj/dep/%.o: $(TESTS)/stubs/%.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -c $$< -o $$@

# harness: vendored Unity (src/FX4)
$(BUILD)/$(1)/obj/dep/unity.o: $(REPO)/src/FX4/unity.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -c $$< -o $$@

# harness: the test source
$(BUILD)/$(1)/obj/test_$(1).o: $(TESTS)/test_$(1).c $(TESTDEP_$(1)) | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -c $$< -o $$@

# run (each suite gets its own working dir; the sim flash HAL writes
# sim_flash.bin into the cwd)
$(BUILD)/$(1)/run/$(1).stamp: $(BUILD)/$(1)/bin/$(1) | $(BUILD)/$(1)/run
	cd $$(@D) && { ../bin/$(1) > stdout.log 2>&1 || { \
	    echo "==== SUITE $(1) FAILED ===="; cat stdout.log; exit 1; }; }
	@awk '/Tests/ && /Failures/ {print "-- $(1): " $$$$0}' $$(@D)/stdout.log
	@touch $$@

endef
$(foreach s,$(SUITES),$(eval $(call suite_rules,$(s))))

RUNSTAMPS := $(foreach s,$(SUITES),$(BUILD)/$(s)/run/$(s).stamp)
OBJDIRS   := $(foreach s,$(SUITES),$(BUILD)/$(s)/obj)

# --- FX4 language test suite (delegated) ----------------------------------
# src/FX4 has its own Makefile and 80+ .fx4 regression tests, run with
# `./fx4 tests -r` (compiles every test and executes the VM; non-zero exit
# on failure). SCAN=1 skips their -fanalyzer pass (very slow). COV=1 adds
# the gcov flags so the suite is part of the coverage report (unity, the
# harness, is excluded by the report). It is cleaned and rebuilt on every
# run: their Makefile does not track flag changes, so a stale mix of
# instrumented/uninstrumented objects would corrupt the gcda data.

FX4_SRCS := $(wildcard $(REPO)/src/FX4/*.c $(REPO)/src/FX4/*.h \
                  $(REPO)/src/FX4/tests/*.fx4)

$(BUILD)/fx4/run/fx4.stamp: $(FX4_SRCS) | $(BUILD)/fx4/run
	$(MAKE) -C $(REPO)/src/FX4 clean
	$(MAKE) -C $(REPO)/src/FX4 fx4 SCAN=1 COV=1
	cd $(REPO)/src/FX4 && { ./fx4 tests -r > $(BUILD)/fx4/run/stdout.log 2>&1 || { \
	    echo "==== FX4 SUITE FAILED ===="; cat $(BUILD)/fx4/run/stdout.log; exit 1; }; }
	@awk '/Compiled [0-9]+ files/ {print "-- fx4: " $$0}' $(BUILD)/fx4/run/stdout.log
	@touch $@

fx4: $(BUILD)/fx4/run/fx4.stamp

# generated hash test vectors from the repo-root JSON files
$(BUILD)/vectors.h: $(TESTS)/gen_vectors.py $(REPO)/kv_hashes.json $(REPO)/catbus_hashes.json | $(BUILD)
	$(PYTHON) $(TESTS)/gen_vectors.py $(REPO) $@

# coverage: aggregate gcov JSON over every suite's object dir (FX4's
# objects live in src/FX4 itself)
$(COVDIR)/summary.json: $(RUNSTAMPS) $(BUILD)/fx4/run/fx4.stamp $(TESTS)/coverage_report.py
	$(PYTHON) $(TESTS)/coverage_report.py \
	    $(addprefix --objdir=,$(OBJDIRS) $(REPO)/src/FX4) \
	    --gcov $(GCVR) --root $(REPO) --out $(COVDIR)

# regression gate: runs on every make (phony), even when the report is
# up to date, so an unchanged build cannot skip the check
check: $(COVDIR)/summary.json
	$(PYTHON) $(TESTS)/cov_check.py $(COVDIR)/summary.json $(BASELINE)

# default: unit suites + FX4 suite + coverage report + baseline gate
test: fx4 check
all: test

# deliberate action: record the current coverage as the new baseline
cov-baseline: $(COVDIR)/summary.json
	cp $(COVDIR)/summary.json $(BASELINE)
	@echo "baseline updated: $(BASELINE)"

$(BUILD):
	mkdir -p $@

# per-suite directories
$(BUILD)/%/obj:
	mkdir -p $@

$(BUILD)/%/run:
	mkdir -p $@

$(BUILD)/%/bin:
	mkdir -p $@

clean:
	rm -rf $(TESTS)/build $(TESTS)/build_asan $(COVDIR)
	rm -f $(REPO)/src/FX4/*.gcno $(REPO)/src/FX4/*.gcda

.PHONY: test all clean cov-baseline check fx4

-include $(wildcard $(addprefix $(BUILD)/,test_*/obj/*.d test_*/obj/*/*.d test_*/obj/*/*/*.d test_*/obj/*/*/*/*.d))
