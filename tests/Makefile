# Chromatron host unit tests (Phase 1)
#
#   make                build + run all suites + coverage report + baseline check
#   make test           same as above
#   make ASAN=1         build & run with ASan/UBSan (separate build dir)
#   make cov-baseline   refresh baseline.json (run deliberately, after review)
#   make clean          remove build/ and coverage/
#
# C flags are kept in sync with targets/sim/settings.json. If you change
# one, change both: struct packing and enum widths must match the firmware.

CC      ?= gcc
PYTHON  ?= python3

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
            -I$(REPO)/src/hal/sim -I$(REPO)/src/FX4 -I$(BUILD)
COVFLAGS := -fprofile-arcs -ftest-coverage

CFLAGS     := $(SIMFLAGS) $(DEFS) $(INC) $(SANFLAGS) -MMD -MP
COVFLAGS   := -fprofile-arcs -ftest-coverage
# SUTs also need sapphiremake's generated keyvalue defines (gen_kv_defs.py)

# --- suites ---------------------------------------------------------------
# SUITE_<name>   sources under test, coverage-instrumented. Restrict to
#                src/sapphireos and src/lib_gfx: those are the paths the
#                coverage report includes.
# HAL_*          sim HAL files a suite links (file-backed, host-native)
# STUB_*         harness stubs, see stubs/README.md
# TESTDEP_*      extra prerequisites for the test source (e.g. generated
#                vectors.h)

SUITES := hash ffs_smoke

SUITE_hash      := src/sapphireos/hash.c
TESTDEP_hash    := $(BUILD)/vectors.h

SUITE_ffs_smoke := src/sapphireos/ffs_page.c \
                        src/sapphireos/ffs_block.c \
                        src/sapphireos/flash25.c \
                        src/sapphireos/memory.c
HAL_ffs_smoke   := hal_flash25.c crc.c
STUB_ffs_smoke  := stub_ffs.c

BINS :=

define suite_rules

$(eval KVDEFS_$(1) := $(shell $(PYTHON) $(TESTS)/gen_kv_defs.py $(REPO) $(SUITE_$(1)) $(SUITE_$(1):.c=.h)))
$(eval SUOBJS_$(1) := $(addprefix $(BUILD)/$(1)/obj/,$(addsuffix .o,$(SUITE_$(1):.c=))))
$(eval DEPOBJS_$(1) := $(addprefix $(BUILD)/$(1)/obj/dep/,$(addsuffix .o,$(HAL_$(1):.c=) $(STUB_$(1):.c=))) $(BUILD)/$(1)/obj/dep/unity.o $(BUILD)/$(1)/obj/test_$(1).o)
$(eval BINS += $(BUILD)/$(1)/bin/$(1))

$(BUILD)/$(1)/bin/$(1): $(SUOBJS_$(1)) $(DEPOBJS_$(1)) | $(BUILD)/$(1)/run $(BUILD)/$(1)/bin
	$(CC) $(CFLAGS) $(COVFLAGS) $(SUOBJS_$(1)) $(DEPOBJS_$(1)) -lm -o $$@

# instrumented sources under test (paths relative to repo root)
$(BUILD)/$(1)/obj/%.o: $(REPO)/%.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) $(COVFLAGS) $(KVDEFS_$(1)) -c $$< -o $$@

# harness: sim HAL files
$(BUILD)/$(1)/obj/dep/%.o: $(REPO)/src/hal/sim/%.c | $(BUILD)/$(1)/obj
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -c $$< -o $$@

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
	@touch $$@

endef
$(foreach s,$(SUITES),$(eval $(call suite_rules,$(s))))

RUNSTAMPS := $(foreach s,$(SUITES),$(BUILD)/$(s)/run/$(s).stamp)
OBJDIRS   := $(foreach s,$(SUITES),$(BUILD)/$(s)/obj)

# generated hash test vectors from the repo-root JSON files
$(BUILD)/vectors.h: $(TESTS)/gen_vectors.py $(REPO)/kv_hashes.json $(REPO)/catbus_hashes.json | $(BUILD)
	$(PYTHON) $(TESTS)/gen_vectors.py $(REPO) $@

# coverage: aggregate gcov JSON over every suite's object dir
$(COVDIR)/summary.json: $(RUNSTAMPS) $(TESTS)/coverage_report.py
	$(PYTHON) $(TESTS)/coverage_report.py \
	    $(addprefix --objdir=,$(OBJDIRS)) \
	    --root $(REPO) --out $(COVDIR)

# regression gate: runs on every make (phony), even when the report is
# up to date, so an unchanged build cannot skip the check
check: $(COVDIR)/summary.json
	$(PYTHON) $(TESTS)/cov_check.py $(COVDIR)/summary.json $(BASELINE)

# default: everything
test: check
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

.PHONY: test all clean cov-baseline check

-include $(wildcard $(addprefix $(BUILD)/,test_*/obj/*.d test_*/obj/*/*.d test_*/obj/*/*/*.d))
