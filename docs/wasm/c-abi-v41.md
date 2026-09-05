# JR-800 WASM C ABI 41

## Purpose

The ABI in `wasm/include/jr800/wasm/api.h` is the only browser-facing
execution boundary. It keeps C++ layouts, exceptions, containers, and debugger
ownership out of JavaScript while exposing structured records to one module
Worker.

ABI 41 replaces ABI 40 without a compatibility adapter. It retains the synthetic
JR8APP and explicitly configured JR-800 session kinds and the fixed-size
three-state LCD matrix snapshot and mode-based memory watchpoints behind the
same opaque handle. It retains bounded run-to-address, metadata-driven
step-over with an explicit continuation address, return-classified step-out
with explicit resumable nesting state, and deterministic reverse lookup from a
JR8DBG source line to an execution address, query-time access filtering, and
C++-compiled conditional execution breakpoints with explicit evaluation errors
and exact JR8DBG symbol-to-address resolution plus persistent C++-compiled
expression and symbol watches, instruction-bounded disassembly, and the shared
canonical formatter for every debugger consumer, plus JR8ROM validation and
exact logical-range extraction at the C++ boundary used by the browser, plus
independent explicit CPU-internal-RAM initialization, explicit reset-state
experiment inputs for otherwise unresolved CPU state, and privacy-bounded raw-
keyboard activity transport, explicit RP5C01 oscillator-tick advancement, the
visible post-step bus-advance fault, and the bus-fault word in suspended-cycle
results and ABI 26's named, opt-in CPU-cycle ratio, now renamed for E-030's
verified nominal clock; omission still selects explicit oscillator ticks only.
It retains ABI 27's four-valued calendar alarm-terminal diagnostic, ABI 28's
explicit qualified calendar-seconds adjustment operation, and ABI 29's nominal
E-030 ratio name, ABI 30's five-valued Port 2 bit 1 timer-output diagnostic,
and ABI 31's explicit-validity word plus 64-bit experimental LCD-substitution
count, ABI 32's physical-key setter, ABI 33's fixed sixteen-entry copy of
E-187 raw LCD-indicator RAM values, and ABI 34's complete 77-position physical-
key enumeration and ABI 35's validated JR8APP loading into an already loaded
JR-800 session for host-assisted RAM-program launch. ABI 36 adds validated
native `MSAVE` WAV decoding into the same RAM-program loader and a two-word
failure record. C, JavaScript, and Worker
hosts retain fixed identities without duplicating the C++ selection/bit map.
ABI 37 adds opt-in unmapped-I/O handling and a reset-scoped diagnostic count.
The different machine and bus implementations remain internal adapters.

ABI 38 adds `jr800_machine_set_calendar_datetime` with a six-word
`jr800_calendar_datetime` (year, month, day, hour, minute, second). The 32-word
hardware configuration and other records are unchanged. The host supplies a
valid local Gregorian date in 2000-2099 and a 24-hour time. C++ initializes
all date/time digits, weekday, and the independent Mod-4 leap counter together,
selects the 24-hour time bank with Timer EN and without Alarm EN, and resets
subsecond/Clock Hold state. Alarm comparison storage and battery-backed RAM
are retained. Invalid inputs leave the RTC unchanged. CPU state, instruction
history, and memory-access history are unaffected. This is an explicit host
initialization operation, not a hardware reset or a changed `DATE$` operation.

## Program import retained from ABI 39

Both hardware program-load functions require a `uint32_t run_after_load`
(0 or 1) and an optional `jr800_program_info*` output after their previous
arguments. The WAV function keeps its required issue output before these two
new arguments. No old signature is retained.

`jr800_program_info` is 28 bytes: `uint32_t kind`, `uint32_t byte_count`,
`uint32_t name_length`, then sixteen `uint8_t name` bytes. Kind 1 is machine
code, 2 native BASIC text, and 3 native BASIC binary. Names remain JR-800 bytes.
The output is assigned only after successful loading. J8A v1 binds kind, name,
profile and body with SHA-256. The current pre-release specification is version 1;
obsolete pre-release layouts are rejected.

Machine-code load-only preserves PC. BASIC uses a detached copy of all device
and CPU state, verifies the full ROM identity and console readiness, and lets
the user's LOAD routine allocate and tokenize the verified input. Header/data
transfer services receive host-supplied blocks; no ROM byte is changed. Only a
successful, structurally verified import replaces the original state. Run mode
queues the normal RUN command before committing; ordinary execution follows
through the existing Worker run request. Debugger observers are never copied.

Statuses 26-29 mean unsupported BASIC ROM, BASIC not ready, invalid BASIC
program, and BASIC load failed. WAV issues 13/14 mean invalid BASIC data and
unexpected trailing blocks. The Worker forwards numeric status and native WAV
issue details for localized user-facing errors, and returns a `program` record
with `kind`, `byteLength`, and `nameBytes` on successful loading.

## Restart and data-only programs in ABI 41

`jr800_machine_reset` now restores the configured initial hardware state,
including RAM, devices and the loaded ROM. It retains debugger observers and
completed exported saves, and discards a partial save. This host session restart
is separate from the core's low-level reset-entry operation (E-429).

J8A v1 may preserve an MSAVE execution address outside its data segments.
Load-only accepts these data files. Load-and-run rejects them with
`JR800_STATUS_ENTRY_POINT_NOT_LOADED` (30) before modifying RAM or CPU state.
Synthetic execution and SDK linked executables still require a loaded entry.
The file format version remains 1.0 (E-430).

