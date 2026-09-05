# Synthetic execution model v1

## Scope

This document defines the first executable CPU and debugger slice. It is a
deterministic software contract for SDK tests, not a claim about the complete
JR-800 memory map or package-pin timing.

Execution always selects an exact documented CPU profile. The
`jr800_unresolved` profile is a fail-closed choice for an unconfigured machine
and refuses every instruction. Owner evidence E-002 and E-028 establish
`hd6301v1` as the exact JR-800 instruction profile; selecting it does not imply
an effective clock, external memory decode, or physical bus schedule.

`CpuState` carries a separate known-register mask and known-CCR-bit mask.
Synthetic application initialization deliberately marks its provided PC, SP,
zeroed registers, and CCR as known. JR-800 reset initialization instead marks
only PC and CCR bits 7, 6, and I as known. A value stored in an unknown field is
an internal placeholder and must not be consumed as emulated hardware state.
`CpuState` also carries explicit `active`, `sleeping`, and
`waiting_for_interrupt` execution states. Reset and application initialization
establish `active`; successful HD6301V1 `SLP` and `WAI` steps establish their
respective suspended states. A two-cycle maskable-interrupt delay counter
retains the manufacturer-defined `CLI`/`TAP` recognition boundary across
instruction steps and debugger history snapshots.

## Machine initialization

`runtime::load_application` provides the initial program counter from the
JR8APP entry point and receives the initial stack pointer explicitly from its
caller. It loads all application sections into a 65,536-byte synthetic RAM bus,
including zero-filled sections.

Reset vectors, internal peripheral state, ROM, and the physical JR-800 address
map are outside this version. Application loading is a host operation and does
not produce emulated bus-access events.

## Implemented instruction subset

The executable subset is deliberately smaller than the reviewed ISA metadata:

- `NOP`;
- HD6301V1 `TAP`;
- HD6301V1 `TPA`;
- HD6301V1 `INX`;
- HD6301V1 `DEX`;
- HD6301V1 `TAB`;
- HD6301V1 `TBA`;
- HD6301V1 `XGDX`;
- HD6301V1 `DAA`;
- HD6301V1 `SLP`;
- HD6301V1 `WAI`;
- HD6301V1 `SWI`;
- HD6301V1 `ABA`;
- HD6301V1 `CBA`;
- HD6301V1 `SBA`;
- HD6301V1 `LSRD`;
- HD6301V1 `ASLD`;
- HD6301V1 `ABX`;
- HD6301V1 `ADDA #imm`;
- HD6301V1 `ADDA direct`;
- HD6301V1 `ADDA displacement,X`;
- HD6301V1 `ADDA extended`;
- HD6301V1 `ADDB #imm`;
- HD6301V1 `ADDB direct`;
- HD6301V1 `ADDB displacement,X`;
- HD6301V1 `ADDB extended`;
- HD6301V1 `ADCA #imm`;
- HD6301V1 `ADCA direct`;
- HD6301V1 `ADCA displacement,X`;
- HD6301V1 `ADCA extended`;
- HD6301V1 `ADCB #imm`;
- HD6301V1 `ADCB direct`;
- HD6301V1 `ADCB displacement,X`;
- HD6301V1 `ADCB extended`;
- HD6301V1 `ANDA #imm`;
- HD6301V1 `ANDA direct`;
- HD6301V1 `ANDA displacement,X`;
- HD6301V1 `ANDA extended`;
- HD6301V1 `ANDB #imm`;
- HD6301V1 `ANDB direct`;
- HD6301V1 `ANDB displacement,X`;
- HD6301V1 `ANDB extended`;
- HD6301V1 `BITA #imm`;
- HD6301V1 `BITA direct`;
- HD6301V1 `BITA displacement,X`;
- HD6301V1 `BITA extended`;
- HD6301V1 `BITB #imm`;
- HD6301V1 `BITB direct`;
- HD6301V1 `BITB displacement,X`;
- HD6301V1 `BITB extended`;
- HD6301V1 `SUBA #imm`;
- HD6301V1 `SUBA direct`;
- HD6301V1 `SUBA indexed`;
- HD6301V1 `SUBA extended`;
- HD6301V1 `SUBB #imm`;
- HD6301V1 `SUBB direct`;
- HD6301V1 `SUBB indexed`;
- HD6301V1 `SUBB extended`;
- HD6301V1 `CPX #imm16`;
- HD6301V1 `CPX direct`;
- HD6301V1 `CPX displacement,X`;
- HD6301V1 `CPX extended`;
- HD6301V1 `CMPA #imm`;
- HD6301V1 `CMPA direct`;
- HD6301V1 `CMPA indexed`;
- HD6301V1 `CMPA extended`;
- HD6301V1 `EORA #imm`;
- HD6301V1 `EORA direct`;
- HD6301V1 `EORA displacement,X`;
- HD6301V1 `EORA extended`;
- HD6301V1 `EORB #imm`;
- HD6301V1 `EORB direct`;
- HD6301V1 `EORB displacement,X`;
- HD6301V1 `EORB extended`;
- HD6301V1 `ORAA #imm`;
- HD6301V1 `ORAA direct`;
- HD6301V1 `ORAA displacement,X`;
- HD6301V1 `ORAA extended`;
- HD6301V1 `ORAB #imm`;
- HD6301V1 `ORAB direct`;
- HD6301V1 `ORAB displacement,X`;
- HD6301V1 `ORAB extended`;
- HD6301V1 `CMPB #imm`;
- HD6301V1 `CMPB direct`;
- HD6301V1 `CMPB displacement,X`;
- HD6301V1 `CMPB extended`;
- HD6301V1 `SBCA #imm`;
- HD6301V1 `SBCA direct`;
- HD6301V1 `SBCA displacement,X`;
- HD6301V1 `SBCA extended`;
- HD6301V1 `SBCB #imm`;
- HD6301V1 `SBCB direct`;
- HD6301V1 `SBCB displacement,X`;
- HD6301V1 `SBCB extended`;
- HD6301V1 `CLV`;
- HD6301V1 `SEV`;
- HD6301V1 `CLC`;
- HD6301V1 `SEC`;
- HD6301V1 `CLI`;
- HD6301V1 `SEI`;
- HD6301V1 `DECA`;
- HD6301V1 `DECB`;
- HD6301V1 `DEC displacement,X`;
- HD6301V1 `DEC extended`;
- HD6301V1 `INCA`;
- HD6301V1 `INCB`;
- HD6301V1 `INC displacement,X`;
- HD6301V1 `INC extended`;
- HD6301V1 `CLRA`;
- HD6301V1 `CLRB`;
- `LDAA #imm`;
- HD6301V1 `LDAB #imm`;
- HD6301V1 `LDAB direct`;
- HD6301V1 `LDAB displacement,X`;
- HD6301V1 `LDAB extended`;
- HD6301V1 `LDD #imm16`;
- HD6301V1 `LDD direct`;
- HD6301V1 `LDD displacement,X`;
- HD6301V1 `LDD extended`;
- HD6301V1 `MUL`;
- HD6301V1 `NEGA`;
- HD6301V1 `NEGB`;
- HD6301V1 `NEG displacement,X`;
- HD6301V1 `NEG extended`;
- HD6301V1 `COMA`;
- HD6301V1 `COMB`;
- HD6301V1 `COM displacement,X`;
- HD6301V1 `COM extended`;
- HD6301V1 `LSRA`;
- HD6301V1 `LSRB`;
- HD6301V1 `LSR displacement,X`;
- HD6301V1 `LSR extended`;
- HD6301V1 `ROLA`;
- HD6301V1 `ROLB`;
- HD6301V1 `ROL displacement,X`;
- HD6301V1 `ROL extended`;
- HD6301V1 `RORA`;
- HD6301V1 `RORB`;
- HD6301V1 `ROR displacement,X`;
- HD6301V1 `ROR extended`;
- HD6301V1 `ASLA`;
- HD6301V1 `ASLB`;
- HD6301V1 `ASL displacement,X`;
- HD6301V1 `ASL extended`;
- HD6301V1 `ASRA`;
- HD6301V1 `ASRB`;
- HD6301V1 `ASR displacement,X`;
- HD6301V1 `ASR extended`;
- HD6301V1 `ADDD #imm16`;
- HD6301V1 `ADDD direct`;
- HD6301V1 `ADDD displacement,X`;
- HD6301V1 `ADDD extended`;
- HD6301V1 `SUBD direct`;
- HD6301V1 `SUBD displacement,X`;
- HD6301V1 `SUBD #imm16`;
- HD6301V1 `SUBD extended`;
- HD6301V1 `LDAA direct`;
- HD6301V1 `LDAA displacement,X`;
- HD6301V1 `LDAA extended`;
- HD6301V1 `LDS #imm16`;
- HD6301V1 `LDS direct`;
- HD6301V1 `LDS displacement,X`;
- HD6301V1 `LDS extended`;
- HD6301V1 `LDX #imm16`;
- HD6301V1 `LDX direct`;
- HD6301V1 `LDX displacement,X`;
- HD6301V1 `LDX extended`;
- `STAA direct`;
- HD6301V1 `STAA extended`;
- HD6301V1 `STAA displacement,X`;
- HD6301V1 `STAB direct`;
- HD6301V1 `STAB displacement,X`;
- HD6301V1 `STAB extended`;
- HD6301V1 `STX direct`;
- HD6301V1 `STX displacement,X`;
- HD6301V1 `STX extended`;
- HD6301V1 `STS direct`;
- HD6301V1 `STS displacement,X`;
- HD6301V1 `STS extended`;
- HD6301V1 `STD direct`;
- HD6301V1 `STD displacement,X`;
- HD6301V1 `STD extended`;
- HD6301V1 `TSX`;
- HD6301V1 `INS`;
- HD6301V1 `PULA`;
- HD6301V1 `PULB`;
- HD6301V1 `DES`;
- HD6301V1 `TXS`;
- HD6301V1 `PSHA`;
- HD6301V1 `PSHB`;
- HD6301V1 `PSHX`;
- HD6301V1 `PULX`;
- `BRA rel8`;
- HD6301V1 `BRN rel8`;
- HD6301V1 `BHI rel8`;
- HD6301V1 `BLS rel8`;
- HD6301V1 `BCC rel8`;
- HD6301V1 `BCS rel8`;
- HD6301V1 `BNE rel8`;
- HD6301V1 `BEQ rel8`;
- HD6301V1 `BVC rel8`;
- HD6301V1 `BVS rel8`;
- HD6301V1 `BPL rel8`;
- HD6301V1 `BMI rel8`;
- HD6301V1 `BGE rel8`;
- HD6301V1 `BLT rel8`;
- HD6301V1 `BGT rel8`;
- HD6301V1 `BLE rel8`;
- HD6301V1 `JMP displacement,X`;
- HD6301V1 `JMP extended`;
- HD6301V1 `JSR direct`;
- HD6301V1 `JSR indexed`;
- HD6301V1 `JSR extended`;
- `BSR rel8`;
- `RTS`;
- HD6301V1 `RTI`;
- HD6301V1-only `AIM #mask,direct`;
- HD6301V1-only `AIM #mask,displacement,X`;
- HD6301V1-only `EIM #immediate,direct`;
- HD6301V1-only `EIM #immediate,displacement,X`;
- HD6301V1-only `OIM #immediate,direct`;
- HD6301V1-only `OIM #immediate,displacement,X`;
- HD6301V1-only `TIM #immediate,direct`;
- HD6301V1-only `TIM #immediate,displacement,X`;
- HD6301V1 `TST indexed`;
- HD6301V1 `TST extended`;
- HD6301V1 `TSTA`;
- HD6301V1 `TSTB`;
- HD6301V1 `CLR displacement,X`;
- HD6301V1 `CLR extended`.

These operations execute only when present in the selected exact profile. In
particular, opcodes `$1A`, `$61`, `$62`, `$65`, `$6B`, `$71`, `$72`, `$75`,
and `$7B` remain unsupported for MC6801. An unknown opcode reports
`unsupported_opcode`;
metadata staged without semantics must report `unimplemented_operation`.
Neither condition silently guesses state changes or cycle counts.

