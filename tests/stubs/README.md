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