## Program save export in ABI 40

ABI 40 adds four operations on completed native SAVE, ASCII SAVE and MSAVE
transfers. They do not change the J8A format version, which remains 1.0.

| Operation | Result |
| --- | --- |
| `jr800_machine_get_program_saves(machine, state)` | Current capture state and completed-file count |
| `jr800_machine_get_saved_program_info(machine, index, info)` | Existing 28-byte `jr800_program_info` for one zero-based index |
| `jr800_machine_export_saved_program(machine, index, format, bytes, capacity, byte_count)` | Format 1: J8A v1; format 2: native mono 48 kHz PCM16 WAV |
| `jr800_machine_clear_program_saves(machine)` | Clear completed files, unless a transfer is in progress |

`jr800_program_saves_state` is eight bytes: `uint32_t state`, then
`uint32_t count`. State values are 0 unavailable, 1 idle, 2 recording,
3 failed transfer, and 4 completed-file list full. The maximum count is 32.
An unsupported ROM, an unloaded hardware session or a synthetic session has
state 0 and no completed files. A missing index returns `NOT_FOUND`.
Null required pointers, unsupported formats and clearing while recording return
`INVALID_ARGUMENT`. Clearing an empty or unavailable session is a no-op.

For export, a null `bytes` with zero capacity queries `byte_count`. A non-null
buffer must provide at least that many bytes; insufficient capacity returns
`BUFFER_TOO_SMALL`, sets the required count and leaves the buffer untouched.
Metadata preserves the original sixteen-byte-bounded native name. The browser
derives a safe download name separately; neither serialized format changes
the native name. Export uses the captured immutable file, not current RAM.

A separate C++ observer verifies the full ROM identity and observes E-427's
header/data output services alongside the debugger. It snapshots service
inputs and accepts blocks only after a successful service return. Native block
validation and J8A validation must both pass before a completed file is added.
It does not patch ROM, replace instructions, suppress device events or shorten
SAVE execution. Raw block validation reconstructs checksums for the common
decoder; it does not verify the checksum bytes electrically emitted by ROM.

Incomplete or failed transfers do not appear in the list. Reset discards an
incomplete transfer and retains completed files. The C handle retains files
until explicit clear or destruction; its direct ROM reload revalidates capture
support. The browser's transactional ROM load creates a new handle, so selecting
another ROM discards that browser session's list. Page reload or close also
discards it. Repeated names append separate files, up to the capacity limit.

Worker snapshots include `programSaves: {state, files}`, whose entries contain
`index`, `kind`, `byteLength` and `nameBytes`. Commands `export-saved-program`
(index, format `j8a` or `wav`) and `clear-program-saves` work between bounded
execution slices. Export transfers an ArrayBuffer to the DOM host for download.
The synthesized WAV follows E-031 framing and measured cycle means. Software
round trips are verified in E-427. E-428 records owner-reported physical loading
of one binary BASIC file after separate stereo conversion for a ZOOM H2n at
playback volume 60. Physical ASCII SAVE and MSAVE acceptance remain unverified.

## Session kinds and ownership

`jr800_machine_create` constructs a synthetic application session.
`jr800_machine_create_jr800` constructs a JR-800 session from one
`jr800_hardware_configuration`. A handle owns exactly one machine adapter and
one attached C++ debugger with fixed-capacity history and access rings.
A loaded JR-800 handle also owns the passive program-save observer.

`jr800_machine_destroy` releases the whole session. The caller must not reuse
the handle after destruction. No exported function allows a C++ exception to
cross the C boundary.

Kind-specific operations fail with `JR800_STATUS_WRONG_MACHINE_KIND`:

- JR8APP and JR8DBG loading, source/symbol lookup, and symbol watches require a
  synthetic session;
- logical-ROM byte-span loading, JR8ROM loading, JR8APP or WAV RAM-program loading,
  raw keyboard response updates, structured physical-key updates, RP5C01
  oscillator-tick advancement,
  qualified calendar-seconds adjustment, LCD panel snapshots, and raw LCD
  indicator snapshots require a
  JR-800 session;
- state, reset, step, step-over, step-out, run, run-to-address, breakpoints,
  memory watchpoints, expression watches, history, access trace, memory
  inspection, and disassembly use either session kind.

## Explicit JR-800 configuration

The 32-word `jr800_hardware_configuration` has its own ABI-version word. Every
experimental device or host input is opt-in:

- SP, X, A, and B may each receive an explicit complete reset value;
- reset CCR values may explicitly mark any subset of H/N/Z/V/C known, but may
  not override the two fixed high bits or the verified reset I bit;
- CPU internal RAM requires an explicit uniform initial byte;
- standard RAM requires an explicit initial byte;
- expansion RAM requires both standard RAM and its own explicit byte;
- the LCD adapter requires an explicit unknown-data read byte;
- the calendar adapter requires both an address source and an upper-read-bit
  rule;
- `calendar_cpu_cycle_ratio` is either
  `JR800_CALENDAR_CPU_CYCLE_RATIO_EXPLICIT_TICKS_ONLY` or the sole named
  `JR800_CALENDAR_CPU_CYCLE_RATIO_E030_NOMINAL_1_2288_MHZ` selection; a
  nonzero value requires the calendar adapter;
- Port 1 and Port 2 values are paired with known masks, and values outside the
  known mask are rejected;
- RAM standby validity is either explicitly known or absent;
- a keyboard-window value, when present, initializes every raw address from
  `$0C00` through `$0FFF` to the same byte.