An instruction that requires an unknown CPU register reports `unknown_state`
and identifies the required `CpuStatePart`. Operand fetches that precede the
dependency remain visible, but CPU registers, flags, and cycles remain at the
instruction boundary. Instructions may make outputs known: all implemented
`LDAA` forms make A and N/Z/V known while preserving the knownness of H/I/C.
Immediate `LDAB` similarly establishes B and N/Z/V without consuming the prior
B value; A and the knownness of H/I/C are preserved.
Direct `LDAB` fetches one page-zero address byte and then reads its operand.
It likewise establishes B and N/Z/V without consuming the prior B value while
preserving A and H/I/C. Both reads must succeed before CPU state commits, and
the architectural trace orders the address-byte fetch before the data read.
Indexed `LDAB` fetches one unsigned displacement, requires X after that fetch,
and reads the byte at the low 16 bits of `X + displacement`. It establishes B
and N/Z/V without consuming the prior B value while preserving A, X, and
H/I/C. A displacement-fetch fault, unresolved X, or data-read fault leaves CPU
state uncommitted. The architectural trace omits the published restart-vector
dummy read and next-opcode prefetch.
Extended `LDAB` first fetches a two-byte big-endian address, then reads the
selected byte under the same state and flag boundary. Both address fetches and
the data read must succeed before B, CCR, PC, or cycles commit. The MC6801 rows
remain unstaged.
Immediate `LDD` fetches its high byte then low byte, establishes A from the high
byte and B from the low byte without consuming either reset-time placeholder,
and derives N/Z/V from the complete 16-bit A:B result. H/I/C are preserved.
Direct `LDD` fetches one page-zero address, then reads the A byte at that
address and the B byte at the following 16-bit address. All three reads must
succeed before A, B, N/Z/V, PC, or cycles commit. It preserves H/I/C and every
unrelated register while establishing both accumulator knownness bits. Indexed
`LDD` first fetches one unsigned displacement and requires X before reading the
same ordered high/low byte pair at the low 16 bits of `X + displacement`. It
does not consume the old A or B values. Extended `LDD` fetches a big-endian
16-bit address, then reads the same ordered pair without consuming X or the old
accumulators. All exact MC6801 `LDD` rows remain unstaged.
HD6301V1 `TAB` requires A to be known, copies it to B, makes B and N/Z/V known,
and preserves A plus H/I/C values and knownness. A prior unknown B is replaced
without being consumed.
HD6301V1 `TBA` requires B to be known, copies it to A, makes A and N/Z/V known,
and preserves B plus H/I/C values and knownness. A prior unknown A is replaced
without being consumed. An unresolved B commits no CPU state, and the MC6801
row remains unstaged pending separate exact-profile review.
HD6301V1 `XGDX` requires A, B, and X to be known, exchanges A:B with X, and
preserves every condition-code value and knownness bit. If more than one source
is unresolved, A is reported before B, and B before X; a failure commits no CPU
state.
HD6301V1 `SLP` requires no register or CCR value. It advances PC by one,
preserves every CPU register, CCR value, and knownness bit, accounts the
published four cycles, and changes the execution state from `active` to
`sleeping`. The architectural trace contains only the opcode fetch; the
published next-opcode and `$FFFF` restart-address reads remain non-architectural
at this layer. Ordinary step/run requests while sleeping perform no further
instruction fetch, add no cycles, and create no debugger-history entry. The
debugger reports `sleeping` rather than a CPU fault, and gives the entering SLP
one executed instruction while later dormant requests report zero. Reset or
explicit initialization restores `active`. E-172 adds the separately recorded
sleep-resume and internal maskable-interrupt entry steps described below.
E-182 adds a separate bounded suspended-cycle operation: it advances peripherals
and the CPU cycle counter by at most the caller's E-cycle limit, sampling the
prioritized request before the first cycle and after every elapsed cycle. It
stops immediately when the request is asserted or becomes unknown, leaves the
CPU sleeping, performs no architectural bus access, and creates no debugger
history. The next ordinary step owns wake or fault handling. RES-pin and
STBY-pin wake and automatic dormant scheduling remain unstaged.
HD6301V1 `WAI` requires PC, SP, X, A, B, and every CCR bit to be known. It
advances PC by one; writes the return PC low/high, X low/high, A, B, and CCR to
seven descending stack addresses; subtracts seven from SP; preserves all
register and CCR values; accounts nine cycles; and enters
`waiting_for_interrupt`. Its architectural trace contains the opcode fetch and
seven stack writes. A failed later write retains earlier memory side effects
but commits no CPU state or cycles. A known inactive request leaves the CPU
dormant. A maskable request with I set does not release WAI. With I clear, an
internal IRQ2 request reads its vector high then low, sets I, resumes at the
handler without stacking a second frame, and accounts five cycles. The five-
cycle internal-IRQ2 resume is taken from the compatible MC6801 WAI sequence and
remains an explicit HD6301V1 timing assumption pending an exact Hitachi wake
diagram. Reset, NMI, IRQ1, and external wake paths remain unstaged.
HD6301V1 `SWI` requires PC, SP, X, A, B, and every CCR bit to be known. It
advances the return PC by one and writes the same seven-byte frame and order as
WAI, including the pre-SWI CCR. It then reads the fixed `$FFFA:$FFFB` vector
MSB first, sets I, leaves the CPU active at the vector target, subtracts seven
from SP, and accounts twelve cycles. A pre-existing I bit does not inhibit the
instruction. The architectural trace contains the opcode fetch, seven stack
writes, and two vector reads. A failed later access retains earlier bus effects
but commits no CPU state or cycles. Its `call` classification is a debugger
policy used by E-304 step-over to target the stacked `P + 1`; it is not a claim
that SWI uses subroutine-call hardware.
HD6301V1 `DAA` requires A plus H and C to be known. It selects `$00`, `$06`,
`$60`, or `$66` from Hitachi's nine documented post-addition input regions,
adds the correction to A, writes N/Z/C, and preserves H/I/V. Inputs outside
those nine regions fail with `unimplemented_operation` rather than extending
the source table by inference. Successful execution advances PC by one and
accounts two cycles; the architectural trace contains only the opcode fetch.
HD6301V1 `ABA` requires A and B to be known, replaces A with the low eight bits
of their sum, and makes H/N/Z/V/C known while preserving I. B, X, and SP are
unchanged. If both accumulators are unresolved, A is reported before B; a
failure commits no CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `CBA` requires A and B to be known, computes A minus B without
changing either accumulator, and makes N/Z/V/C known while preserving H/I.
If both accumulators are unresolved, A is reported before B; a failure commits
no CPU state. The MC6801 row remains unstaged pending separate exact-profile
review.
HD6301V1 `SBA` requires A and B to be known, replaces A with the low eight bits
of A minus B, and makes N/Z/V/C known while preserving H/I. B, X, and SP are
unchanged. If both accumulators are unresolved, A is reported before B; a
failure commits no CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `LSRD` requires both halves of D to be known, checking A before B. It
shifts A:B right by one with zero entering bit 15, places old bit 0 in C,
clears N, sets Z from the complete 16-bit result, sets V to N exclusive-or C,
and preserves H/I. A failure commits no CPU state. The MC6801 row remains
unstaged pending separate exact-profile review.
HD6301V1 `ASLD` requires both halves of D to be known, checking A before B. It
shifts A:B left by one with zero entering bit 0, places old bit 15 in C, sets N
and Z from the complete 16-bit result, sets V to N exclusive-or C, and
preserves H/I. A failure commits no CPU state. The MC6801 row remains unstaged
pending separate exact-profile review.
HD6301V1 `ABX` requires B and X to be known, zero-extends B, adds it to X with
16-bit register wrap, and preserves B plus every condition-code value and
knownness bit. If both sources are unresolved, B is reported before X; a
failure commits no CPU state.
HD6301V1 immediate `ADDA` fetches its operand before requiring A, replaces A
with the low eight bits of the sum, and makes H/N/Z/V/C known while preserving
I. B, X, and SP are unchanged. A rejected operand fetch or unresolved A
commits no CPU state.
HD6301V1 direct `ADDA` fetches a page-zero address byte, reads the selected
operand, and only then requires A to be known. It applies the same result and
H/N/Z/V/C rules as the immediate form while preserving I, B, X, SP, and
memory. An address-fetch fault, data-read fault, uninitialized read, or
unresolved A commits no CPU state. The MC6801 and extended rows remain
unstaged pending separate exact-profile review.
HD6301V1 indexed `ADDA` fetches an unsigned displacement, requires X, reads the
selected byte at the low 16 bits of `X + displacement`, and only then requires
A. It applies the same result and H/N/Z/V/C rules while preserving I, B, X,
SP, and memory. A failed fetch/read or unresolved X/A commits no CPU state.
The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 extended `ADDA` fetches a big-endian address, reads the selected byte,
and only then requires A. It applies the same result and H/N/Z/V/C rules while
preserving I, B, X, SP, and memory. A failed fetch/read or unresolved A commits
no CPU state. The MC6801 row remains unstaged pending separate exact-profile
review.
HD6301V1 immediate `ADDB` fetches its operand before requiring B, replaces B
with the low eight bits of the sum, and makes H/N/Z/V/C known while preserving
I. A, X, and SP are unchanged. A rejected operand fetch or unresolved B
commits no CPU state. E-239 and E-240 separately stage indexed and extended.
The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 direct `ADDB` fetches a page-zero address byte, reads the selected
operand, and only then requires B to be known. It replaces B with the low eight
bits of the sum and applies the same H/N/Z/V/C and I rules as the immediate
form while preserving A, X, SP, and memory. An address-fetch fault, data-read
fault, or unresolved B commits no CPU state. E-239 and E-240 separately stage
indexed and extended. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 indexed `ADDB` fetches an unsigned displacement, requires X, reads the
selected byte at the low 16 bits of `X + displacement`, and only then requires
B. It applies the same result and H/N/Z/V/C rules while preserving I, A, X,
SP, and memory. A failed fetch/read or unresolved X/B commits no CPU state.
E-240 separately stages extended. The MC6801 row remains unstaged pending
separate exact-profile review.
HD6301V1 extended `ADDB` fetches a big-endian address, reads the selected byte,
and only then requires B. It applies the same result and H/N/Z/V/C rules while
preserving I, A, X, SP, and memory. A failed fetch/read or unresolved B commits
no CPU state. The MC6801 row remains unstaged pending separate exact-profile
review.
HD6301V1 immediate `ADCA` fetches its operand before requiring A and then the
carry bit. It replaces A with the low eight bits of `A + operand + C`, makes
H/N/Z/V/C known, and preserves I, B, X, and SP. A rejected operand fetch or
unresolved A/C commits no CPU state. E-242 through E-244 separately stage
direct, indexed, and extended. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 direct `ADCA` fetches a page-zero address byte, reads the selected
operand, and only then requires A followed by the carry bit. It applies the
same carry-input result and H/N/Z/V/C rules while preserving I, B, X, SP,
memory, and every unrelated state. A failed fetch/read or unresolved A/C
commits no CPU state. E-243 and E-244 separately stage indexed and extended.
The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 indexed `ADCA` fetches an unsigned displacement, requires X, reads the
selected byte at the low 16 bits of `X + displacement`, and only then requires
A followed by the carry bit. It applies the same carry-input result and
H/N/Z/V/C rules while preserving I, B, X, SP, memory, and every unrelated
state. A failed fetch/read or unresolved X/A/C commits no CPU state. E-244
separately stages extended. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 extended `ADCA` fetches a big-endian address, reads the selected byte,
and only then requires A followed by the carry bit. It applies the same carry-
input result and H/N/Z/V/C rules while preserving I, B, X, SP, memory, and
every unrelated state. A failed fetch/read or unresolved A/C commits no CPU
state. The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 immediate `ADCB` fetches its operand before requiring B and then the
carry bit. It replaces B with the low eight bits of `B + operand + C`, makes
H/N/Z/V/C known, and preserves I, A, X, and SP. A rejected operand fetch or
unresolved B/C commits no CPU state. E-246 through E-248 separately stage
direct, indexed, and extended. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 direct `ADCB` fetches a page-zero address byte, reads the selected
operand, and only then requires B followed by the carry bit. It applies the
same carry-input result and H/N/Z/V/C rules while preserving I, A, X, SP,
memory, and every unrelated state. A failed fetch/read or unresolved B/C
commits no CPU state. E-247 and E-248 separately stage indexed and extended.
The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 indexed `ADCB` fetches an unsigned displacement, requires X, reads the
selected byte at the low 16 bits of `X + displacement`, and only then requires
B followed by the carry bit. It applies the same carry-input result and
H/N/Z/V/C rules while preserving I, A, X, SP, memory, and every unrelated
state. A failed fetch/read or unresolved X/B/C commits no CPU state. E-248
separately stages extended. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 extended `ADCB` fetches a big-endian address, reads the selected byte,
and only then requires B followed by the carry bit. It applies the same carry-
input result and H/N/Z/V/C rules while preserving I, A, X, SP, memory, and
every unrelated state. A failed fetch/read or unresolved B/C commits no CPU
state. The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 immediate `ANDA` fetches its operand before requiring A, replaces A
with the bitwise AND of A and the operand, derives N/Z from the result, and
clears V. H/I/C values and knownness and every unrelated register are
preserved. A rejected operand fetch or unresolved A commits no CPU state. The
direct form is staged separately, E-194 later stages the indexed form, and
E-195 stages the extended form. The MC6801 row remains unstaged pending
separate exact-profile review.
HD6301V1 direct `ANDA` fetches a page-zero address byte, reads the selected
operand, and only then requires A to be known. It replaces A with the bitwise
AND of A and the operand and applies the same N/Z/V and H/I/C rules as the
immediate form. An address-fetch fault, data-read fault, or unresolved A
commits no CPU state. E-194 later stages the indexed form and E-195 stages the
extended form. The MC6801 row remains unstaged pending separate exact-profile
review.
HD6301V1 indexed `ANDA` fetches an unsigned displacement, requires X, reads the
byte at the low 16 bits of `X + displacement`, and only then requires A. It
replaces A with the bitwise AND result, derives N/Z from that result, clears V,
and preserves B, X, SP, memory, and H/I/C values and knownness. A
displacement-fetch fault, unresolved X, data-read fault, or unresolved A
leaves CPU state uncommitted. The architectural trace omits the published
restart-vector dummy read and next-opcode prefetch. E-195 separately stages
the extended form. The MC6801 row remains unstaged.
HD6301V1 extended `ANDA` fetches a 16-bit big-endian address, reads the byte at
that address, and only then requires A. It replaces A with the bitwise AND
result, derives N/Z from that result, clears V, and preserves B, X, SP, memory,
and H/I/C values and knownness. A high- or low-address-byte fetch fault,
data-read fault, or unresolved A leaves CPU state uncommitted; completed reads
remain observable. The final PC uses the low 16 bits of `P + 3`. The
architectural trace omits the published next-opcode prefetch. The MC6801 row
remains unstaged.
HD6301V1 immediate `ANDB` fetches its operand before requiring B, replaces B
with the bitwise AND of B and the operand, derives N/Z from the result, and
clears V. H/I/C values and knownness and every unrelated register are
preserved. A rejected operand fetch or unresolved B commits no CPU state. The
direct form is staged separately by E-196, E-197 later stages the indexed
form, and E-198 stages the extended form. The exact MC6801 rows remain
unstaged pending separate exact-profile review.
HD6301V1 direct `ANDB` fetches a page-zero address byte, reads the selected
operand, and only then requires B to be known. It replaces B with the bitwise
AND of B and the operand, derives N/Z from the result, clears V, and preserves
A, X, SP, memory, and H/I/C values and knownness. An address-fetch fault,
data-read fault, or unresolved B leaves CPU state and cycles uncommitted while
completed reads remain observable. The architectural trace contains the
opcode fetch, address-byte fetch, and data read while omitting the published
next-opcode prefetch. The MC6801 row and indexed and extended forms were
unstaged at E-196; E-197 later stages the indexed form and E-198 stages the
extended form. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 indexed `ANDB` fetches an unsigned displacement, requires X, reads
the byte at the low 16 bits of `X + displacement`, and only then requires B.
It replaces B with the bitwise AND result, derives N/Z from that result, clears
V, and preserves A, X, SP, memory, and H/I/C values and knownness. A
displacement-fetch fault, unresolved X, data-read fault, or unresolved B
leaves CPU state and cycles uncommitted while completed reads remain
observable. The architectural trace omits the published restart-vector dummy
read and next-opcode prefetch. E-198 separately stages the extended form. The
exact MC6801 rows remain unstaged pending separate exact-profile review.
HD6301V1 extended `ANDB` fetches a 16-bit big-endian address, reads the byte at
that address, and only then requires B. It replaces B with the bitwise AND
result, derives N/Z from that result, clears V, and preserves A, X, SP, memory,
and H/I/C values and knownness. A high- or low-address-byte fetch fault,
data-read fault, or unresolved B leaves CPU state and cycles uncommitted;
completed reads remain observable. The final PC uses the low 16 bits of
`P + 3`. The architectural trace omits the published next-opcode prefetch. The
MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 immediate `BITA` fetches its operand before requiring A, derives N/Z
from `A AND operand`, and clears V without changing A. H/I/C values and
knownness and every CPU register are preserved. A rejected operand fetch or
unresolved A commits no CPU state. The direct form is staged separately by
E-199, and E-200 stages the indexed form. The exact MC6801 rows and extended
form were still unstaged at E-200; E-201 later stages the extended form. The
exact MC6801 rows remain unstaged pending separate exact-profile review.
HD6301V1 direct `BITA` fetches a page-zero address byte, reads the selected
operand, and only then requires A. It derives N/Z from `A AND operand`, clears
V, and leaves A, memory, every other register, and H/I/C values and knownness
unchanged. An address-fetch fault, data-read fault, or unresolved A leaves CPU
state and cycles uncommitted while completed reads remain observable. The
architectural trace contains the opcode fetch, address-byte fetch, and data
read while omitting the published next-opcode prefetch. E-200 separately stages
the indexed form and E-201 stages the extended form. The exact MC6801 rows
remain unstaged pending separate exact-profile review.
HD6301V1 indexed `BITA` fetches an unsigned displacement, requires X, reads the
selected operand, and only then requires A. It derives N/Z from `A AND operand`,
clears V, and leaves A, X, memory, every other register, and H/I/C values and
knownness unchanged. A displacement-fetch fault, unresolved X, data-read fault,
or unresolved A leaves CPU state and cycles uncommitted while completed reads
remain observable. PC and effective-address calculation use 16-bit wraparound.
The architectural trace omits the published restart-vector dummy read and
next-opcode prefetch. E-201 separately stages the extended form. The exact
MC6801 rows remain unstaged pending separate exact-profile review.
HD6301V1 extended `BITA` fetches a 16-bit big-endian address, reads the selected
operand, and only then requires A. It derives N/Z from `A AND operand`, clears
V, and leaves A, memory, every other register, and H/I/C values and knownness
unchanged. A high- or low-address-byte fetch fault, data-read fault, or
unresolved A leaves CPU state and cycles uncommitted while completed reads
remain observable. The final PC uses the low 16 bits of `P + 3`. The
architectural trace omits the published next-opcode prefetch. The exact MC6801
row remains unstaged pending separate exact-profile review.
HD6301V1 immediate `BITB` fetches its operand before requiring B, derives N/Z
from `B AND operand`, and clears V without changing B. H/I/C values and
knownness and every CPU register are preserved. A rejected operand fetch or
unresolved B commits no CPU state. E-202 separately stages the direct form.
E-203 stages the indexed form, and E-204 the extended form. The exact MC6801
rows remain unstaged pending separate exact-profile review.
HD6301V1 direct `BITB` fetches a page-zero address byte, reads the selected
operand, and only then requires B. It derives N/Z from `B AND operand`, clears
V, and leaves B, memory, every other register, and H/I/C values and knownness
unchanged. An address-fetch fault, data-read fault, or unresolved B leaves CPU
state and cycles uncommitted while completed reads remain observable. The
architectural trace contains the opcode fetch, address-byte fetch, and data
read while omitting the published next-opcode prefetch. E-203 separately stages
the indexed form and E-204 the extended form. The exact MC6801 rows remain
unstaged pending separate exact-profile review.
HD6301V1 indexed `BITB` fetches an unsigned displacement, requires X, reads the
selected operand, and only then requires B. It derives N/Z from `B AND operand`,
clears V, and leaves B, X, memory, every other register, and H/I/C values and
knownness unchanged. A displacement-fetch fault, unresolved X, data-read fault,
or unresolved B leaves CPU state and cycles uncommitted while completed reads
remain observable. PC and effective-address calculation use 16-bit wraparound.
The architectural trace omits the published restart-vector dummy read and
next-opcode prefetch. E-204 separately stages the extended form. The exact
MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 extended `BITB` fetches a 16-bit big-endian address, reads the selected
operand, and only then requires B. It derives N/Z from `B AND operand`, clears
V, and leaves B, memory, every other register, and H/I/C values and knownness
unchanged. A high- or low-address-byte fetch fault, data-read fault, or
unresolved B leaves CPU state and cycles uncommitted while completed reads
remain observable. The final PC uses the low 16 bits of `P + 3`. The
architectural trace omits the published next-opcode prefetch. The exact MC6801
row remains unstaged pending separate exact-profile review.
HD6301V1 immediate `EORA` fetches one byte, then requires A to be known. It
replaces A with the bitwise exclusive OR of A and the operand, derives N/Z
from the result, and clears V while preserving B plus H/I/C values and
knownness. A rejected operand fetch or unresolved A commits no CPU state.
E-152 separately stages the direct form, E-205 the indexed form, and E-206 the
extended form. E-207 separately stages immediate EORB, E-208 its direct form,
E-209 its indexed form, and E-210 its extended form. The exact MC6801 rows
remain unstaged.
HD6301V1 direct `EORA` fetches a page-zero address byte, reads the selected
operand, and only then requires A to be known. It replaces A with the bitwise
exclusive OR of A and the operand, derives N/Z from the result, and clears V.
B plus H/I/C values and knownness are preserved. An address-fetch fault,
data-read fault, or unresolved A commits no CPU state. E-205 separately stages
the indexed form, and E-206 the extended form. E-207 separately stages
immediate EORB, E-208 its direct form, E-209 its indexed form, and E-210 its
extended form. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 indexed `EORA` fetches an unsigned displacement, requires X, reads the
selected operand, and only then requires A. It replaces A with `A XOR operand`,
derives N/Z from the result, and clears V while preserving B, X, memory, every
other register, and H/I/C values and knownness. A displacement-fetch fault,
unresolved X, data-read fault, or unresolved A leaves CPU state and cycles
uncommitted while completed reads remain observable. PC and effective-address
calculation use 16-bit wraparound. The architectural trace omits the published
restart-vector dummy read and next-opcode prefetch. E-206 separately stages the
extended form. E-207 separately stages immediate EORB, E-208 its direct form,
E-209 its indexed form, and E-210 its extended form. The exact MC6801 rows
remain unstaged pending separate exact-profile review.
HD6301V1 extended `EORA` fetches a big-endian 16-bit address, reads the selected
operand, and only then requires A. It replaces A with `A XOR operand`, derives
N/Z from the result, and clears V while preserving B, X, SP, memory, and H/I/C
values and knownness. A high- or low-address-byte fetch fault, data-read fault,
or unresolved A leaves CPU state and cycles uncommitted while completed reads
remain observable. The final PC uses the low 16 bits of `P + 3`. The
architectural trace omits the published next-opcode prefetch. E-207 separately
stages immediate EORB, E-208 its direct form, E-209 its indexed form, and E-210
its extended form. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 immediate `EORB` fetches one byte, then requires B to be known. It
replaces B with `B XOR operand`, derives N/Z from the result, and clears V while
preserving A plus H/I/C values and knownness. An operand-fetch fault or
unresolved B leaves CPU state and cycles uncommitted while the completed fetch
remains observable. Operand-fetch and final-PC calculation use 16-bit
wraparound. The architectural trace omits the published next-opcode prefetch.
E-208 separately stages the direct form, E-209 the indexed form, and E-210 the
extended form. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 direct `EORB` fetches a page-zero address byte, reads the selected
operand, and only then requires B to be known. It replaces B with
`B XOR operand`, derives N/Z from the result, and clears V while preserving A,
memory, every other register, and H/I/C values and knownness. An address-fetch
fault, data-read fault, or unresolved B leaves CPU state and cycles uncommitted
while completed reads remain observable. The architectural trace contains the
opcode fetch, address fetch, and data read while omitting the published
next-opcode prefetch. E-209 separately stages the indexed form and E-210 the
extended form. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 indexed `EORB` fetches an unsigned displacement, requires X, reads the
selected operand, and only then requires B. It replaces B with `B XOR operand`,
derives N/Z from the result, and clears V while preserving A, X, memory, every
other register, and H/I/C values and knownness. A displacement-fetch fault,
unresolved X, data-read fault, or unresolved B leaves CPU state and cycles
uncommitted while completed reads remain observable. PC and effective-address
calculation use 16-bit wraparound. The architectural trace omits the published
restart-vector dummy read and next-opcode prefetch. E-210 separately stages the
extended form. The exact MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 extended `EORB` fetches a big-endian 16-bit address, reads the selected
operand, and only then requires B. It replaces B with `B XOR operand`, derives
N/Z from the result, and clears V while preserving A, X, SP, memory, every
unrelated register, and H/I/C values and knownness. A high- or low-address-byte
fetch fault, data-read fault, or unresolved B leaves CPU state and cycles
uncommitted while completed reads remain observable. The final PC uses the low
16 bits of `P + 3`. The architectural trace omits the published next-opcode
prefetch. The exact MC6801 row remains unstaged pending separate exact-profile
review.
HD6301V1 immediate `ORAA` fetches one immediate byte and then requires A to be
known. It replaces A with the bitwise inclusive OR of A and the operand,
derives N/Z from the result, and clears V. H/I/C values and knownness and every
unrelated register are preserved. An operand-fetch fault or unresolved A
commits no CPU state.
HD6301V1 direct `ORAA` fetches a page-zero address byte, reads the selected
operand, and only then requires A to be known. It replaces A with the bitwise
inclusive OR of A and the operand, derives N/Z from the result, and clears V.
H/I/C values and knownness and every unrelated register are preserved. An
address-fetch fault, data-read fault, or unresolved A commits no CPU state.
HD6301V1 indexed `ORAA` fetches an unsigned displacement, requires X, reads
the operand at the low 16 bits of `X + displacement`, and only then requires A
to be known. It replaces A with the bitwise inclusive OR of A and the operand,
derives N/Z from the result, and clears V. H/I/C values and knownness, X,
memory, and every unrelated register are preserved. A displacement-fetch
fault, unresolved X, data-read fault, or unresolved A commits no CPU state.
HD6301V1 extended `ORAA` fetches a big-endian 16-bit address, reads the
selected operand, and only then requires A to be known. It replaces A with the
bitwise inclusive OR of A and the operand, derives N/Z from the result, and
clears V. H/I/C values and knownness, memory, and every unrelated register are
preserved. An address-byte fetch fault, data-read fault, or unresolved A
commits no CPU state. The MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 immediate `ORAB` fetches one immediate byte and then requires B to be
known. It replaces B with the bitwise inclusive OR of B and the operand,
derives N/Z from the result, and clears V. H/I/C values and knownness and every
unrelated register are preserved. An operand-fetch fault or unresolved B
commits no CPU state. The direct form is separately staged by E-214, the
indexed form by E-215, and the extended form by E-216. The exact MC6801 rows
remain unstaged pending separate exact-profile review.
HD6301V1 direct `ORAB` fetches a page-zero address byte, reads the selected
operand, and only then requires B to be known. It replaces B with the bitwise
inclusive OR of B and the operand, derives N/Z from the result, and clears V.
H/I/C values and knownness, memory, A, and every unrelated register are
preserved. An address-fetch fault, data-read fault, or unresolved B commits no
CPU state. The indexed form is separately staged by E-215 and the extended
form by E-216. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 indexed `ORAB` fetches an unsigned displacement, requires X, reads the
operand at the low 16 bits of `X + displacement`, and only then requires B to
be known. It replaces B with the bitwise inclusive OR of B and the operand,
derives N/Z from the result, and clears V. H/I/C values and knownness, X,
memory, A, and every unrelated register are preserved. A displacement-fetch
fault, unresolved X, data-read fault, or unresolved B commits no CPU state.
The extended form is separately staged by E-216. The exact MC6801 rows remain
unstaged pending separate exact-profile review.
HD6301V1 extended `ORAB` fetches a big-endian 16-bit address, reads the
selected operand, and only then requires B to be known. It replaces B with the
bitwise inclusive OR of B and the operand, derives N/Z from the result, and
clears V. H/I/C values and knownness, memory, A, and every unrelated register
are preserved. An address-byte fetch fault, data-read fault, or unresolved B
commits no CPU state. The exact MC6801 rows remain unstaged pending separate
exact-profile review.
HD6301V1 `MUL` requires both A and B to be known, computes their unsigned
16-bit product, and replaces A:B with its high and low bytes. It updates and
makes known only C, taken from result bit 7; H/I/N/Z/V values and knownness are
preserved. If both accumulators are unresolved, A is reported first.
HD6301V1 `NEGA` requires A to be known, replaces it with the low eight bits of
`0 - A`, derives N/Z from the result, sets V only for input `$80`, and sets C
exactly for a nonzero input. H/I values and knownness and B are preserved. An
unresolved A commits no CPU state.
HD6301V1 `NEGB` applies the same two's-complement and flag behavior to B while
preserving A and its knownness. An unresolved B commits no CPU state; the
exact MC6801 row remains unstaged.
HD6301V1 indexed `NEG` fetches an unsigned displacement, requires X to be
known, reads the byte at the low 16 bits of `X + displacement`, replaces it
with the low eight bits of `0 - value`, then writes it back to the same
address. Extended `NEG` fetches a big-endian 16-bit address and performs the
same read-modify-write operation without consuming X. Both forms derive N/Z
from the result, set V only for input `$80`, set C exactly for a nonzero input,
preserve H/I and every CPU register, and take six cycles. Operand, data-read,
and rejected-write faults leave CPU state uncommitted; a rejected write keeps
the preceding destination-read trace. The exact MC6801 rows remain unstaged.
HD6301V1 `COMA` requires A to be known, replaces it with its one's complement,
derives N/Z from the result, clears V, and sets C. It preserves H/I values and
knownness and does not consume or change B.
HD6301V1 `COMB` applies the same flag behavior to B while preserving A and its
knownness; unresolved B fails without committing CPU state.
HD6301V1 indexed `COM` fetches an unsigned displacement, requires X, reads the
selected byte, writes its one's complement back to the same address, derives
N/Z from the result, clears V, and sets C. Extended `COM` fetches a big-endian
16-bit address and applies the same operation without consuming X. Both forms
preserve H/I and every CPU register and take six cycles. Operand, data-read,
and rejected-write faults leave CPU state uncommitted; a rejected write keeps
the preceding destination-read trace. The exact MC6801 rows remain unstaged.
HD6301V1 `LSRA` requires A to be known, shifts it right by one with zero
entering bit 7, writes the original bit 0 to C, clears N, derives Z from the
result, and sets V to `N xor C` (therefore equal to C). H/I values and
knownness and B are preserved; A and N/Z/V/C become known.
HD6301V1 `LSRB` requires B to be known, shifts it right by one with zero
entering bit 7, writes the original bit 0 to C, clears N, derives Z from the
result, and sets V to `N xor C` (therefore equal to C). H/I values and
knownness and A are preserved; B and N/Z/V/C become known. Indexed memory
`LSR` fetches an unsigned displacement, requires X, reads the selected byte,
shifts it right with zero entering bit 7, and writes it back to the same
address. Extended memory `LSR` fetches a big-endian 16-bit address and applies
the same operation without consuming X. Both forms use the same N/Z/V/C rules,
preserve H/I and every register, and take six cycles. Operand, data-read, and
rejected-write faults leave CPU state uncommitted; a rejected write keeps the
preceding destination-read trace. The exact MC6801 rows remain unstaged.
HD6301V1 `ROLA` requires A and the prior C value to be known, shifts A left by
one with prior C entering bit 0, writes the original A bit 7 to C, derives N/Z
from the result, and sets V to `N xor C`. H/I values and knownness are
preserved; A and N/Z/V/C become known. If both inputs are unresolved, A is
reported first.
HD6301V1 `ROLB` requires B and the prior C value to be known, shifts B left by
one with prior C entering bit 0, writes the original B bit 7 to C, derives N/Z
from the result, and sets V to `N xor C`. H/I values and knownness are
preserved; B and N/Z/V/C become known. If both inputs are unresolved, B is
reported first. Indexed memory `ROL` fetches an unsigned displacement, requires
X and prior C, reads the selected byte, rotates it left through C, and writes
it back to the same address. Extended memory `ROL` fetches a big-endian 16-bit
address and applies the same operation without consuming X. Both forms derive
N/Z from the result, copy the original bit 7 to C, set V to `N xor C`, preserve
H/I and every register, and take six cycles. Operand, unresolved-C, data-read,
and rejected-write failures leave CPU state uncommitted. The exact MC6801 rows
remain unstaged.
HD6301V1 `RORA` requires A and the prior C value to be known, shifts A right by
one with prior C entering bit 7, writes the original A bit 0 to C, derives N/Z
from the result, and sets V to `N xor C`. H/I values and knownness and B are
preserved; A and N/Z/V/C become known. If both inputs are unresolved, A is
reported first.
HD6301V1 `RORB` requires B and the prior C value to be known, shifts B right by
one with prior C entering bit 7, writes the original B bit 0 to C, derives N/Z
from the result, and sets V to `N xor C`. H/I values and knownness are
preserved along with A; B and N/Z/V/C become known. If both inputs are
unresolved, B is reported first. Indexed memory `ROR` fetches an unsigned
displacement, requires X and prior C, reads the selected byte, rotates it right
through C, and writes it back to the same address. Extended memory `ROR`
fetches a big-endian 16-bit address and applies the same operation without
consuming X. Both forms derive N/Z from the result, copy the original bit 0 to
C, set V to `N xor C`, preserve H/I and every register, and take six cycles.
Operand, unresolved-C, data-read, and rejected-write failures leave CPU state
uncommitted. The exact MC6801 rows remain unstaged.
HD6301V1 `ASLA` requires A to be known, shifts it left by one with zero entering
bit 0, writes the original bit 7 to C, derives N/Z from the result, and sets V
to `N xor C`. B plus H/I values and knownness are preserved; A and N/Z/V/C
become known.
HD6301V1 `ASLB` requires B to be known, shifts it left by one with zero entering
bit 0, writes the original bit 7 to C, derives N/Z from the result, and sets V
to `N xor C`. A plus H/I values and knownness are preserved; B and N/Z/V/C
become known. Indexed memory `ASL` fetches an unsigned displacement, requires
X, reads the selected byte, shifts it left with zero entering bit 0, and writes
it back to the same address. Extended memory `ASL` fetches a big-endian 16-bit
address and applies the same operation without consuming X. Both forms derive
N/Z from the result, copy the original bit 7 to C, set V to `N xor C`, preserve
H/I and every register, and take six cycles. Operand, data-read, and rejected-
write faults leave CPU state uncommitted. The exact MC6801 rows remain
unstaged.
HD6301V1 `ASRA` requires A to be known, shifts it right by one while retaining
the original sign bit in result bit 7, writes the original bit 0 to C, derives
N/Z from the result, and sets V to `N xor C`. B plus H/I values and knownness
are preserved; A and N/Z/V/C become known.
HD6301V1 `ASRB` requires B to be known, shifts it right by one while retaining
the original sign bit in result bit 7, writes the original bit 0 to C, derives
N/Z from the result, and sets V to `N xor C`. A plus H/I values and knownness
are preserved; B and N/Z/V/C become known. Indexed memory `ASR` fetches an
unsigned displacement, requires X, reads the selected byte, shifts it right
while retaining its sign bit, and writes it back to the same address. Extended
memory `ASR` fetches a big-endian 16-bit address and applies the same operation
without consuming X. Both forms derive N/Z from the result, copy the original
bit 0 to C, set V to `N xor C`, preserve H/I and every register, and take six
cycles. Operand, data-read, and rejected-write faults leave CPU state
uncommitted. The exact MC6801 rows remain unstaged.
HD6301V1 immediate `ADDD` fetches a big-endian 16-bit addend from `P + 1` and
`P + 2`, and only then requires A and B to be known. It replaces A:B with the
low 16 bits of the sum, makes N/Z/V/C known, preserves H/I values and
knownness, and takes three cycles. Either failed operand fetch or unresolved
accumulator state leaves CPU state uncommitted.
HD6301V1 direct `ADDD` fetches a page-zero address byte, reads the addend high
byte there and the low byte at the next 16-bit address, and only then requires
A and B to be known. It replaces A:B with the low 16 bits of the sum, makes
N/Z/V/C known, preserves H/I values and knownness, and takes four cycles. A
failed second read retains the successful first-read trace without CPU-state
commit.
HD6301V1 indexed `ADDD` fetches one unsigned displacement, requires X, reads
the addend high byte at the low 16 bits of `X + displacement`, then reads the
low byte at the next 16-bit address. It only then requires A and B to be known,
replaces A:B with the low 16 bits of the sum, makes N/Z/V/C known, preserves
X and H/I values and knownness, and takes five cycles. A failed second read
retains the successful first-read trace without CPU-state commit.
HD6301V1 extended `ADDD` fetches its big-endian 16-bit source address from
`P + 1` and `P + 2`, then reads the addend high byte at that address and low
byte at the next 16-bit address. Both reads finish before A/B knownness is
enforced. It replaces A:B with the low 16 bits of the sum, makes N/Z/V/C known,
and preserves H/I values and knownness. It takes five cycles; a failed second
read retains the successful first-read trace without CPU-state commit.
HD6301V1 direct `SUBD` reads its page-zero subtrahend high byte then low byte,
requires both A and B to be known before committing, and replaces A:B with the
low 16 bits of A:B minus that operand. It makes N/Z/V/C known, uses C as an
unsigned borrow, and preserves H/I values and knownness. If both accumulators
are unresolved, A is reported first.
Indexed `SUBD` first fetches its unsigned displacement and requires X, forms
the low 16 bits of `X + displacement`, then reads the high and low subtrahend
bytes. Both reads finish before A/B knownness is enforced. It applies the same
16-bit result and flag rules in five cycles while leaving X unchanged; a failed
second read retains the successful first-read trace without CPU-state commit.
Immediate `SUBD` fetches its big-endian 16-bit subtrahend from `P + 1` and
`P + 2`, then enforces A/B knownness. It applies the same result and flag rules
in three cycles without data-memory access; either operand-fetch failure or
unresolved accumulator state leaves CPU state uncommitted.
Extended `SUBD` fetches its big-endian 16-bit source address from `P + 1` and
`P + 2`, then reads the subtrahend high byte at that address and low byte at
the next 16-bit address. Both reads finish before A/B knownness is enforced.
It applies the same result and flag rules in five cycles; a failed second read
retains the successful first-read trace without CPU-state commit.
HD6301V1 `DECB` requires B to be known, replaces it with the low eight bits of
`B-1`, updates N/Z/V, and preserves H/I/C. It does not consume or change A. An
unresolved reset-time B reports `unknown_state` without committing CPU state;
the MC6801 row remains unstaged pending separate exact-profile review. Indexed
memory `DEC` fetches an unsigned displacement, requires X, reads the selected
byte, subtracts one with 8-bit wrap, and writes the result back to the same
address. Extended memory `DEC` fetches a big-endian 16-bit address and applies
the same operation without consuming X. Both forms derive N/Z from the result,
set V exactly for input `$80`, preserve H/I/C and every register, and take six
cycles. Operand, data-read, and rejected-write failures leave CPU state
uncommitted; a rejected write retains the preceding destination-read trace.
The exact MC6801 rows remain unstaged.
HD6301V1 `INCB` requires B to be known, replaces it with the low eight bits of
`B+1`, derives N/Z from the result, sets V only for input `$7F`, and preserves
H/I/C plus A and its knownness. An unresolved B commits no CPU state. The
MC6801 row remains unstaged.
HD6301V1 indexed `INC` fetches an unsigned displacement, requires X to be
known, forms the low 16 bits of `X + displacement`, then reads and increments
the selected byte before writing it back. It takes six cycles, leaves X and
every other register unchanged, updates N/Z/V, and preserves H/I/C. Operand,
data-read, and rejected-write faults leave CPU state uncommitted; a rejected
write retains the preceding destination-read trace. The architectural trace
omits the two published restart-vector dummy reads and next-opcode prefetch.
The MC6801 row remains unstaged.

