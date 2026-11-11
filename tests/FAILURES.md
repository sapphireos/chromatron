# SUT bugs found by the test harness

Log-don't-fix: these are bugs in the code *under test*, not in the
harness. Each entry records the symptom, the root cause, the impact,
and where the test pins the current (broken) behavior. Fixing the SUT
means changing the pinned assertions back to the intended behavior.

## 1. `pixelarray_add` append path drops the new array

- **File**: `src/lib_gfx/pixelarray.c`, `pixelarray_add()`
- **Symptom**: when no free zero slot is available, `pixelarray_add()`
  appends the new array to the backing array (`array_add`) but never
  calls `pixelarray_deref()`, so the cached `pixarray_ptr` /
  `pixarray_count` statics never see the new element. Every store/load
  op (`pixelarray_pstore_val*`, `pixelarray_load`, vector ops, ...)
  goes through the cached view (`_pixelarray_lookup_ptr`) and fails
  with -1 until something calls `pixelarray_deref()`. The public
  `pixelarray_lookup()` is unaffected: it reads the live array via
  `array_get_data`/`array_get_count`. (The free-slot path is also
  unaffected; it writes into the already-cached array.)
- **Root cause**: the append path ends with `array_add(&pixel_array,
  pixarray);` and falls off the function, unlike
  `pixelarray_create()` which ends with `pixelarray_deref()`. The
  cached count is always short by one; the cached pointer is stale as
  well whenever the append grows the backing array (realloc).
- **Impact**: the FX4 bytecode path `LOAD_PIXELARRAY`/add
  (`src/FX4/bytecode.c:3391`) runs `pixelarray_deinit()` on VM teardown
  (`bytecode.c:2337`); a re-created VM whose pixel arrays are *added*
  (not created) past the initial slot cannot find them.
- **Proposed fix**: append `pixelarray_deref();` after `array_add(...)`
  in `pixelarray_add()`.
- **Pinned in**: `tests/test_pixelarray.c` `test_pixelarray_2d` (fills the
  cached slot table with two creates so the add must append, asserts
  live lookup 0 / cached `pixelarray_lookup_ptr` -1 / pstore -1, then
  forces `pixelarray_deref()` so the 2D addressing logic is still
  exercised).

## 2. `util_u16_linear_interp` wraps on descending ramps

- **File**: `src/sapphireos/util.c`, `util_u16_linear_interp()`
- **Symptom**: `util_u16_linear_interp(50, 0, 100, 100, 0)` returns
  32818 instead of 50. Any call with `y0 > y1` is wrong.
- **Root cause**: `uint16_t y_diff = y1 - y0;` underflows when
  descending (100 to 0 becomes 65436), so the ramp runs 654.36x too
  steep: `100 + 50 * 65436 / 100 = 32818`.
- **Impact**: callers that interpolate a decreasing curve. In the
  firmware the palette interpolator (`src/lib_gfx/gfx_lib.c:2952-2964`)
  hits this whenever a palette channel (hue/sat/val) decreases between
  adjacent stops; the battery SOC ramp (`src/lib_battery/fuel_gauge.c`)
  ramps up and is unaffected.
- **Proposed fix**: compute the difference in `int32_t` (or
  `int16_t`) and let the result clamp to the `uint16_t` return.
- **Pinned in**: `tests/test_util.c` `test_util_linear_interp`
  (asserts the deterministic wrapped value 32818; becomes 50 once
  fixed).

## 3. `kv_v_init` orphans the hash index on re-init

- **File**: `src/sapphireos/keyvalue.c`, `kv_v_init()`
- **Symptom**: a second `kv_v_init()` (same process) mallocs a new
  `ram_index` and drops the pointer to the first one; it is never
  freed. ASan/LeakSanitizer reports the orphaned allocation (84 bytes
  with 14 fixed parameters).
- **Root cause**: `ram_index = malloc(len)` overwrites the static
  pointer with no `free()` of the previous table.
- **Impact**: none in the firmware - `kv_v_init()` runs exactly once
  per boot. It only manifests when a host test re-inits keyvalue
  mid-process.
- **Proposed fix**: `free(ram_index)` before the re-`malloc` (guard the
  first-init case with a NULL check).
- **Worked around in**: `tests/test_keyvalue.c` and `tests/test_kvdb.c`
  (one `kv_v_init()` per process; the "fresh boot" file removals happen
  *before* the first init, since the persist pass never rewrites the
  file header - a file deleted and re-created by a later init is
  missing its magic header).