`ignore_unsupported_io` is a boolean (0 or 1). When enabled, unmapped data
writes in I/O/reserved regions are discarded and unmapped data reads return
`$FF`. This is the E-418 host policy, not a hardware open-bus claim. Instruction
fetches, RAM/ROM failures and faults from attached device models remain strict.
Accepted accesses still generate bus trace/watchpoint events. Inspection uses
the same substituted value without events or counter changes.

An otherwise zero configuration with only `abi_version` set keeps SP, X, A, B,
and reset CCR bits H/N/Z/V/C unknown, keeps all RAM unknown, leaves optional LCD
and calendar adapters disconnected, and leaves external inputs unknown. PC
continues to come from the reset vector; the fixed CCR high bits and reset I bit
retain their verified semantics and are not configurable. Configured reset
fields are applied on ROM load and every machine reset. A CCR value bit outside
its known mask is rejected, and the mask may contain only `$2F` bits. It does
not invent a physical JR-800 default. CPU
internal RAM initialization is independent from standard and expansion RAM;
the configured byte is retained across CPU-device reset. These settings expose
existing bounded experiments; they do not resolve the hardware questions
described in `docs/user/compatibility.md`.

When the calendar is attached, the zero ratio leaves CPU cycles disconnected
from it. The E-030 nominal ratio converts E cycles to RP5C01 oscillator ticks at
exactly `2/75`, including during bounded suspended-cycle advancement. It is
derived from the marked nominal 4.9152 MHz external input and the documented
divide by four; it is not a claim that pin 40 was measured at exactly 1.2288
MHz. The C ABI accepts no arbitrary frequency or unnamed ratio.

## Loading and reset

`jr800_machine_load_application` accepts a serialized JR8APP, validates it on a
temporary synthetic machine, and commits it only on success. A successful load
clears debug data, breakpoints, memory watchpoints, expression and symbol
watches, history, and access records.
`jr800_machine_load_debug_info` accepts an integrity-bound JR8DBG only for the
current JR8APP. A rejected load preserves the last valid session state.

`jr800_machine_load_logical_rom` accepts exactly 32 KiB for logical addresses
`$8000-$FFFF`, then initializes the HD6301V1 state from the reset vector. A
successful load clears debug data, breakpoints, memory watchpoints, expression
and symbol watches, history, and access records. The repository and tests contain no owner
ROM; tests generate small project-authored programs inside an otherwise
synthetic 32 KiB image. This function is a fixed byte-span embedding boundary,
not a serialized file format. The browser uses it only for an explicitly
selected 32 KiB `.rom` file; extension dispatch remains in the DOM host and no
content sniffing occurs.

`jr800_machine_load_jr8rom` size-bounds and fully validates one JR8ROM v1 byte
span, then requires stored coverage of every address from `$8000` through
`$FFFF` through the shared format operation. It accepts explicit adjacent
segments, reports `JR800_STATUS_INCOMPLETE_JR8ROM` for any gap,
`JR800_STATUS_INTEGRITY_MISMATCH` for a digest mismatch, and
`JR800_STATUS_INVALID_JR8ROM` for other format failures. Only after validation
does it delegate the resulting 32 KiB span to the same machine load/reset path.
Invalid input cannot partially load the machine.

`jr800_machine_load_program` accepts J8A v1 after a JR8ROM or logical ROM
has been loaded. Machine-code segments are checked against configured RAM
before writing; BASIC follows the transaction described above.
`jr800_machine_load_native_program_wav` uses the same C++ importer after
validating and identifying MSAVE, binary SAVE, or text SAVE. It returns a
structured WAV issue on decoding failure, without changing the machine.
Neither path models the physical cassette input or native transfer timing.

The JavaScript adapter performs complete synthetic JR8APP/JR8DBG, JR8ROM, and
fixed 32 KiB logical-ROM loads on a candidate session, captures the requested
snapshot, and swaps handles only after every operation succeeds. `.j8r` and
`.rom` select separate Worker commands; neither command falls back to the
other parser. `.j8a` and `.wav` select separate RAM-program operations; the WAV
operation exposes its decoder reason to the Worker and browser status. Both use the C++ loader's
all-segment preflight on the existing paused session. A failed JavaScript or
Worker load therefore leaves the active session intact.

A shared project-authored parity fixture now covers one split JR8ROM load and
NOP step from both a Native C ABI snapshot executable and the module Worker. It
binds ABI version, profile, reset-knownness masks, memory and disassembly views,
history/access counts, fetched bytes, cycles, stop classification, and the
resulting state without using owner ROM data.

Project format readers report validation failures with C++ exceptions. The
Emscripten build enables `-fexceptions` for C++ compilation and linking so each
C ABI entry point can translate those exceptions into a status instead of
aborting the Worker.

## Execution and explicit device time

`jr800_machine_step`, `jr800_machine_step_over`, `jr800_machine_step_out`, and
`jr800_machine_run` invoke the C++ debugger directly. The bounded operations
are synchronous and accept a nonzero 32-bit instruction limit. The Worker uses
bounded calls and yields to its event loop between them, so pause remains
responsive without Emscripten pthreads.

The Worker also accepts `set-keyboard-response` and
`set-keyboard-key-state` while such a run is active. Because each C++ call is
synchronous, either update is applied only after a complete run slice and
before the next one begins. Its response records whether the run was active
and the aggregate number of completed instructions at that boundary; an idle
update reports no run count. The browser leaves the raw control enabled during
JR-800 runs and does not infer a key or matrix meaning.