`BSR` writes the return low byte at the initial SP before the high byte at
`SP-1`; HD6301V1 `JSR` uses the same write order after fetching its direct,
indexed, or extended target operand. The return is `P+2` for direct/indexed and
`P+3` for extended. Both call families finish with `SP-2`, while `RTS`
reads high from `SP+1` before low from `SP+2`. Direct `AIM`, `EIM`, and `OIM`
fetch their immediate byte then direct address and perform destination read
before write. Their indexed forms use the same operand order, treat the second
operand as an unsigned displacement from X, and require X to be known before
the destination read. Direct and indexed `TIM` likewise fetch immediate before
the address byte, compute flags from the AND result, and do not write memory.
The indexed form treats its address byte as an unsigned displacement and
requires X before the destination read; the direct form uses the byte as a
page-zero address and consumes no register value. These are architectural
access events, not the manufacturers' dummy/prefetch pin cycles.
Extended `TST` fetches the tested address high byte then low byte, reads that
byte without writing it, derives N/Z from the value, and clears V/C while
preserving H/I. It consumes no CPU register value, so it can establish N/Z/V/C
knownness directly from readable memory after reset. Either address-fetch
fault or the data-read fault leaves CPU state uncommitted.
Indexed `TST` fetches one unsigned displacement, then requires X before reading
the byte at the low 16 bits of `X + displacement`. It applies the same
read-only flag semantics as the extended form. A displacement-fetch fault,
unresolved X, or data-read fault leaves CPU state uncommitted.
Extended `INC` fetches its destination address high byte then low byte, reads
the selected byte, increments it with 8-bit wrap, then writes the result back
to the same address. N/Z reflect the result, V is set only for input `$7F`, and
H/I/C plus every CPU register are preserved. Address-fetch and data-read
faults perform no write; a rejected write leaves CPU state uncommitted after
the observable destination read.
Extended `JMP` fetches target MSB then LSB, preserves CCR and unknown-state
masks, and can therefore execute from reset entry without consuming unknown
A, B, X, or SP.
Indexed `JMP` fetches an unsigned displacement, then requires the old X value
to be known. Its final PC is the low 16 bits of `X + displacement`; X, CCR, and
all knownness masks are preserved. The architectural trace contains only the
opcode and displacement fetches, not the published restart-vector read or
target-opcode prefetch.
Each `JSR` checks SP knownness only after its encoded target bytes have been
fetched and, for indexed addressing, after X is known. Indexed JSR diagnoses an
unknown X before an unknown SP. A failed first stack write leaves CPU state
unchanged; if the second write fails, the successful low-byte bus write remains
observable while CPU state is still uncommitted. The MC6801 JSR rows remain
unstaged pending separate exact-profile review.
HD6301V1 `PULA` requires SP to be known, reads A at `SP+1`, then establishes A
and finishes with `SP+1` while preserving every CCR value and knownness bit.
A rejected read commits no CPU state. The MC6801 row remains unstaged pending
separate exact-profile review.
HD6301V1 `PULB` applies the same stack-address and fault boundary to B: it
reads `SP+1`, establishes B only after a successful read, and preserves A plus
every CCR value and knownness bit. The MC6801 row remains unstaged pending
separate exact-profile review.
HD6301V1 `TSX` requires SP to be known, replaces X with the low 16 bits of
`SP+1`, and leaves SP plus every CCR value and knownness bit unchanged. Success
establishes X knownness without consuming its old reset-time placeholder; an
unresolved SP fails without committing CPU state. The MC6801 row remains
unstaged pending separate exact-profile review.
HD6301V1 `INS` requires SP to be known, replaces it with the low 16 bits of
`SP+1`, and preserves every CCR value and knownness bit. An unresolved SP fails
without committing CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `DES` requires SP to be known, replaces it with the low 16 bits of
`SP-1`, and preserves every CCR value and knownness bit. An unresolved SP fails
without committing CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `TXS` requires X to be known, replaces SP with the low 16 bits of
`X-1`, and leaves X plus every CCR value and knownness bit unchanged. Success
establishes SP knownness without consuming its old reset-time placeholder; an
unresolved X fails without committing CPU state. The MC6801 row remains
unstaged pending separate exact-profile review.
HD6301V1 `PSHA` requires A and SP to be known, writes A at the initial SP, and
finishes with `SP-1` while preserving A and every CCR value and knownness bit.
An unresolved A is reported before an unresolved SP, and a rejected write
commits no CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `PSHB` requires B and SP to be known, writes B at the initial SP, and
finishes with `SP-1` while preserving B and every CCR value and knownness bit.
An unresolved B is reported before an unresolved SP, and a rejected write
commits no CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `PSHX` requires both X and SP to be known, writes X low at the initial
SP and X high at `SP-1`, and finishes with `SP-2` while preserving X and CCR.
It follows the same partial-write boundary: a failed second write retains the
successful low-byte bus write but does not commit CPU state. The MC6801 row
remains unstaged pending separate exact-profile review.
HD6301V1 `PULX` requires SP to be known, reads X high at `SP+1` and X low at
`SP+2`, then establishes X and finishes with `SP+2` while preserving CCR. A
failed second read retains the successful first read in the architectural trace
but does not commit CPU state. The MC6801 row remains unstaged pending separate
exact-profile review.
Extended `LDAA` fetches the effective-address MSB then LSB, reads that address
through `Bus`, and establishes A without consuming its reset-time placeholder.
Direct `LDAA` fetches one address byte, zero-extends it into page zero, then
reads the addressed byte through `Bus` with the same state-establishing behavior.
Indexed `LDAA` fetches one unsigned displacement byte, requires X to be known,
then reads from the low 16 bits of `X + displacement`; it establishes A and
N/Z/V without consuming the prior A value.
Indexed `STAA` uses the same unsigned X-relative address formation, then
requires A to be known and writes it once without changing A or X. It updates
N/Z/V from A only after the write succeeds. If both source registers are
unknown, X is reported first because the destination cannot yet be formed.
The published restart-vector read and next-opcode prefetch are not architectural
trace events, and the corresponding MC6801 row remains unstaged.
Direct `STAB` fetches one page-zero destination byte, requires B to be known,
and writes it once without changing A or B. It updates N/Z/V from B only after
the write succeeds, preserves H/I/C values and knownness, and commits no CPU
state after an address-fetch or rejected-write fault. Indexed `STAB` instead
fetches one unsigned displacement, requires X before B, and writes B to the low
16 bits of `X + displacement`. It has the same register and flag effects in
four cycles. Its published restart-vector read and next-opcode prefetch are not
architectural trace events. Extended `STAB` fetches a big-endian destination,
then applies the same single-write, flag, and preservation boundary in four
cycles. All exact MC6801 `STAB` rows remain unstaged.
`TAP` requires A to be known, copies A bits 5 through 0 into H/I/N/Z/V/C,
restores the fixed one bits at CCR positions 7 and 6, makes all CCR bits known,
and leaves A unchanged. An unresolved A fails without committing CPU state.
`TPA` requires all six documented CCR bits to be known, copies the CCR value
with fixed bits 7 and 6 set into A, makes A known, and preserves every CCR value
and knownness bit. An unresolved CCR fails without committing CPU state.
`CLV` clears V and makes that single CCR bit known while preserving every other
CCR value and knownness bit.
`SEV` sets V and makes that single CCR bit known while preserving every other
CCR value and knownness bit.
`CLC` clears C and makes that single CCR bit known while preserving every other
CCR value and knownness bit.
`SEC` sets C and makes that single CCR bit known while preserving every other
CCR value and knownness bit.
`CLI` clears I and makes that single CCR bit known while preserving every other
CCR value and knownness bit. If a known request was already asserted while I
was set, `CLI` defers active-mode acceptance until subsequent instructions have
completed at least two E cycles. Thus two one-cycle instructions or one
instruction taking two or more cycles execute before interrupt entry. `TAP`
uses the same delay when it clears a previously set I bit under an already
asserted request. A request first asserted after either instruction is sampled
at the next boundary without that delay.
`SEI` sets I and makes that single CCR bit known while preserving every other
CCR value and knownness bit.
Immediate `LDS` fetches the stack-pointer high byte then low byte, makes SP and
N/Z/V known, and does not consume the unresolved reset-time SP placeholder.
Direct `LDS` fetches one page-zero address, reads SP high at that address and
SP low at the following 16-bit address, then establishes SP and N/Z/V only
after both reads succeed. The old SP value is not required, so the instruction
can establish stack state immediately after reset. H/I/C and every unrelated
register value and knownness bit are preserved.
Indexed `LDS` fetches an unsigned displacement, requires X to form the effective
address, then reads the replacement SP high byte followed by its low byte. An
unresolved X is diagnosed after the displacement fetch and before either data
read. Extended `LDS` fetches a big-endian 16-bit address and performs the same
ordered high/low reads without consuming X or the old SP value. Effective and
following addresses use 16-bit wraparound. Both forms commit SP, N/Z/V, PC, and
cycles only after every required read succeeds.
Immediate `LDD` uses that same big-endian fetch order for accumulator D=A:B;
both bytes must be fetched before A, B, N/Z/V, PC, or cycles are committed.
Direct `LDD` applies the same result and flag rules after an address-byte fetch
and ordered high/low data reads. The following address uses project-wide
16-bit arithmetic, so a direct address of `$FF` reads the low byte at `$0100`.
Indexed `LDD` additionally requires X after fetching its unsigned displacement;
effective-address and following-byte calculations use the same 16-bit
arithmetic. Extended `LDD` fetches a big-endian address and reads its high/low
pair without requiring X or the old A/B values. The published restart-vector
read and next-opcode prefetch are not architectural trace events.
Immediate `LDX` uses the same big-endian fetch and flag rules for X, making the
index register known without consuming its reset-time placeholder.
Direct `LDX` fetches one page-zero address, reads X high from that address and
X low from the following 16-bit address, and applies the same state-establishing
and flag rules. Both reads must succeed before X, N/Z/V, PC, or cycles commit;
the architectural trace retains the address fetch and both ordered data reads.
Indexed `LDX` fetches an unsigned displacement, requires the old X value to
form the 16-bit effective address, and reads the replacement X high byte then
low byte. The displacement and both reads precede state commit; unresolved X
fails after the displacement fetch without reading the destination.
Extended `LDX` fetches a big-endian 16-bit address and reads replacement X high
from that address and low from the following 16-bit address. Both address bytes
and both data reads must succeed before X, N/Z/V, PC, or cycles commit; the old
X value is not consumed. The corresponding MC6801 `LDS` and `LDX` rows remain
unstaged pending separate exact-profile review.
`CLRA` likewise does not consume reset-time A: it writes zero to A, makes A and
N/Z/V/C known, and preserves H/I values and knownness.
`CLRB` applies the same state-establishing clear to B without consuming its
reset-time placeholder; A and every unrelated register remain unchanged.
`DECA` requires A to be known, subtracts one with 8-bit wraparound, makes N/Z/V
known, and preserves H/I/C values and knownness. Its overflow output is set
only when the pre-decrement operand is `$80`.
`INCA` requires A to be known, adds one with 8-bit wraparound, makes N/Z/V
known, and preserves H/I/C values and knownness. Its overflow output is set
only when the pre-increment operand is `$7F`.
`INX` requires X to be known, adds one with 16-bit wraparound, updates and
makes known only Z, and preserves H/I/N/V/C values and knownness.
`DEX` follows the same knownness and flag boundary while subtracting one with
16-bit wraparound.
Direct `STX` fetches one page-zero destination byte, requires X to be known,
writes X high byte to that address and X low byte to the following address,
then derives N/Z/V from the unchanged X value while preserving H/I/C. If the
second write fails, the first bus write remains visible while CPU state stays
at the instruction boundary. Indexed `STX` uses the unchanged X both as the
base of an unsigned-displacement destination and as the stored value; effective
address calculation wraps to 16 bits. Extended `STX` first fetches a two-byte
big-endian destination and otherwise follows the same knownness, write ordering,
flag, and partial-write boundary. A low-address fetch fault occurs before X is
consumed or any destination write is attempted.
Direct `STS` fetches one page-zero destination byte, requires SP to be known,
writes SP high byte to that address and SP low byte to the following 16-bit
address, then derives N/Z/V from the unchanged SP while preserving H/I/C. It
uses the same ordered-write and partial-failure boundary as `STX`: a successful
high-byte write remains visible if the low-byte write fails, while CPU state
does not commit. Indexed `STS` fetches an unsigned displacement, requires X
before SP, and writes at the low 16 bits of `X + displacement`. Extended `STS`
fetches a big-endian destination before requiring SP. Both addressed forms
preserve the same flags, write ordering, wraparound, and partial-write boundary.
The exact MC6801 `STS` rows remain unstaged.
Direct `STD` follows the same two-byte page-zero write shape with A as the high
byte and B as the low byte. It fetches the destination before requiring A and B
to be known, checks A before B, derives N/Z/V from the unchanged 16-bit A:B
value, and does not commit CPU state until both writes succeed. A successful
first write remains visible if the second write fails. Indexed `STD` first
fetches one unsigned displacement and requires X before A and B. It writes A
then B at the low 16 bits of `X + displacement` and the following 16-bit
address, with the same flags, knownness, and partial-write boundary. The
published restart-vector read and next-opcode prefetch are not architectural
trace events. Extended `STD` instead fetches a big-endian 16-bit destination,
then applies the same A/B knownness, ordered writes, flags, and partial-write
boundary. The MC6801 `STD` rows remain unstaged.
Immediate `CPX` fetches its 16-bit operand high byte first, requires X to be
known, and computes the flags for `X - operand` without changing X. It makes
N/Z/V/C known, treats C as unsigned borrow, and preserves H/I values and
knownness. Direct `CPX` fetches its page-zero address and reads the 16-bit
operand high byte then low byte before requiring X. It applies the same flag
and preservation rules in four cycles, advances PC by two with 16-bit wrap,
and commits no CPU state on a fetch, read, or knownness failure. The exact
MC6801 rows were left unstaged. Indexed `CPX` fetches its unsigned
displacement, requires X, then reads the 16-bit operand high byte followed by
the low byte at the wrapped `X + displacement` address. It compares the
unchanged X in five cycles, wraps the effective and following addresses to 16
bits, and commits no CPU state on a fetch, read, or knownness failure. Extended
`CPX` fetches a big-endian 16-bit address, reads the operand high byte then low
byte, and only then requires X. It compares the unchanged X in five cycles,
advances PC by three with 16-bit wrap, and commits no CPU state on a fetch,
read, or knownness failure. The exact MC6801 rows remain unstaged.
Immediate `CMPA` fetches its 8-bit operand before requiring A to be known and
computes the flags for `A - operand` without changing A. It makes N/Z/V/C
known, treats C as unsigned borrow, and preserves H/I values and knownness.
Direct `CMPA` fetches its page-zero address, reads the operand, then requires A
to be known. It uses the same subtraction flags without changing A or memory.
An address-fetch fault, data-read fault, or unresolved A commits no CPU state.
The MC6801 direct row and HD6301V1 indexed and extended rows were unstaged when
the direct form was added. Indexed `CMPA` fetches its unsigned displacement,
requires X, reads the selected byte, and then requires A. It preserves A, X,
and memory while applying the same flags in four cycles with 16-bit effective-
address and PC wrap. Extended `CMPA` fetches a big-endian 16-bit address, reads
the selected byte, and then requires A. It preserves A and memory, advances PC
by three with 16-bit wrap, and applies the same flags in four cycles. Failed
address fetches, a failed data read, or unresolved A commit no CPU state. The
exact MC6801 rows remain unstaged pending separate exact-profile review.
Immediate `CMPB` uses the same operand-fetch and flag boundary for
`B - operand`, requires B rather than A, and leaves both accumulators
unchanged. Direct `CMPB` first fetches its page-zero address and reads the
operand, then applies the same comparison without changing B or memory. An
address-fetch fault, data-read fault, or unresolved B commits no CPU state.
Indexed `CMPB` fetches its unsigned displacement, requires X, reads the byte at
the low 16 bits of `X + displacement`, and then requires B. It leaves B, X,
and memory unchanged while applying the same flags in four cycles. Failed
fetches or reads and unresolved X or B commit no CPU state. Extended `CMPB`
fetches a big-endian 16-bit address, reads the selected byte, and then requires
B. It leaves B and memory unchanged, advances PC by three with 16-bit wrap,
and applies the same flags in four cycles. Failed address fetches, a failed
data read, or unresolved B commit no CPU state. The exact MC6801 rows remain
unstaged pending separate exact-profile review.
Immediate `SBCA` fetches its operand before requiring A and the old C bit to be
known, in that order. It replaces A with the low eight bits of
`A - operand - oldC`, makes N/Z/V/C known, treats C as the unsigned borrow from
the full nine-bit subtrahend, and preserves B plus H/I values and knownness.
Direct `SBCA` fetches its page-zero address and reads the selected byte before
requiring A and then the old C bit. It applies the same result, flags, and
preservation rules in three cycles. Address-fetch, data-read, and knownness
failures commit no CPU state. Indexed `SBCA` fetches its unsigned displacement,
requires X, reads the byte at the low 16 bits of `X + displacement`, and then
requires A followed by old C. It applies the same result, flags, and
preservation rules in four cycles while preserving X and memory. Failed
fetches or reads and unresolved X, A, or C commit no CPU state. Extended
`SBCA` fetches a big-endian 16-bit address, reads the selected byte,
and then requires A followed by old C. It applies the same result, flags, and
preservation rules in four cycles and advances PC by three with 16-bit wrap.
Failed address fetches, a failed data read, or unresolved A or C commit no CPU
state. The exact MC6801 rows remain unstaged pending separate exact-profile
review.
Immediate `SBCB` fetches its operand before requiring B and the old C bit to be
known, in that order. It replaces B with the low eight bits of
`B - operand - oldC`, makes N/Z/V/C known, derives C from the full nine-bit
subtrahend, and preserves A plus H/I values and knownness. Operand-fetch and
knownness failures commit no CPU state. Direct `SBCB` fetches its page-zero
address and reads the selected byte before requiring B and then the old C bit.
It applies the same result, flags, and preservation rules in three cycles.
Indexed `SBCB` fetches its unsigned displacement, requires X, reads the byte at
the low 16 bits of `X + displacement`, and then requires B followed by old C.
It applies the same result, flags, and preservation rules in four cycles.
Extended `SBCB` fetches a big-endian 16-bit address, reads the selected byte,
and then requires B followed by old C. It applies the same result, flags, and
preservation rules in four cycles and advances PC by three with 16-bit wrap.
Fetch, data-read, and knownness failures commit no CPU state. The exact MC6801
rows remain unstaged pending separate review.
Immediate `SUBA` fetches its operand before requiring A to be known. It
replaces A with the low eight bits of `A - operand`, makes N/Z/V/C known with C
as unsigned borrow, and preserves B plus H/I values and knownness. Direct
`SUBA` fetches one page-zero address and reads the selected byte before
requiring A. It applies the same result, flag, preservation, and non-commit
rules in three cycles. Indexed `SUBA` fetches its unsigned displacement before
requiring X, reads the selected byte, and only then requires A. It preserves X
and memory, applies the same subtraction rules in four cycles, and wraps the
effective address to 16 bits. Extended `SUBA` fetches a big-endian 16-bit
address, reads the selected byte, and only then requires A. It applies the same
subtraction rules in four cycles and advances PC by three bytes with 16-bit
wrap. The exact MC6801 rows remain unstaged pending separate exact-profile
review.
Immediate `SUBB` applies the two-cycle immediate-fetch and subtraction rules to
B, preserving A plus H/I values and knownness. It commits only after the
operand fetch and B-knownness check succeed. The MC6801 row remains unstaged.
Direct `SUBB` follows the same address/data read order and subtraction rules
using B as the destination. It preserves A plus H/I values and knownness, and
commits B, N/Z/V/C, PC, and three cycles only after both reads and the B
knownness check succeed. The MC6801 row and HD6301V1 indexed and extended rows
were unstaged when the direct form was added. Indexed `SUBB` fetches its
unsigned displacement before requiring X, reads the selected byte, and only
then requires B. It preserves X and memory, applies the same subtraction rules
in four cycles, and wraps the effective address to 16 bits. Extended `SUBB`
fetches a big-endian 16-bit address, reads the selected byte, and only then
requires B. It applies the same subtraction rules in four cycles and advances
PC by three bytes with 16-bit wrap. The exact MC6801 rows remain unstaged
pending separate exact-profile review.
HD6301V1 `BRN` fetches and records its arbitrary second byte but never applies
it: final PC is always `P + 2`. It consumes no register or CCR value, preserves
all value and knownness state, and therefore executes from reset-time partial
state. A failed second-byte fetch commits no CPU state. The MC6801 row remains
unstaged pending separate exact-profile review.
HD6301V1 `BHI` fetches its signed displacement before requiring both C and Z.
It branches from `P + 2` exactly when both known bits are zero and otherwise
advances to `P + 2`. Either unresolved bit reports `unknown_state` without
committing CPU state. Both paths preserve every register and CCR value and
knownness bit. The MC6801 row remains unstaged pending separate exact-profile
review.
HD6301V1 `BLS` shares BHI's fetch and knownness boundary but takes the
complementary path: it branches when either known C or Z is one and advances
when both are zero. It uses the same three cycles and preserves all register
and CCR value and knownness state. The MC6801 row remains unstaged pending
separate exact-profile review.
`BCC` fetches its signed displacement before testing C. It branches from
`P + 2` when the known C bit is zero, advances to `P + 2` when C is one, and
reports `unknown_state` without committing CPU state when C is unresolved.
`BCS` uses the same fetch and fault boundary but takes the branch when the
known C bit is one and advances when C is zero. Both paths preserve every CCR
value and knownness bit.
`BNE` fetches its signed displacement before testing Z. It branches from
`P + 2` when the known Z bit is zero, advances to `P + 2` when Z is one, and
reports `unknown_state` without committing CPU state when Z is unresolved.
`BEQ` uses the same fetch and fault boundary but takes the branch when the
known Z bit is one and advances when Z is zero. Both paths preserve every CCR
value and knownness bit.
HD6301V1 `BVC` fetches its signed displacement before testing V. It branches
from `P + 2` when the known V bit is zero, advances when V is one, and reports
`unknown_state` without committing CPU state when V is unresolved. Both paths
take three cycles and preserve every register and CCR value and knownness bit.
The MC6801 row remains unstaged pending separate exact-profile review.
HD6301V1 `BVS` shares BVC's fetch, knownness, timing, and preservation boundary
but takes the complementary path: it branches when known V is one and advances
when V is zero. The MC6801 row remains unstaged pending separate exact-profile
review.
`BPL` follows the same relative-address and fault boundary while testing N:
it branches when the known N bit is zero, advances when N is one, and rejects
an unresolved N after fetching the displacement.
`BMI` shares that boundary and takes the complementary path: it branches when
the known N bit is one and advances when N is zero. Both paths preserve every
register and CCR value and knownness bit.
HD6301V1 `BGE` fetches its signed displacement before requiring both N and V.
It branches when the two known bits are equal, advances when they differ, and
reports `unknown_state` without committing CPU state if either bit is
unresolved. Both paths take three cycles and preserve every register and CCR
value and knownness bit. The MC6801 row remains unstaged pending separate
exact-profile review.
HD6301V1 `BLT` shares BGE's operand, knownness, timing, preservation, and fault
boundaries but takes the complementary path: it branches when known N and V
differ and advances when they are equal. The MC6801 row remains unstaged pending
separate exact-profile review.
HD6301V1 `BGT` additionally requires Z to be known after its displacement
fetch. It branches only when Z is zero and N equals V, otherwise advances, and
preserves every register and CCR value and knownness bit. The MC6801 row remains
unstaged pending separate exact-profile review.
HD6301V1 `BLE` shares BGT's operand, N/Z/V knownness, timing, preservation, and
fault boundaries but takes the complementary path: it branches when Z is one or
N differs from V. The MC6801 row remains unstaged pending separate exact-profile
review.
`OIM` replaces its direct or indexed memory operand with the bitwise OR of the
old value and immediate byte. The indexed effective address is the low 16 bits
of `X + displacement`, where the encoded displacement is unsigned. It sets N/Z,
clears V, and preserves H/I/C.
Direct or indexed `AIM` replaces its memory operand with the bitwise AND of the
old value and immediate byte; direct or indexed `EIM` applies exclusive OR
instead. Each fetches both encoded operands before the data read, writes the
result back to the same address, sets N/Z, clears V, and preserves H/I/C. The
indexed forms require X after both operand fetches and use the low 16 bits of
`X + unsigned displacement`; direct forms consume no CPU register value. Fetch,
unknown-X, read, or rejected-write failure leaves CPU state and cycles
uncommitted; a rejected write retains the preceding successful read event.
Direct `TIM` reads one page-zero byte, while indexed `TIM` reads the byte at the
low 16 bits of `X + unsigned displacement`. Both derive N/Z from the bitwise
AND with the immediate byte, clear V, preserve H/I/C, and leave the memory byte
and every CPU register unchanged. Both encoded operands precede the data read;
the indexed form requires X only after those fetches, and any destination-read
failure commits no CPU state.
Indexed and extended `TST` read one byte without changing memory or any CPU
register. N/Z reflect the byte, V/C are cleared, and H/I are preserved. The
indexed architectural order is opcode, unsigned displacement, then data read;
the extended order is opcode, address high, address low, then data read. The
published indexed restart-vector dummy read and both next-opcode prefetches are
omitted.
HD6301V1 `TSTA` and `TSTB` require their selected accumulator to be known,
derive N/Z from it, clear V/C, and preserve every register plus H/I values and
knownness. An unresolved A or B is reported after the opcode fetch without
committing CPU state. The MC6801 rows remain unstaged pending separate
exact-profile review.
Indexed `CLR` fetches one unsigned displacement, requires X to be known, reads
the byte at the low 16 bits of `X + displacement`, then writes zero to the same
address. The explicit read is retained for architectural trace ordering, but
its byte is not consumed when computing the result. An uninitialized modeled
RAM destination can therefore emit a successful unknown-value read before the
zero write without inventing its power-on byte. It clears N/V/C, sets Z, and
preserves H/I. Extended `CLR` fetches a big-endian 16-bit destination and then
uses the same value-discarding read, zero write, flag, and fail-closed boundary
without consuming any CPU register. The MC6801 rows remain unstaged.

