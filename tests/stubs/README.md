# Stubs

Every stubbed symbol lives in one `stub_<area>.c` file. Each stub exists
only because the sources under test call an OS/HAL service that is *not*
under test; the rule is:

1. Prefer compiling the **real** sapphireos file over stubbing it.
2. Prefer a **sim HAL** file (`src/hal/sim/`) over a stub for hardware
   access - it is the same code the simulator runs.
3. Stubs stay minimal. If a stub grows past ~20 lines, stop and link the
   real module instead.
4. Every stub is listed below with its reason. Add to this table when you
   add a stub.

## stub_ffs.c (test_ffs_smoke)

| Symbol | Reason |
|---|---|
| `tmr_u32_get_system_time_ms` | ffs_block records timings; a fake monotonic clock is enough |
| `tmr_u32_elapsed_time_ms` | same |
| `sys_u8_get_mode` | return `SYS_MODE_NORMAL` so the ffs code paths match normal operation |
| `sys_v_set_warnings` | error reporting goes nowhere in the harness |
| `sys_v_wdt_reset` | no watchdog on host |
| `thread_t_create` | ffs_block's background free-scan thread; no scheduler in the harness. Smoke tests must not depend on the free-scan having run |
| `thread_v_active` | thread body references it; no scheduler in the harness |
| `thread_v_set_alarm` | same |
| `sys_v_set_error` | error reporting goes nowhere in the harness |
| `sos_assert` | print the expression and `abort()` - a tripped assertion must fail the suite, never be swallowed |
| `_log_v_print_P` | real logging.c drags in netmsg/ip/msgflow; routed to stderr instead |
| `fs_v_create_virtual` | mem2's "handleinfo" vfile is never read by the smoke tests; real fs.c comes in M3 |

## stub_common.c (kvdb, keyvalue, catbus_common, list, datetime, array, pixelarray)

Shared harness for the suites that link real sapphireos modules with
real system services. Unlike `stub_ffs.c` this is infrastructure, not
single-symbol stand-ins - the file-backed fs, the mem2 pool and the
threading model each need real behavior for the SUTs to run (see the
notes below the tables). It is the largest stub on purpose: the
alternative (stubbing dozens of fs/mem2/thread symbols per suite)
would be larger and less faithful.

| Symbol | Reason |
|---|---|
| `mem2_*` (`_mem2_vp_get_ptr`, `_mem2_v_free`, `_mem2_u16_get_size`, `mem2_vp_get_ptr_fast`, `mem2_b_verify_handle`) | mem2 pool: 128 slots x 256 B, 1-based handles, memset on alloc. Real mem2.c drags in the vfile/defrag machinery; list.c and array.c only need allocation semantics |
| `fs_f_open_P`, `fs_i16_read/write`, `fs_i32_get_size(_fname)`, `fs_i32_tell`, `fs_v_seek`, `fs_f_close`, `fs_v_delete` | FILE\*-backed flash-fs stand-in (8-slot table, one slot per filename). Two deliberate deviations from the flash fs: `FS_MODE_WRITE_OVERWRITE` is cached in memory and flushed on close (the firmware persist file is rewritten in place with no truncate - a real truncate would let a stale persist thread wipe the file out from under the current one), and append is emulated (seek-to-end before each write) because of a glibc 2.39 fseek+fread quirk on O_APPEND streams |
| `stub_fs_v_flush_all`, `stub_fs_v_wait_idle` | harness-only handles: force-flush cached OVERWRITE writes / wait for in-flight persist-thread writes. Not SUT symbols |
| `sys_u8_get_mode` | `SYS_MODE_NORMAL` so normal (non-safe) paths run |
| `sys_u32_get_kv_index_addr`, `sys_u32_get_fw_length` | the ESP32 persist-index build path is never taken on `__SIM__`; values are irrelevant |
| `sys_v_set_warnings`, `sys_u8_get_loader_status` | error reporting goes nowhere in the harness |
| `thread_t_create`, `thread_v_active`, `thread_v_set_alarm`, `thread_v_signal*`, `thread_b_signalled`, `thread_u16_get_signals` | no cooperative scheduler on the host: each PT thread runs in its own pthread (PT_INIT once, re-entry with a 1 ms sleep on PT_WAITING). Creation is idempotent per PT function - keyvalue's persist thread holds statics and the SUTs re-init keyvalue across tests; the firmware runs exactly one instance |
| `tmr_u32_get_system_time_ms/us`, `tmr_u32_elapsed_time_*`, `tmr_i8_compare_time(s)` | fake monotonic clock, NTP-1970 anchored, +1 s per call: datetime tests see a coherent wall clock, TMR_WAIT loops spin fast |
| `sos_assert` | print and `abort()` - a tripped assertion must fail the suite |
| `itoa` | logging.c helper the keyvalue/kvdb paths reference; libc has no portable itoa |
| `sim_sdl_v_frame` | pixel.c (sim) calls it for frame pacing; no-op on the harness |
| `cfg_i8_get` | config lookup returns -1 (not found); no cfg table in the harness |
| `gfx_vm_v_update_frame_rate` | the real definition lives in gfx_vm.c (out of scope); no-op |