`JR800_STOP_SLEEPING` remains a normal stop. ABI 40 retains
`jr800_machine_advance_suspended_cycles`. It advances devices and the CPU cycle
counter by at most the caller's nonzero limit, stops at an asserted or unknown
interrupt-request boundary, executes no CPU instruction, and creates no
debugger history or access record. Interrupt entry or masked sleep release
occurs on a later ordinary step. Its five-word result includes `bus_fault`.
Only cycles accepted by every bus-owned device contribute to `cycles_elapsed`
or the CPU cycle counter. A rejected next cycle preserves those counters and
returns its exact device-state fault.

Every completed instruction or interrupt entry asks the bus to advance by the
same E-cycle total after the architectural work commits. If that request fails,
the stop uses `JR800_FAULT_BUS_ADVANCE` and carries either
`JR800_BUS_FAULT_DEVICE_STATE_UNKNOWN` or
`JR800_BUS_FAULT_DEVICE_STATE_UNSUPPORTED`. The completed instruction remains
in history, advances the PC and CPU cycle counter, and contributes to
`instructions_executed`; it is not retried. A bus implementation must reject
the complete cycle batch without partially advancing its devices. This
post-step distinction keeps the committed CPU state observable instead of
misreporting the event as a memory access or silently losing the failure.

For a JR-800 Worker run, a sleeping stop is advanced by the request's bounded
`suspendedCycleLimit`. The Worker divides that total into at most 65,536-cycle
calls and yields between them. The run resumes only when the result reaches an
asserted or unknown request; otherwise it returns the sleeping stop at the
finite total limit. Synthetic-session sleep behavior is unchanged.
A suspended bus-advance fault instead finishes the Worker run as
`cpu-fault`/`bus-advance` and retains the five-word advance result.

`jr800_machine_advance_calendar_oscillator_ticks` advances the explicitly
configured RP5C01 model by a caller-supplied unsigned 32-bit number of 32.768
kHz oscillator ticks. Zero is a valid no-op. The call requires a JR-800 session
but does not require a ROM because it acts only on the configured device. A
disconnected calendar or a counter combination outside the modeled device
semantics reports `JR800_STATUS_UNSUPPORTED_ACCESS`; unknown divider or required
counter state reports `JR800_STATUS_UNINITIALIZED_READ`. A rejected multi-second
advance leaves both divider phase and calendar counters unchanged.

The Worker command `advance-calendar-oscillator` is idle-only, validates the
requested readable snapshot view before advancing, and returns the accepted tick
count with the resulting snapshot. That explicit operation does not advance CPU
cycles, execute instructions, create debugger history or bus trace, or alter
the optional CPU-cycle conversion phase. JavaScript configuration accepts only
`calendarCpuCycleRatio: "e030-nominal-1.2288mhz"`, together with both calendar
adapter fields. Ordinary execution then uses the shared C++ bus conversion; the
Worker adds no scheduler or host-time coupling.

The browser's explicit experimental form exposes that exact value behind a
default-off checkbox labeled as the E-030 nominal clock. The checkbox depends on
calendar attachment and changes only the next transactional JR8ROM load.

`jr800_machine_adjust_calendar_seconds` applies one caller-qualified E-353
`ADJ` action to an explicitly configured RP5C01. The C operation does not
require a ROM because it acts only on the attached device. A synthetic session
reports `JR800_STATUS_WRONG_MACHINE_KIND`; a detached calendar or unsupported
Clock Hold/counter state reports `JR800_STATUS_UNSUPPORTED_ACCESS`; required
unknown retained state reports `JR800_STATUS_UNINITIALIZED_READ`.

The Worker command `adjust-calendar-seconds` is idle-only and validates its
requested readable view before changing the calendar. A successful response is
the resulting ordinary machine snapshot. The operation advances no CPU cycle or
oscillator phase, executes no instruction, and adds no debugger history or bus
access. It represents one already-qualified action and does not model a pin
level, minimum pulse timing, repetition while held, or JR-800 board wiring.

`jr800_machine_run_to` accepts a 16-bit target and a nonzero 32-bit instruction
limit. It checks for the target before an execution breakpoint at the same
address and stops with `JR800_STOP_ADDRESS_REACHED` without fetching that
instruction. A breakpoint, watchpoint, sleep state, or CPU fault encountered
earlier remains the reported stop. Reaching the target on the final allowed
instruction takes precedence over the instruction-limit stop.

The Worker retains the target across its bounded calls, so browser run-to
remains pausable and can use an instruction total larger than one C call. The
same finite suspended-cycle policy applies if the JR-800 CPU sleeps before the
target.

`jr800_machine_source_address` resolves an exact JR8DBG source path and nonzero
line to the lowest mapped address for that line. Source paths are compared as
their exact UTF-8 byte sequences. If one line has multiple noncontiguous
mappings, selecting the lowest address makes the result independent of input
table order. The function returns `JR800_STATUS_NOT_FOUND` for a missing path,
line, or debug-info attachment and leaves the caller's output word unchanged.
Empty paths, embedded NUL bytes, line zero, and null pointers are invalid. A
JR-800 hardware session reports `JR800_STATUS_WRONG_MACHINE_KIND` because it
does not accept JR8DBG.

The Worker `run-to-source` command resolves the source location through that C++
lookup once, then reuses bounded run-to-address and its target-priority,
breakpoint, pause, and suspended-cycle behavior. JavaScript parses host input
but does not inspect or search JR8DBG data.