## Architectural access trace

Every opcode byte, operand byte, data read, and data write performed by the CPU
passes through `Bus`. Events preserve the meaningful access order within an
instruction and identify instruction fetches, data reads, and data writes.
`value_known` distinguishes a known transferred byte from a successful read
whose byte is deliberately unused and unresolved. `previous_value_known` does
the same for the byte replaced by a write.

This is an **architectural access trace**, not a pin-cycle trace. Event sequence
numbers order calls to `Bus`; they are not physical CPU cycle numbers. In
particular, this version does not synthesize dummy reads, speculative fetches,
or the HD6301V1 pipeline's next-opcode fetch. The manufacturer-derived boundary
and future requirements are recorded in
`docs/research/hd6301-execution-semantics.md`.

Successful instructions add the exact profile metadata's total cycle count.
An opcode or operation fault retains every successful access before the fault
but does not add a successful instruction cycle total.

Trace filtering is a read-only debugger query over the retained ring. An
inclusive 16-bit address range and any nonempty combination of instruction
fetch, data read, and data write select records without changing capture,
sequence numbers, or the unfiltered access counts stored in instruction
history. Changing the filter therefore requires no re-execution, and gaps in a
filtered sequence remain visible rather than being renumbered.

## JR-800 fault-aware bus boundary