`jr800_machine_symbol_address` compares one nonempty symbol name as an exact,
case-sensitive byte sequence. It succeeds only when exactly one JR8DBG symbol
has that name and its kind is `address`. No match returns
`JR800_STATUS_NOT_FOUND`, multiple exact matches return
`JR800_STATUS_AMBIGUOUS_SYMBOL`, and an exact absolute symbol returns
`JR800_STATUS_SYMBOL_NOT_ADDRESS`; every failure leaves the output word
unchanged. Embedded NUL bytes and null pointers are invalid. This deliberately
does not infer source scope or reinterpret an absolute constant as an address.

The Worker `run-to-symbol` command resolves once through that C++ API and then
uses the same bounded run-to-address path. JavaScript transports the name but
does not inspect JR8DBG or choose among matches.

`jr800_machine_step_over` reads the current opcode without side effects and
uses the project-generated ISA classification. A non-call instruction uses one
ordinary step. A call executes once without being blocked by a persistent
breakpoint at its own address, then runs toward the wrapped fall-through
address. A breakpoint or watchpoint inside the called routine, a CPU fault, or
an unexpected interrupt entry remains visible. The fall-through target takes
priority over a persistent breakpoint at the same address, matching
run-to-address.

If a call cannot reach its fall-through address within one instruction slice,
an instruction-limit or sleeping stop sets `continuation_address_valid` and
`continuation_address`. The Worker consumes that address with bounded
run-to-address calls and retains it across finite suspended-cycle advancement.
Other stops and completed step-over operations clear the validity word. This
keeps instruction classification and target calculation in C++ while the
Worker only schedules resumable calls.

`jr800_machine_step_out` starts from an all-zero `jr800_step_out_state`. It
executes the current instruction without being blocked by a persistent
breakpoint at that address, then tracks generated `call`, `subroutine_return`,
and `interrupt_return` classifications. A hardware interrupt entry encountered
after the first instruction also increases the nesting depth. The operation
stops with `JR800_STOP_STEP_OUT_COMPLETE` immediately after the first return at
depth zero. That completion therefore takes priority over a persistent
breakpoint at the returned-to address, while an earlier breakpoint,
watchpoint, sleep, or fault remains visible.

Step-out does not inspect the current stack or guess where a return address is
stored. Stack locals and register saves therefore do not change its control
decision. A jump or tail call is not treated as a return; the finite instruction
limit remains the termination boundary if no classified return is executed.
An interrupt entry before the first requested instruction is reported as an
unexpected visible step and does not silently arm a step-out continuation.

The three-word in/out `jr800_step_out_state` contains `continued` and a 64-bit
low/high nesting depth. An instruction-limit or sleeping stop returns the state
needed by the next call. Other stops and successful completion clear it. A
fresh state with nonzero depth, a non-Boolean `continued` word, null pointers,
and a zero instruction limit are rejected before execution. The Worker retains
this state across 1,000-instruction slices and also returns it with a finite
instruction-limit or pause stop so a host may explicitly resume the same
operation. JavaScript never decodes an opcode or guesses a stack frame.

## Access trace filtering

The three-word `jr800_access_filter` contains an inclusive `first_address` and
`last_address` plus a `kind_mask`. Mask bits select instruction fetch, data
read, and data write independently; zero, unknown bits, reversed ranges, and
addresses outside 16 bits are invalid. `JR800_ACCESS_TRACE_ALL` selects the
complete retained trace.

`jr800_machine_access_count` and `jr800_machine_copy_accesses` now return an
explicit status and write their count only after validating the complete
filter. Copying retains the newest matching records when capacity is smaller
than the match count. The filter is evaluated over the retained ring without
changing capture, original sequence numbers, or instruction-history access
counts. A host can therefore change filters without re-executing the machine;
sequence gaps in a filtered result are intentional.

## Memory watchpoints

`jr800_machine_set_memory_watchpoint` configures one address with
`JR800_WATCHPOINT_READ`, `JR800_WATCHPOINT_WRITE`, or
`JR800_WATCHPOINT_ACCESS`. Read and write bits can be enabled or disabled
independently; access mode changes both. Instruction fetches never match a
memory watchpoint because execution breakpoints own that behavior.

A matching data access completes as part of its containing instruction before
the debugger returns `JR800_STOP_MEMORY_WATCHPOINT`. The stop record identifies
the address and sets `trigger_access_valid` with `trigger_access` equal to
`JR800_ACCESS_DATA_READ` or `JR800_ACCESS_DATA_WRITE`. If one instruction has
multiple matching data accesses, the first in the structured bus-access order
is reported. A CPU fault takes precedence over a watchpoint from the faulting
instruction. Non-watchpoint stops clear `trigger_access_valid`, so consumers
must not interpret the stored placeholder access value.

## Conditional execution breakpoints

`jr800_machine_set_execution_breakpoint` continues to enable or disable an
unconditional address breakpoint. Enabling it replaces any condition at that
address. `jr800_machine_set_conditional_execution_breakpoint` accepts a
nonempty byte span and commits a compiled condition only after the complete
expression is valid; `JR800_STATUS_INVALID_EXPRESSION` leaves the prior
breakpoint unchanged.

The expression syntax is ASCII and limited to 256 bytes, 128 nodes, and 32
levels. It provides unsigned 64-bit literals in decimal, `$`-prefixed hex, or
`0x`-prefixed hex; registers `PC`, `SP`, `X`, `A`, `B`, `CCR`, and `cycles`;
individual `H`, `I`, `N`, `Z`, `V`, and `C` flags; and non-invasive byte reads
as `mem8[address]`. Identifiers are case-insensitive. Parentheses, unary
`+ - ~ !`, arithmetic `+ - * / %`, shifts, unsigned comparisons, equality,
bitwise operations, and short-circuit `&&` and `||` are supported. Arithmetic
wraps at 64 bits. A successful nonzero result stops; zero continues execution.