`Jr800Bus` adapts the evidence-bounded `Jr800Memory` map to the same CPU
Interface used by the synthetic `RamBus`. It distinguishes missing logical ROM,
uninitialized RAM, unsupported addresses, and writes to read-only ROM. A failed
bus access does not emit a successful `BusAccessEvent`, advance PC, change
registers or flags, or add cycles. `StepResult` retains the bus-fault reason,
address, and access kind; an operand fault retains only the bytes successfully
fetched before it.

CPU register, flag, PC, SP, and cycle updates are committed only after every
required access succeeds. A later E-354 bus-advance fault is a distinct
post-step event: the CPU state and cycle total remain committed while the bus
reports that its atomic device-time batch was rejected. Earlier successful
memory accesses are not rolled
back when a later access faults: for example, if `BSR` writes the return low
byte successfully and its following high-byte write is rejected, the first
write remains visible while CPU state stays at the instruction boundary. This
preserves ordered bus side effects and makes retry behavior explicit without
claiming a hardware exception mechanism.

Successful first writes to RAM are permitted because they do not depend on a
power-on value. Their structured event marks `previous_value_known` false. This
prevents the zero-filled C++ storage placeholder from becoming an emulator
claim about physical power-on RAM.

`Bus::read8_discard` is the narrower contract for a documented read whose byte
does not affect instruction state. `Jr800Bus` accepts an unresolved result only
for modeled uninitialized RAM and emits `value_known` false without marking the
RAM initialized. Unknown device inputs, disabled internal RAM, unsupported
regions, and unavailable ROM remain faults and emit no successful event.