`symbol("exact.name")` resolves a quoted name against the currently attached
JR8DBG every time the expression is evaluated. The name is an exact,
case-sensitive byte sequence, so it can contain assembler dots and does not
collide with case-insensitive CPU identifiers. Quoted names have no escape
syntax; a backslash, newline, or missing closing quote is invalid. Address and
absolute symbols both yield their original 16-bit value. The function itself
does not read memory; an explicit `mem8[symbol("data")]` performs a non-invasive
byte inspection.

Conditions are evaluated in C++ at an active, known-PC instruction boundary
before opcode fetch. A true condition reports the ordinary
`JR800_STOP_EXECUTION_BREAKPOINT`. An unknown referenced register or flag,
failed `mem8` inspection, division by zero, shift count of 64 or greater,
memory address outside 16 bits, missing symbol, or ambiguous exact symbol name
reports
`JR800_STOP_BREAKPOINT_CONDITION_ERROR` without executing an instruction. The
stop record's `condition_error` identifies the failure; `state_fault`,
`bus_fault`, and `condition_fault_address` add detail where applicable. `mem8` uses
side-effect-free inspection and creates no architectural access event.

Run-to-address still checks its temporary target before a persistent
conditional breakpoint at the same address. Step-over bypasses a condition at
the call site and gives its fall-through target the same priority. Step-out
bypasses the initial address once and evaluates persistent conditions at later
instruction boundaries. Worker slices therefore preserve the same behavior
without parsing or evaluating expressions in JavaScript.

## Expression watches

`jr800_machine_set_expression_watch` associates one caller-selected 32-bit ID
with a compiled expression. It uses the same bounded language and compiler as a
conditional breakpoint, but it does not affect execution. A successful call
replaces the expression already stored under that ID. Invalid expression text
returns `JR800_STATUS_INVALID_EXPRESSION` and preserves the previous compiled
watch.

`jr800_machine_evaluate_expression_watch` evaluates a registered watch against
the current machine without executing an instruction or creating a history or
access record. The six-word `jr800_expression_watch_result` carries a 64-bit
unsigned value plus the expression error, bus fault, fault address, and unknown
CPU-state part. Expression evaluation failures are successful transport calls:
the function returns `JR800_STATUS_OK` and reports the failure in the result.
An unknown ID returns `JR800_STATUS_NOT_FOUND`; an unloaded session reports its
kind-specific not-ready status. API-level failures leave the caller's result
unchanged.

`jr800_machine_clear_expression_watch` removes one ID and reports
`JR800_STATUS_NOT_FOUND` when it does not exist. Reset preserves registered
watches. A successful JR8APP, logical-ROM span, or JR8ROM load clears all watches along with
the other target-specific debugger state. The Worker owns only each expression's
display text and ordering; compilation, storage, and evaluation remain in C++.

## Symbol watches

`jr800_machine_set_symbol_watch` associates a caller-selected 32-bit ID with
exactly one JR8DBG symbol. Names are case-sensitive byte sequences. Missing
names return `JR800_STATUS_NOT_FOUND`, and duplicate local names return
`JR800_STATUS_AMBIGUOUS_SYMBOL`; either failure preserves a symbol watch already
stored under that ID. Address and absolute symbols are both valid watches.

`jr800_machine_evaluate_symbol_watch` returns the symbol's 16-bit value,
address-or-absolute kind, local-or-global binding, declared size, and optional
source-file index in a six-word `jr800_symbol_watch_result`. It does not read
emulated memory: JR8DBG contains no type, byte width, signedness, or endianness
that could justify such a read. An unknown ID returns
`JR800_STATUS_NOT_FOUND`, and API-level failures leave the caller's result
unchanged.

The record words are `value`, `binding`, `kind`, `size`,
`source_file_index_valid`, and `source_file_index`. Binding uses 1 for local
and 2 for global; kind uses 1 for address and 2 for absolute. Consumers read
the final word only when its validity word is one.

`jr800_machine_clear_symbol_watch` removes one ID and reports a missing ID.
Reset preserves registered symbol watches. Rejected JR8DBG and target loads
preserve them. A successful JR8DBG replacement clears them because their stored
metadata belongs to the previous debug image; successful JR8APP or logical-ROM
loads clear them with the other target-specific debugger state. The Worker
owns only display names and ordering.

## Knownness and fault detail

Real reset leaves several CPU fields intentionally unknown. The state record
therefore carries a register-known mask and condition-code-known mask. The five
register bits identify PC, SP, X, A, and B. A stored numeric field must not be
presented as a hardware value unless its corresponding bit is set.

The `calendar_alarm_terminal` word is one
`jr800_calendar_alarm_terminal_state`. Synthetic sessions and JR-800 sessions
without the calendar adapter report `DISCONNECTED`. An attached calendar maps
an unresolved internal drive request to `UNKNOWN`, a known inactive request to
`RELEASED`, and a known active internal pull-down request to `PULL_LOW`.
JavaScript exposes the same vocabulary as `calendarAlarmTerminal`. This word is
a side-effect-free device diagnostic, not a physical terminal voltage, a CPU
interrupt, or a port connection.