The CPU-device layer accepts writes to the mode-6 HD6301V1 Port 1 data direction
register at `$0000`, Port 2 data direction register at `$0001`, Port 1 data
register at `$0002`, Port 2 data/mode register at `$0003`, and Port 4 data
direction register at `$0005`.
`Hd6301v1Ports` clears the three direction registers on reset, matching the
documented input state. Port 2 has five I/O lines, so only the lower five
direction bits are retained. The device retains written Port 1 data and the
lower five Port 2 data-latch bits, but treats both reset contents as unknown.
Port 1 reads return only an explicitly supplied eight-bit pin state. Port 2
reads combine the reset-latched mode-6 value `$C0` in bits 7-5 with explicitly
supplied pin state in bits 4-0. A per-bit known mask must cover every pin bit
required by the register or the read reports `uninitialized_read`. Pin state is
external to the CPU, persists across CPU-device reset, and is never synthesized
from an output latch. Port 2 writes ignore the read-only upper mode bits. Reads
of the three write-only direction registers remain unsupported. Pin output,
Port 2 timer/serial behavior, external address routing, and partial-decode
effects are not modeled by this register slice.

The timer slice accepts writes to the HD6301V1 timer control/status register at
`$0008`. `Hd6301v1Timer` retains only the documented writable lower five control
bits and clears them on reset; writes cannot alter the upper three read-only
status flags.

The slice also models the documented paired write to the 16-bit free-running
counter at `$0009:$000A`. A high-byte write stages that byte and immediately
presets the counter model to `$FFF8`; the following low-byte write commits both
staged high and supplied low bytes. Reset clears the counter and pending pair.
A low-byte write without a staged high byte remains `unsupported_access` rather
than assuming an undocumented latch value.

Every `Bus` implementation participates in emulated time through the mandatory
`advance_cycles` contract. `Cpu::step_instruction` invokes it with the exact
documented total only after an instruction has committed successfully; a fault
does not advance either the CPU cycle total or attached devices. `RamBus` has no
timed devices and consumes the notification as a no-op, while `Jr800Bus`
advances the FRC once per reported E cycle with 16-bit wrap. The FRC high and
low bytes are therefore readable and inspectable at architectural instruction
boundaries, and their write traces have exact previous bytes. Register effects
within one instruction remain atomic in this execution model; this is not a
claim of the manufacturer's pipelined pin-cycle schedule.

The output compare register at `$000B:$000C` resets to `$FFFF` and supports
independent high- and low-byte reads and writes. Its retained bytes are exact,
so output-compare write traces include the previous byte. Each uninhibited FRC
increment that reaches OCR sets OCF. An OCR-high or paired FRC write starts the
documented two-cycle comparison-inhibit interval in the architectural timer
model. An FRC wrap to `$0000` sets TOF.

TCSR reads combine those status flags with the five retained control bits. A
read with TOF set arms the documented clear sequence; only a following FRC-high
read clears it. A read with OCF set similarly requires a following write to
either OCR byte. Inspection is side-effect free and does not arm either
sequence. TCSR writes preserve all status bits and now expose the exact previous
full byte in structured traces.

Port 2 bit 0 input changes supplied explicitly by the host feed the timer edge
detector only while the corresponding direction bit selects input. IEDG chooses
falling or rising capture. A qualified transition stores the current FRC in the
read-only ICR at `$000D:$000E` and sets ICF; TCSR read with ICF set followed by
an ICR-high read clears only ICF. Before the first qualified transition, ICR is
unknown and reads report `uninitialized_read`. Host transitions are sampled
architectural input events; the model does not claim the physical response to a
sub-two-E-cycle pulse.

Reset makes ICF known-clear, but an unspecified Port 2 bit 0 level can no longer
support that claim after emulated time advances while capture input is enabled.
At that point full TCSR and ICR reads report `uninitialized_read`. Supplying a
known baseline before CPU-device reset preserves the external level across the
reset and allows subsequent status to remain exact; a confirmed qualifying edge
also makes ICF and the captured ICR value known-set.

The timer exposes a prioritized internal interrupt request only when a status
flag and its matching enable bit are both set. Input capture has priority over
output compare, which has priority over overflow. An enabled unresolved ICF is
reported as an unknown request unless ICF is already known-set; a known lower
priority flag cannot bypass that uncertainty. Merely setting an enable bit does
not manufacture a timer request. Input pin synchronization within an
instruction and the OLVL transfer to Port 2 bit 1 remain unimplemented.

The SCI slice accepts writes to the HD6301V1 rate/mode control register at
`$0010`. It retains the documented four bits (`SS0`, `SS1`, `CC0`, and `CC1`),
clears them on reset, and ignores the upper four data-bus bits. The register is
write-only, so reads and inspection remain `unsupported_access`. Its retained
reset and previous values are exact for tracing, but no baud timing, internal or
external serial clock, data format, or Port 2 bit 2 clock function is produced.

The same slice accepts writes to the transmit/receive control/status register at
`$0011`. It retains only the documented writable lower five control bits (`WU`,
`TE`, `TIE`, `RE`, and `RIE`) and clears them on reset; writes cannot alter the
upper hardware status flags. Reset exposes the documented full value `$20`:
`TDRE` is known-set and `ORFE`/`RDRF` are known-clear. Because TDR writes remain
unsupported, `TDRE` stays set in this slice. Reads, discarded reads, and
inspection return the full byte while that status remains known; normal reads
and discarded reads produce structured read events, while inspection does not.
TRCSR writes likewise report a known previous full byte when available.

The receiver uses Port 2 bit 3 only when the host explicitly supplies that pin
as known. A known-high input with `RE` set and `WU` clear is a deterministic
idle line, so `ORFE` and `RDRF` remain known-clear. If `RE` is enabled while the
pin is unknown or low, or a known-high enabled input later becomes unknown or
low, the full status remains exact only through the documented minimum
ten-bit receive interval. The fastest internal or permitted external bit rate
is E/16, so after 160 E cycles the unmodeled receive outcome makes the full
TRCSR read fail with `uninitialized_read`. A known-high receiver with `WU` set
uses the same 160-cycle lower bound before the unmodeled hardware WU clear can
occur. These conservative transitions never infer an idle JR-800 wire level;
once receive status becomes unknown, only CPU-device reset restores the known
reset flags.

The SCI request is known asserted when `TIE` is set because `TDRE` is still
known-set. With `TIE` clear, `RIE` reports known inactive while `ORFE`/`RDRF`
are known-clear and reports unknown after receive status becomes unresolved.
A known transmit request dominates unresolved receive flags. Baud progression,
TDR/RDR access, TRCSR-to-TDR/RDR flag-clear sequences, hardware WU clearing,
data transfer, serial clock sampling, and Port 2 output overrides remain
unimplemented. TRCSR reads therefore have no clear-sequence side effect yet.

`Bus::maskable_interrupt_request` returns a prioritized source together with
explicit request knownness. `Jr800Bus` evaluates the three timer sources before
the lower-priority SCI source. `RamBus` defaults to a known inactive request and
allows a synthetic request to be supplied for deterministic tests.

E-172 samples that boundary while the HD6301V1 CPU is sleeping. Any known
maskable request releases sleep. If CCR I is set, the CPU resumes at the
instruction following SLP without vectoring or adding cycles. If I is clear,
the interrupt entry writes PC low, PC high, X low, X high, A, B, and the
pre-interrupt CCR to descending stack addresses, then reads the selected vector
MSB and LSB. ICF uses `$FFF6:$FFF7`, OCF `$FFF4:$FFF5`, TOF `$FFF2:$FFF3`, and
SCI `$FFF0:$FFF1`. The final SP is seven less than its prior value, PC takes the
big-endian vector, CCR I is set, and the CPU returns to `active` after 12 E
cycles.

E-262 also samples this boundary while the CPU is
`waiting_for_interrupt`. A known asserted internal request is accepted only
when CCR I is clear. Because WAI has already stored the complete frame, the
accepted path performs only the selected vector's two reads, sets I, and
returns to `active`; SP and the stored frame remain unchanged. A request masked
by I leaves the machine dormant. Unknown request or I state fails visibly.

E-292 samples the same prioritized internal request at every active instruction
boundary. A known asserted request with known-clear I enters the interrupt
before any opcode fetch, stores the current PC in the shared seven-byte frame,
reads the selected vector, sets I, and accounts the same 12 E cycles. A set I
bit permits ordinary execution even if request state is unknown. With I clear,
unknown request state fails explicitly; with an asserted request, unknown I
also fails explicitly. The two-cycle `CLI`/`TAP` delay described above is
decremented only by successfully completed following instructions. Normal
sampling resumes immediately after successful `RTI` unless that restored I bit
is set.

Interrupt entry and masked sleep release are distinct `StepKind` values. They
create structured execution-history entries, while the debugger counts neither
as an executed instruction. A run can therefore enter an interrupt and still
stop at a breakpoint on the first handler instruction without consuming its
instruction budget. The architectural bus trace contains the seven stack
writes followed by two vector reads. It omits the timing diagram's dummy reads
and first handler-opcode fetch while retaining the full 12-cycle total, matching
the existing trace policy for documented prefetch cycles. Unknown request
state, unknown CCR I, or any unknown byte needed for the stack fails visibly;
CPU state and cycle count remain at the sleeping boundary. Successful earlier
bus writes remain visible if a later stack or vector access fails.

HD6301V1 `RTI` reverses the seven-byte interrupt frame. It requires a known SP,
then reads CCR, B, A, X high, X low, PC high, and PC low from `SP+1` through
`SP+7` in that order. All reads must succeed before CPU state commits. Success
restores the six writable CCR bits while forcing the two fixed upper bits,
restores B, A, X, and PC, advances SP by seven, makes those restored fields
known, returns the execution state to `active`, and accounts ten cycles. The
architectural trace contains the opcode fetch and seven stack reads; it omits
the published restart-vector dummy read, sequential prefetch, and first
return-target opcode fetch. Its debugger classification is `interrupt_return`,
not `subroutine_return`, and step-over remains disabled.

IRQ1, NMI, TRAP, RES/STBY wake, automatic selection of a dormant duration, and
wall-clock pacing remain unstaged. `advance_suspended_cycles` provides only
explicit bounded E-cycle progression while the CPU is already suspended.

The RAM-control slice models the HD6301V1 register at `$0014`. Reset sets its
RAM-enable bit, its six unused bits read as their documented initialized ones,
and writes affect only the standby and RAM-enable bits. The standby bit has no
machine-wide default because it reports the preceding power-retention
condition; a caller must supply its value and knownness, or a register read
reports `uninitialized_read`. This external status persists across CPU reset.
When RAM enable is cleared, `$0080-$00FF` becomes an external address range on
the processor. That external fallback is not yet decoded, so `Jr800Bus` reports
`unsupported_access` instead of continuing to use the retained internal-RAM
backing. Re-enabling RAM makes the retained internal contents accessible again.

The JR-800 keyboard boundary handles reads throughout the verified
`$0C00-$0FFF` selection window only when the host supplies a complete raw byte
for the exact logical address. Each address starts unknown and reports
`uninitialized_read` rather than an invented idle value. Inspection is
side-effect free, successful CPU and discarded reads emit one structured read
event, and writes remain unsupported. Raw responses persist across CPU-device
reset because they represent external input. This boundary does not assign
keys, matrix rows or columns, modifier behavior, address aliases, or active
polarity. The keyboard component separately counts CPU and discarded read
attempts plus the number of distinct selected addresses since its activity was
cleared. Inspection, writes, and out-of-window reads do not contribute. CPU
device reset clears only these diagnostic aggregates while preserving the
external response values. The headless runner also clears them immediately
before execution and returns only the two totals, never the addresses, values,
ordering, or per-address counts. ABI 24 transports the same pair as two 64-bit
values. JR-800 Worker runs clear the interval before their first bounded C++
call, and browser snapshots display only those aggregates.

The E-293 LCD board attachment is opt-in. A default `Jr800Bus` continues to
report `$0A00-$0BFF` as unsupported. A bus constructed with an experimental
machine configuration containing `Jr800ExperimentalLcdConfiguration` routes
only the E-184 one-hot candidate selects to the eight-controller E-185
composite. CPU-device reset asserts and releases all eight controller reset
inputs. Every accepted control or data operation completes its device-local
busy period before the bus access returns.
The configuration also supplies the byte returned to the CPU when an accepted
display-data read encounters the HD44102's initially unknown output register;
the internal register itself remains unknown until the documented pipeline
loads it. These are bounded experiment inputs, not verified board timing or
electrical behavior. Unsupported select patterns and instructions still fault,
partially known status reads fail as uninitialized, and side-effect-free
inspection rejects the invasive display-data register. Controller state,
display RAM, provisional panel dots, and raw indicator RAM remain available
through read-only optional machine views. A read-only counter starts at zero on
CPU-device reset and increments only when a successful display-data read
actually returns the configured substitute, so the experiment does not hide
its dependence on that input.

E-294 independently adds `Jr800ExperimentalMemoryConfiguration`. Its required
byte marks the complete E-008 standard-RAM range known at machine construction.
Its optional expansion byte attaches equally initialized storage to the E-009
logical expansion range; omission retains the default unsupported result.
Configured RAM uses the ordinary read, write, discarded-read, inspection, and
trace paths. CPU-device reset does not refill either region, so program writes
survive reset. These caller-selected bytes are experiment state, not claimed
power-on values, and attaching expansion storage does not close U-013's
physical population or chip-select questions.

E-336 adds an independent `Jr800ExperimentalInternalRamConfiguration`. Its one
caller-selected byte marks all 128 CPU-internal-RAM locations known only at
machine construction. Omission retains byte-level unknownness, and CPU-device
reset neither refills nor invalidates the region. The direct memory model,
Native runner, C ABI 21, module Worker, and browser all use the same opt-in
boundary. A uniform test byte is an execution input, not a power-on or retained-
RAM claim.

E-337 adds `Jr800ExperimentalResetStateConfiguration` as a separate input to
the JR-800 machine. Optional SP, X, A, and B values make only their complete
registers known. Optional H/N/Z/V/C values make only their selected CCR bits
known, while PC still comes from the reset vector and the fixed high bits and I
retain their verified reset values. The same configuration is reapplied on
machine reset. Omission leaves every unresolved field unknown, so this enables
bounded boot experiments without turning a hypothesis into reset behavior.

E-295 independently adds `Jr800ExperimentalCalendarConfiguration`. It routes
the verified `$0600-$07FF` selection window to the E-065 device-local RP5C01
register file only when explicitly present. The register file begins as known
zero in all four banks and the mode register and remains static except for CPU
writes. The address-source enum chooses CPU A0-A3, A1-A4, A2-A5, A3-A6, A4-A7,
or A5-A8 as the device's four local address bits. A second enum supplies either
all-zero or all-one CPU read bits 7-4; the RP5C01 continues to supply only its
low nibble. Device masks, bank selection, invalid-BCD rejection, write-only
readback, prior-value knownness, discarded reads, and structured traces remain
those of the shared register model. CPU-device reset preserves this retained
state, and cycle advancement does not tick it. Invalid configuration values
and unsupported device operations fail visibly. These constraints reproduce a
bounded execution hypothesis without resolving U-016's wiring, live counter,
alarm, reset control, or oscillator behavior.

E-339 deepens the shared RP5C01 register module without selecting a JR-800
wiring hypothesis. A write to device address `F` now follows Ricoh's documented
bit contract: D0 clears MODE 01 alarm-time addresses `2-8`, D1 is accepted as
the subsecond-divider reset command, and D2/D3 set retained active-low 16 Hz and
1 Hz output-enable state. The write-only readback and prior-value trace remain
zero. At E-339 the static-clock layer had no divider phase, alarm flip-flop,
alarm comparison, or output waveform, so D1 had no retained effect and the
enable state did not manufacture pulses. E-348 later adds only the divider
phase; the other dynamic behaviors stay open under U-016.

E-347 adds a device-local one-second boundary without selecting a JR-800 clock
ratio. `Rp5c01RegisterFile::advance_one_second` observes the retained Timer EN
bit, advances the BCD time and calendar through 12- or 24-hour transitions,
applies the documented February leap rule, and carries the Mod-7 week and
Mod-100 year counters. It reads and writes retained banks directly, so the
currently selected CPU-visible bank does not affect counting. Any unknown or
impossible state needed for the next carry returns a status without committing
partial register changes, while independent unknown week/year/leap counters
remain unknown and do not block determined fields. `Jr800Bus::advance_cycles`
does not call this method; the oscillator phase, divider, Clock Hold behavior,
and conversion from CPU cycles remain separate unresolved work.

E-348 adds the exact RP5C01 oscillator-divider unit above E-347. A raw register
file begins with unknown divider phase; explicit zero initialization and the D1
reset-controller command establish phase zero. Caller-supplied oscillator ticks
use the documented 32.768 kHz frequency and carry at exactly 32,768 ticks. A
request spanning several seconds operates on a candidate copy and commits only
after every one-second transition succeeds, so a later unknown or impossible
carry cannot retain an earlier partial second or divider remainder. Timer EN
still gates the counter boundary. This does not connect `Jr800Bus` CPU cycles,
or generate 16 Hz/1 Hz output waveforms.

E-351 adds the documented Clock Hold state to that same device-local unit.
One or more one-second boundaries while Timer EN is clear retain exactly one
pending pulse. The next known off-to-on transition applies one counter second
on a candidate copy; an unknown or impossible carry rejects the mode write and
preserves both the stopped mode and pending pulse. Successful release blocks
time-bank reads for four further explicit oscillator ticks. That conservative
whole-tick guard is at least the manufacturer's 100-microsecond no-read window
and avoids selecting an undocumented update instant inside it. This state is
shared automatically by the existing bus and ABI 24 explicit-tick path; CPU
cycles still do not drive it.

E-352 adds the device-local Alarm Comparator OUT without claiming a JR-800 pin
connection. Four retained comparison-enable states represent the documented
minute, hour, day-of-week, and date groups. Address-`F` D0 clears their stored
digits and resets every group to the manufacturer's always-matching state; a
successful write to any digit activates comparison of its complete group.
The computed result is true, false, or unknown: any determined mismatch is
false, while a possible match remains unknown if an enabled group lacks a
clock or alarm digit. Timer advancement therefore changes the combinational
result automatically. Alarm EN, clock-waveform mixing, open-drain pin state,
and JR-800 wiring remain outside this layer.

E-353 adds a qualified device-local `ADJ` operation without modeling the
external pin level. It clears seconds `00-29`, or clears seconds `30-59` and
reuses the normal atomic carry to increment minutes and any required
hour/date/year counters. The dedicated pin behavior is independent of Timer
EN, but a pending or still-releasing Clock Hold pulse is rejected because the
manufacturer does not define that overlap. Unknown seconds or a required
unknown/invalid carry also leave the complete register file unchanged. The
method represents one already-qualified high interval of at least 100
microseconds; pin wiring, level sampling, repetition, and divider-phase effects
remain outside the model.

E-354 makes `Bus::advance_cycles` a checked operation before any fallible
device clock is attached to it. A non-none result after an instruction or
interrupt entry becomes `CpuFault::bus_advance` with the exact `BusFault` and
`step_completed` still true. The CPU's architectural state and cycle count have
already committed, so debuggers retain the post-step state and hosts count a
completed instruction exactly once. The bus must reject a failing batch before
changing any of its devices. Suspended advancement checks each cycle first and
increments neither the CPU counter nor `cycles_elapsed` for a rejected cycle.
Its result carries the same bus fault without creating instruction history or
memory-access trace. The two device-state fault classes distinguish unknown
retained state from a known but unsupported state; no production bus produces
either class until an explicit device-clock experiment is added.

E-355 optionally connects `Jr800Bus` CPU cycles to its attached calendar using
only the named E-030 nominal clock. The conversion keeps a numerator remainder
in the range zero through 74 and calculates `ticks = (remainder + cycles * 2) /
75`; the new remainder is the corresponding modulus. Thus 1,228,799 cycles
remain below one modeled RTC second and the next cycle crosses the exact
32,768-tick divider boundary. The calculation uses 64-bit integer arithmetic,
and its maximum 32-bit cycle request still yields a 32-bit tick count. A
candidate calendar update must succeed before the remainder, SCI, or timer is
committed. A failed boundary can therefore be corrected and retried without
losing a tick or advancing the FRC. Explicit oscillator-tick calls remain
available and independent; CPU-device reset preserves both RTC state and the
CPU-to-oscillator remainder.

E-356 makes that one opt-in choice available to hosts without widening the
clock model. Native `jr8run jr800` accepts
`--calendar-cpu-cycle-ratio e030-nominal-1.2288mhz` only when both calendar
adapter options are present. C ABI 26 adds one configuration word whose zero
value preserves explicit ticks only and whose sole nonzero value selects the
same E-030 nominal clock. The JavaScript adapter uses the exact corresponding
string and the Worker forwards it only during transactional session creation.
Unknown values and a nonzero ratio on a disconnected calendar fail before a
machine is created. Boundary tests prime 32,767 explicit oscillator ticks and
show that the six setup cycles plus 31 one-cycle NOPs remain below one second,
while the next NOP crosses it through both Native C and Worker paths. No host
performs its own conversion or schedules calendar time from wall time.

E-357 exposes the E-356 JavaScript choice in the browser's explicit experimental
configuration. The checkbox is unchecked by default, spells out `E-030
nominal 1.2288 MHz CPU E clock`, and remains disabled until the calendar
experiment is attached. Detaching the calendar clears the checkbox before a
configuration is built. A checked control supplies the fixed E-356 string
during the existing transactional JR8ROM load; it does not mutate a loaded
machine or create an automatic host-time scheduler.

E-376 renames the single selectable ratio after E-030 is promoted from an
assumption to a verified nominal frequency. C++, Native, C ABI 29, JavaScript,
the Worker, and the browser use only the nominal name. The old host string is
rejected, and no compatibility alias is retained. The integer `2/75` execution
rule is unchanged and still denotes nominal design timing rather than an exact
measurement of pin 40.

E-358 models the manufacturer-defined Alarm Comparator OUT and Alarm EN branch
before it reaches the physical terminal. Mode-register D2 gates the E-352
tri-state comparator: a known false comparator or known-disabled gate returns
false, both known true inputs return true, and every remaining possible true
case stays unknown. Reads do not latch or mutate the result. The accessor does
not generate the 16 Hz or 1 Hz inputs, combine the three branches, represent
open-drain voltage, or assert a JR-800 interrupt or port signal.

E-359 derives those internal 16 Hz and 1 Hz inputs from E-348's retained
oscillator-divider phase. Zero-based bit 10 changes every 1,024 oscillator
ticks and bit 14 every 16,384 ticks, producing full periods of 2,048 and
32,768 ticks with 50-percent duty. Unknown phase produces unknown signals.
Phase zero after explicit initialization or D1 reset is represented as logical
false for both; this fixes deterministic software phase without asserting
physical `ALARM` terminal polarity or the documented sub-tick counter-update
delay. D2/D3 output enables are not applied at this layer.

E-360 applies the retained address-`F` D2/D3 output enables to the two divider
signals. The register write has already normalized each active-low control to
an enabled boolean, and a shared three-state AND helper gives false whenever
either input is known false, true only when both are known true, and unknown
otherwise. This preserves a definite disabled output even when divider phase
is unknown. It still does not OR the three branches, represent the open-drain
terminal, or route a signal into the JR-800.

E-361 performs the manufacturer-shown OR across all three gated branches and
returns an optional logical pull-low request. A known true branch dominates
unknown peers, three known false branches release the request, and every other
combination stays unknown. This accessor is combinational and does not mutate
or latch device state. It represents the internal transistor command only,
not the externally pulled-up terminal voltage, analog edge timing, host event,
or JR-800 interrupt/port connection.

E-362 exposes E-361 through a read-only `Jr800Bus` diagnostic. Its result has a
separate `connected` flag and optional `pull_low`, so a missing experimental
calendar cannot be confused with an attached device whose state is unknown.
The default bus returns disconnected; the E-295 calendar experiment returns
the live device-local value. Inspection emits no bus trace and does not feed
the maskable-interrupt arbiter or either HD6301 port.

E-363 adds a direct `Jr800Machine` forwarding accessor for that exact result
type. A default machine therefore preserves disconnected/no-value, and an
explicit calendar machine follows the bus value across oscillator ticks. The
forwarder does not sample on instruction boundaries, cache state, emit trace,
or introduce a runtime/C ABI representation.

E-364 adds the typed state to the immutable bounded run summary. Invalid limits
or an uninitialized machine return the current diagnostic without running;
normal and fault exits resample after the final accepted execution cycle. A
test primes divider phase 1,023 and uses only E-030's named cycle ratio so 38
one-cycle NOPs cross the first 16 Hz edge. Advancing the machine afterward
proves the stored summary is a value snapshot rather than a live alias.

E-365 adds a deterministic native text representation. The single
`calendar-alarm-terminal` field maps disconnected, unknown, known release, and
known pull-low to distinct lowercase tokens. Default and attached-release CLI
tests bind two states; a synthetic two-instruction program writes Mode D2 and
binds `pull-low`. The text does not call the result an interrupt, logical pin
level, or measured voltage.