The `port2_timer_output` word is one
`jr800_port2_timer_output_state`. Synthetic sessions report `UNAVAILABLE`.
JR-800 sessions map Port 2 bit 1 input mode to `DISABLED`, output mode with an
unresolved timer latch to `UNKNOWN`, and known output levels to `LOW` or
`HIGH`. JavaScript exposes the same vocabulary as `port2TimerOutput`. This
word reports the CPU-internal DDR/timer composition only; it is not a pin
voltage, transition history, waveform, beeper route, or audio signal.

The final three state words are
`lcd_substituted_data_read_count_valid`,
`lcd_substituted_data_read_count_low`, and
`lcd_substituted_data_read_count_high`. Synthetic sessions and JR-800 sessions
without the explicit LCD experiment set all three words to zero. An attached
LCD sets the validity word to one and stores its reset-scoped 64-bit substitute
count in adjacent low/high words. JavaScript exposes `null` when invalid and a
number while safe, or a decimal string otherwise, as
`lcdSubstitutedDataReadCount` when valid. This counter reports
software substitution under the provisional adapter only; it does not validate
the physical D/I signal, chip selects, busy duration, reset wiring, controller
placement, or panel composition.

`ignored_io_access_count_valid`, `ignored_io_access_count_low`, and
`ignored_io_access_count_high` carry the reset-scoped count of discarded writes
and substituted CPU reads, including discarded reads. All three words are zero
for synthetic sessions and strict JR-800 sessions. JavaScript exposes
`ignoredIoAccessCount` as `null`, a safe number, or a decimal string using the
same rules as the LCD substitution count. A ROM load/reset clears the count.

Stop and history records expose structured bus fault, fault access kind,
unresolved CPU state part, step kind, and interrupt source. A stop additionally
uses an explicit validity word for the memory-watchpoint trigger access.
History also stores the exact fault address and the post-step knownness masks.
Consumers interpret conditional fields only when their reason or validity word
makes them relevant. In particular, `condition_error` is meaningful for
`JR800_STOP_BREAKPOINT_CONDITION_ERROR` and is `JR800_EXPRESSION_OK` otherwise.

`jr800_machine_read_memory` uses non-invasive bus inspection. It first inspects
the complete requested range into temporary storage and copies to the caller
only if every byte succeeds. Unsupported, unavailable, or unknown regions
return a specific status and leave the caller's buffer unchanged. With
`ignore_unsupported_io` enabled, unmapped I/O inspection instead returns `$FF`
without advancing the ignored-access counter or emitting a trace event.

## Disassembly

`jr800_machine_disassemble` uses the same project-controlled disassembler as
the native object tools. It first inspects the opcode, then only the remaining
bytes required by that reviewed instruction. An unknown opcode consumes one
byte and returns `supported` as zero; it is not retried under another profile.
Any failure while inspecting a required byte rejects the operation without
creating a trace record.

The six-word `jr800_disassembly` contains the address, three byte slots,
instruction length, and supported flag. Only the first `length` byte slots are
instruction bytes; every unused slot is zero. Canonical text is retrieved
through the paired size and copy functions. Unknown opcodes render as `.byte
$NN`, and HD6301 immediate-memory operands use a space after the separating
comma. ABI 40 retains this instruction-bounded byte and text contract from ABI
19.

## Provisional LCD matrix snapshot

`jr800_machine_copy_lcd_panel` copies one row-major 192-by-64 matrix from a
loaded JR-800 session whose LCD experiment was explicitly enabled. Each output
byte is `JR800_LCD_DOT_UNKNOWN`, `JR800_LCD_DOT_OFF`, or
`JR800_LCD_DOT_ON`. A synthetic session reports
`JR800_STATUS_WRONG_MACHINE_KIND`; an unloaded JR-800 reports
`JR800_STATUS_NO_ROM`; and a disconnected LCD reports
`JR800_STATUS_UNSUPPORTED_ACCESS`.

The caller supplies capacity for at least `JR800_LCD_PANEL_DOT_COUNT` bytes.
The implementation constructs the entire matrix before copying it, so invalid
arguments, insufficient capacity, unavailable state, or allocation failure
leave the destination unchanged. The operation does not execute instructions,
advance devices, or create trace records.

The matrix uses the isolated E-186 composition experiment. It remains
provisional until U-011 is closed by a controlled physical pattern. ABI 40 does
not expose a guessed active/inactive value for the E-187 raw indicator bytes;
indicator drive and display-off interaction remain unstaged.

## Provisional LCD indicator RAM snapshot

`jr800_machine_copy_lcd_indicators` copies exactly
`JR800_LCD_INDICATOR_COUNT` two-word records in the fixed `jr800_lcd_indicator`
order. `value_known` is one only when the corresponding E-187 candidate RAM
byte is known; `value` then contains that byte. An unknown entry is encoded as
two zero words. The operation has the same kind, ROM, and explicit-LCD gates as
the panel copy and leaves the caller's complete destination unchanged on every
rejected operation.

The JavaScript snapshot exposes the sixteen fixed names from `page-1` through
`battery-warning`, each mapped to a byte or `null`. These are diagnostic raw
values. A nonzero byte is not presented as evidence that a physical custom
segment is active. The battery entry remains in this complete diagnostic
transport even when an emulator UI elects not to display physical-only battery
telemetry.

## Structured physical-key input

`jr800_machine_set_keyboard_key_state` accepts one `jr800_key` value and a
boolean pressed state. ABI 40 retains all 77 E-392 physical positions. Invalid
key values, non-boolean states, null handles, and synthetic sessions are
rejected.