E-366 appends the same diagnostic to the C machine-state record as one ABI 27
enum word. The mapping first distinguishes a synthetic or detached session,
then preserves the attached calendar's optional pull-low request as unknown,
released, or pull-low. JavaScript converts only those four numeric values to
the same lowercase vocabulary, and Worker snapshots inherit the result through
the existing state query. Native C and Worker tests bind attached release and
pull-low behavior plus detached C state; no polling, event edge, CPU interrupt,
or port state is created.

E-367 renders `state.calendarAlarmTerminal` directly in the browser's current
state panel. It does not derive a boolean, latch a transition, or issue another
Worker request. A nearby caveat identifies the value as an internal RP5C01
pull-down request and keeps physical JR-800 wiring unresolved.

E-368 forwards one caller-qualified calendar adjustment from `Jr800Machine` to
its configured `Jr800Bus` and then to E-353's atomic device operation. A
detached calendar reports `calendar_disconnected`; device unknownness and
unsupported Clock Hold or counter combinations remain distinct. Tests bind
29-to-00 without a minute carry, 30-to-00 with one minute carry, and an invalid
April-end rollback. Neither a successful nor rejected adjustment advances CPU
or divider time or emits a bus-access observation.

E-369 exposes one such adjustment through C ABI 28 and the idle-only Worker
command `adjust-calendar-seconds`. The Worker first validates the requested
readable view, then invokes the C operation, and returns the resulting ordinary
snapshot. Wrong-kind, detached, unknown, unsupported, and running states remain
explicit failures. Success changes only the calendar counters: CPU cycles,
oscillator phase, history, and architectural access records are unchanged.

E-370 connects one browser button to that Worker operation. It is disabled
unless the loaded target is an idle JR-800 session whose calendar experiment
was attached. The browser performs no seconds correction itself; it renders the
returned snapshot and labels the action as software-qualified with no modeled
physical level or pulse.

E-371 extends only the HD6301V1 timer's device-local output-compare state. A
non-inhibited FRC/OCR match copies the current TCSR OLVL bit into a separate
optional output latch at the same modeled E-cycle boundary that sets OCF.
Changing OLVL later does not change the retained output until another match.
Reset leaves the latch unknown because the reviewed reset definition specifies
the TCSR value but no initial output-latch value. This layer does not apply the
Port 2 bit 1 DDR, drive a pin, or claim a JR-800 beeper route.

E-372 composes that optional timer value with the HD6301V1 Port 2 bit 1 data
direction inside the CPU-device boundary. `Hd6301v1Port2TimerOutputState`
separates output-enable from level: input mode is disabled with no level,
output mode preserves an unknown timer latch, and a completed compare exposes
its known low or high value. Port 2 bit 1 cannot act as a normal data output,
so its ordinary data latch is not used. `Jr800Bus` forwards this read-only
state without trace or mutation. No board net, voltage, or audio sink is
selected.

E-373 adds a direct `Jr800Machine` forwarder for E-372. The machine neither
samples nor stores the output: each read returns the bus's current output-enable
and optional level. A synthetic CPU program proves the forwarder preserves the
transition from input, to enabled-unknown, to enabled-high after an actual
FRC/OCR match. No runtime summary, C ABI, Worker, browser, or audio behavior is
added.

E-374 adds one immutable endpoint copy to `Jr800RunSummary`. Invalid-limit and
uninitialized exits capture the current machine view without executing; normal
and fault exits replace it after the last accepted cycle. The Native CLI maps
the two-field state to exactly `disabled`, `unknown`, `low`, or `high`. It does
not retain transitions, expose a waveform, or infer a JR-800 beeper route.

E-375 adds presence-only coverage for those same four states. The run records
the initial view, every post-step view, every post-suspended-advance view, and
the final view into four booleans. The CLI emits the observed names once each
in fixed order. The set exposes no count, order, duration, cycle, instruction,
or address and therefore cannot be interpreted as a waveform.

E-377 adds the current Port 2 bit 1 timer-output state to
`jr800_machine_state`. ABI 30 represents synthetic-session unavailability
separately from the four JR-800 states. C++ remains the sole owner of the
DDR/timer composition; the C boundary maps it once and JavaScript forwards the
fixed vocabulary through every existing Worker snapshot. Native C and Worker
tests bind unavailable, disabled, enabled-unknown, low, and high.

E-378 renders E-377 as one read-only browser value. The field updates through
the existing snapshot renderer and is labelled as an internal logical
diagnostic whose physical JR-800 wiring is unresolved. It neither samples the
state between execution boundaries nor creates transitions, timing, sound, or
a board-routing claim.

E-379 adds the existing experimental LCD substituted-data-read count to C ABI
31 machine snapshots. An explicit validity word separates synthetic and
disconnected JR-800 sessions from an attached LCD with a zero count, while two
adjacent words retain the full reset-scoped 64-bit value. The Worker exposes
`null`, a safe number, or an exact decimal string and does not infer physical
LCD behavior from the count.

E-380 displays that nullable count through the ordinary browser snapshot
renderer. The field remains unavailable without an attached LCD and visibly
states that substitution under the explicit experiment does not validate the
physical decoder or panel composition.

E-381 gives `Jr800Keyboard` the first partial physical-key state derived from
owner-operated read-only `PEEK` observations. `$0DFF`, `$0F7F`, and `$0FFE`
have verified idle `$FF` responses. Sixteen named keys clear exactly one
observed active-low bit while held. An explicit raw bus response, when present,
is the base byte before that single bit is cleared; clearing the raw response
restores the verified idle value at these three selections. Same-address
multiple-key states and all unobserved selections remain unknown. Physical key
state is external input, so CPU-device reset clears diagnostic activity without
releasing held keys.

E-383 adds `SPACE`, `A`, and main-keyboard `1` as ROM-expected key states. Their
selection and bit come from the private E-382 `INKEY$` behavior analysis, but
their selection idle bytes remain physically unverified. A read therefore
remains unknown before, during, and after such a key press unless the host has
explicitly supplied an exact-address raw base. With that base present, the
inferred active-low bit is composed in the same keyboard-owned path as an
observed key. Tests bind raw-base removal and the cross-selection `SHIFT+A`
state without implementing BASIC character conversion in the device.

E-384 adds no keyboard-device behavior. It replaces ABI 31 with ABI 32 and
maps nineteen fixed C key identifiers to the existing core type. The
JavaScript adapter accepts the corresponding physical-key names and the Worker
serializes held/released changes between bounded execution slices. Native C
tests bind validation, every enum value, observed-key reads, and raw-qualified
reads; Worker tests bind idle press/release, rejected inputs, and an active-run
update whose following CPU read sees the changed bit.

E-385 adds a browser-host translation module without changing the machine or
ABI. It uses `KeyboardEvent.code`, not layout-dependent produced text, for the
currently unambiguous bindings. A state object suppresses auto-repeat, counts
multiple host sources for one JR-800 key, and produces deterministic release
transitions for all retained sources. Tests bind all mappings, repeat/release
edges, dual Shift aggregation, focus-loss-style release, and invalid input.
DOM capture policy and event listeners remain outside this slice.

E-386 replaces ABI 32 with ABI 33 without changing the LCD device model. A
fixed sixteen-value C enumeration addresses the E-187 identities, and a
transactional copy returns one two-word known/value record for each identity.
The JavaScript adapter maps those records to a fixed object of byte or `null`
values, which every Worker snapshot carries. Synthetic, unloaded, and
LCD-disconnected sessions remain visibly unavailable; raw values do not imply
physical segment state.

E-387 adds a pure browser presentation transform for fifteen emulator-relevant
identities and keeps its frame visible before target-loading and debugger
controls in main-content order. Before LCD data is available, the same frame
shows explicit unavailable state. It orders PAGE 1 through 8 to the left of the provisional matrix
and seven mode indicators to the right, exposes only unknown or raw diagnostic
values, and omits the physical battery-life position. Tests bind identity,
ordering, battery exclusion, unavailable/unknown/raw states, and the rule that
both zero and nonzero raw bytes remain electrically unresolved. The resulting
frame is functional rather than a reproduction of the product enclosure.
Accessible descriptions expose the owner-reported BASIC roles; the conflicting
CTRL initial-state statements remain unresolved instead of selecting a value.

E-388 changes presentation containment only. Target loading remains available,
and the LCD stays visible and first in main-content order. All execution controls
and debugger inspection panels are wrapped in a closed-by-default native
`details` boundary labeled for developers. Opening it exposes the unchanged
controls; machine, Worker, and snapshot state do not depend on that visual
choice.

E-392 expands only the C++ keyboard device. It maps all 77 physical positions
identified by the manual and private BASIC ROM scanner to one active-low bit
across ten selections. The existing physically verified `$FF` fallback remains
limited to `$0FFE`, `$0F7F`, and `$0DFF`; a mapping on another selection cannot
produce a byte until an exact raw response is supplied. More than one held key
on the same selection remains unknown. Tests exercise press, release, base
qualification, and restoration for every key. No character conversion, C ABI,
Worker, DOM, indicator, or BASIC state is added.

E-393 adds no keyboard-device behavior. ABI 34 replaces ABI 33 and exposes all
77 E-392 identities through the existing boolean key-state operation. The
JavaScript adapter accepts a fixed name for each C identifier, and the Worker
serializes the expanded set at the same complete-slice boundaries. Native and
WASM tests bind the complete accepted range, rejection immediately outside it,
and an end-to-end raw-qualified keypad-divide state. The DOM remains unchanged.

E-394 adds no core or Worker behavior. The browser DOM now has exactly one
button for each of the 77 ABI 34 identities and validates that equality during
startup. Pointer and focused-button actions reuse the existing balanced source
aggregation, modifier latches, slice-boundary Worker command, and focus-loss
release. A loaded JR8ROM enables the full physical surface, while a key whose
selection lacks an explicit raw base retains the C++ device's unknown read.
JavaScript does not contain the address/bit table or convert keys into
characters.

E-395 adds no core execution behavior. Eighty `KeyboardEvent.code` bindings
cover the 77 physical identities; the three extra sources are the left/right
Shift pair, left/right Control pair, and main/numpad Enter pair. Both host and
pointer sources enter the same reference-counted browser state before a
structured Worker transition is sent. Capture is active only for JR8ROM,
excludes interactive or editable page targets, and leaves unknown codes alone.
Host Shift plus the modeled number, letter, direction, or editing position is
therefore a physical chord rather than host-produced character injection.

The same slice adds an explicit browser-session `power-off` boundary for the
manual-documented RESET control. The Worker invalidates any scheduled run slice,
destroys its machine, and rejects later machine commands until a new load
creates a fresh machine. The browser asks for confirmation before requesting
that transition and clears every retained key source afterward. Forced power
off is the documented user-visible RESET behavior; the confirmation step is a
Web safeguard. The implementation does not model the RESET pin's electrical
path, standby, retained RAM, or the power-control circuit.

E-349 carries only that explicit device-local time operation through the
JR-800 bus, machine, C ABI 24, JavaScript adapter, and module Worker. A caller
supplies an unsigned 32-bit oscillator-tick count; the host receives distinct
disconnected, unknown-state, or unsupported-state failure classes. The Worker
requires an idle machine and validates its requested readable view before
advancing. The operation does not change the CPU cycle counter, execute an
instruction, add history or bus trace, or establish any CPU-clock conversion.

E-341 adds a content-bounded end-of-run LCD summary to the Native JR-800
runner. The runtime owns one immutable diagnostic snapshot containing the
existing substituted-read total and counts of unknown, off, and on dots across
the provisional 192 by 64 panel. A disconnected LCD leaves both fields absent.
The CLI prints only those three counts and never retains or emits coordinates,
rows, controller RAM, rendered text, or a display digest. Synthetic tests cover
the disconnected state, an all-off initialized panel, and a mixed-state panel.
This diagnoses display progress without validating U-011's physical
composition or disclosing an owner-local screen.

`Jr800Machine` owns this adapter before the generic execution `Machine`, so the
shared debugger can attach without a synthetic-memory mode. The JR8APP/SDK/WASM
path instead uses `SyntheticMachine`, which owns a `RamBus` and keeps host
application loading out of the generic execution Interface. JR-800 reset-entry
initialization uses E-036's vector and known CCR bits without manufacturing A,
B, X, SP, or H/N/Z/V/C. An instruction can proceed from that partial state only
when all state it consumes is known. This seam is not a claim that BASIC boots
or that unsupported devices return open-bus values.

## Debugger stop semantics

`Machine`/`MachineObserver` and `Bus`/`BusObserver` registrations are
one-to-one. Both endpoints are non-copyable and non-movable,
and destruction of either endpoint removes the opposite raw observer pointer.
Attaching one observer to a second owner is rejected rather than silently
transferring it.

- An unconditional execution breakpoint stops before fetching the opcode at
  its address. A conditional breakpoint first evaluates its bounded compiled
  expression in C++; nonzero stops and zero continues. Unknown CPU state,
  failed non-invasive memory inspection, and arithmetic evaluation errors stop
  explicitly without executing an instruction.
- A read, write, or access memory watchpoint stops after the instruction
  containing the first matching data access has completed, so its history entry
  and final CPU state are available. Instruction fetches do not match, and the
  stop record retains the matching data-access kind.
- An instruction limit stops before executing a further instruction.
- Run-to-address stops before fetching the target instruction. The target takes
  priority over a persistent execution breakpoint at the same address, while an
  earlier breakpoint, watchpoint, sleep, or fault remains visible.
- Run-to-source resolves an exact JR8DBG source path and nonzero line to that
  line's lowest mapped address, then uses run-to-address unchanged. Missing
  locations remain unresolved rather than selecting a nearby line.
- Run-to-symbol resolves an exact case-sensitive JR8DBG name only when one
  address-kind symbol matches, then uses run-to-address unchanged. Missing,
  ambiguous, and absolute symbols remain explicit failures.
- Step-over uses generated ISA metadata. A non-call performs one ordinary step;
  a call executes despite a breakpoint at its own address, then runs to its
  wrapped fall-through address. A bounded or sleeping call stop carries the
  address needed to continue, while an intervening breakpoint, watchpoint,
  fault, or interrupt entry remains visible.
- Step-out follows generated call, subroutine-return, and interrupt-return
  classifications with explicit resumable nesting state. It does not guess a
  return address from the current stack, and stops immediately after the first
  return at depth zero.
- A CPU fault stops immediately and remains visible in the history. For a bus
  fault, the trigger address is the failed bus address rather than the opcode
  address. For unknown CPU state, the C++ step/history record identifies the
  required state part and the trigger remains the opcode address.

Instruction history stores structured bytes, registers, cycle totals, and the
range of associated structured bus events. Formatting is deferred to the host.
JR8DBG data is accepted only when both its exact target profile and its bound
JR8APP SHA-256 digest match the loaded machine.

## End-to-end sample

`sdk/examples/write-watch` is the canonical vertical slice. Its Makefile invokes
`jr8as` and `jr8ld`, then `jr8run` loads the resulting JR8APP and JR8DBG files.
The test stops on a memory watchpoint in write mode and resolves that stop back
to the original assembly source. It does not use hand-authored machine-code
bytes.

The sample's `make test` evaluates repeatable `jr8run --expect` expressions
after that stop. All expressions compile before execution. A nonzero result
passes; zero, unknown state, failed non-invasive memory inspection, or an
arithmetic evaluation error fails the process without changing machine state or
the architectural trace. This reuses the conditional-breakpoint evaluator and
replaces the former byte-only expectation parser.