| C value | JavaScript name |
|---|---|
| `JR800_KEY_SHIFT` | `shift` |
| `JR800_KEY_CONTROL` | `control` |
| `JR800_KEY_MENU` | `menu` |
| `JR800_KEY_RETURN` | `return` |
| `JR800_KEY_SPACE` | `space` |
| `JR800_KEY_MAIN_1` | `main-1` |
| `JR800_KEY_LETTER_A` | `letter-a` |
| `JR800_KEY_LETTER_X` | `letter-x` |
| `JR800_KEY_KEYPAD_INSERT_RUB` | `keypad-insert-rub` |
| `JR800_KEY_KEYPAD_VERTICAL_ARROWS` | `keypad-vertical-arrows` |
| `JR800_KEY_KEYPAD_HORIZONTAL_ARROWS` | `keypad-horizontal-arrows` |
| `JR800_KEY_KEYPAD_0` through `JR800_KEY_KEYPAD_7` | `keypad-0` through `keypad-7` |
| `JR800_KEY_BREAK` | `break` |
| `JR800_KEY_HOME_CLS` | `home-cls` |
| `JR800_KEY_MAIN_0`, `JR800_KEY_MAIN_2` through `JR800_KEY_MAIN_9` | `main-0`, `main-2` through `main-9` |
| `JR800_KEY_MAIN_CARET` | `main-caret` |
| `JR800_KEY_LETTER_B` through `JR800_KEY_LETTER_W`, `JR800_KEY_LETTER_Y`, `JR800_KEY_LETTER_Z` | `letter-b` through `letter-w`, `letter-y`, `letter-z` |
| `JR800_KEY_COLON`, `JR800_KEY_SEMICOLON`, `JR800_KEY_COMMA`, `JR800_KEY_PERIOD` | `colon`, `semicolon`, `comma`, `period` |
| `JR800_KEY_PF_1` through `JR800_KEY_PF_10` | `pf-1` through `pf-10` |
| `JR800_KEY_KEYPAD_8`, `JR800_KEY_KEYPAD_9` | `keypad-8`, `keypad-9` |
| `JR800_KEY_KEYPAD_MULTIPLY`, `JR800_KEY_KEYPAD_ADD`, `JR800_KEY_KEYPAD_EQUAL` | `keypad-multiply`, `keypad-add`, `keypad-equal` |
| `JR800_KEY_KEYPAD_SUBTRACT`, `JR800_KEY_KEYPAD_DECIMAL`, `JR800_KEY_KEYPAD_DIVIDE` | `keypad-subtract`, `keypad-decimal`, `keypad-divide` |

The call changes one held state only. It does not return or inject a BASIC
character, alter a raw response byte, execute an instruction, advance time, or
create debugger history and access records. Physically verified selections use
their E-381 idle response. A key on one of the other seven E-392 selections
still requires an exact raw base response; a key-state command never makes such
a selection readable by itself. Same-selection multiple-key states remain
unknown.

JavaScript uses fixed string names for the same enumeration. The Worker
message `set-keyboard-key-state` is accepted while idle or running. A running
update is serialized between complete synchronous C++ execution slices and
reports only the aggregate instruction boundary, matching raw-response update
ordering.

## Raw keyboard activity

`jr800_machine_get_keyboard_activity` copies a four-word record containing
64-bit read-attempt and distinct-address totals from a loaded JR-800 session.
`jr800_machine_clear_keyboard_activity` starts a new diagnostic interval. Both
operations reject synthetic or unloaded sessions and neither changes external
keyboard response bytes.

CPU reads and discarded reads in `$0C00-$0FFF` contribute before response
knownness is checked. Side-effect-free memory inspection, writes, and
out-of-window accesses do not contribute. The record deliberately contains no
address, response value, ordering, or per-address frequency. The JavaScript
snapshot exposes the same two totals or `null` for a synthetic session. Before
each JR-800 Worker run, the Worker clears the interval after validating the run
request and view, so the stopped snapshot describes that run only. Ordinary
single-step snapshots remain cumulative since the last reset or explicit
clear.

## Transport layouts

Transport records use fixed-width fields. The records below contain `uint32_t`
words; C++ binds their sizes with `static_assert`. `jr800_program_info` is the
exception: three words followed by sixteen name bytes, totaling 28 bytes.

| Record | Words |
| --- | ---: |
| `jr800_hardware_configuration` | 32 |
| `jr800_machine_state` | 21 |
| `jr800_stop_info` | 24 |
| `jr800_step_out_state` | 3 |
| `jr800_history_entry` | 33 |
| `jr800_access_record` | 11 |
| `jr800_access_filter` | 3 |
| `jr800_source_location` | 5 |
| `jr800_disassembly` | 6 |
| `jr800_suspended_advance` | 5 |
| `jr800_expression_watch_result` | 6 |
| `jr800_symbol_watch_result` | 6 |
| `jr800_keyboard_activity` | 4 |
| `jr800_lcd_indicator_raw` | 2 per indicator |
| `jr800_native_program_wav_issue` | 2 |
| `jr800_program_saves_state` | 2 |

Adjacent low and high words encode 64-bit sequence, cycle, and count values.
JavaScript returns a number only while the reconstructed value is a safe
integer. Access records retain explicit current-value and previous-value
knownness; the browser renders an unknown byte as `??`.

## Versioning

`jr800_machine_abi_version` and the state record's `abi_version` word both
return 41. Any later incompatible layout or semantic change must increment the
version and update the C header, JavaScript adapter, Worker, and Native/WASM
tests together.
