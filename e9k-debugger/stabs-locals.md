STABS Locals Plan (Amiga Hunk, No `readelf`)
===========================================

Goal
----
Add support for printing *current-frame* local variables and parameters via the existing `print <expr>` command
when we do not have working DWARF + CFI tooling (i.e. no usable `readelf`), but we do have STABS in the
Amiga hunk executable.

Target environment / constraints
-------------------------------
- Toolchain: `/opt/amiga/bin/m68k-amigaos-*`
- Test binary: `tests/amiga/locals/locals` (AmigaOS loadseg()ble / hunk format, not ELF)
- Compilation: includes `-g` and `-fomit-frame-pointer`
- Runtime: we must assume `-fomit-frame-pointer` (no reliable A6 frame pointer), so DWARF `DW_OP_fbreg` style
  evaluation is unavailable without CFI. In the STABS-only path we will *not* have CFI.
- Available tooling: `m68k-amigaos-objdump` and `m68k-amigaos-addr2line` are present; `m68k-amigaos-readelf`
  cannot parse hunk files.

What we observed in `tests/amiga/locals/locals`
----------------------------------------------
- `m68k-amigaos-objdump -h` shows `.stab` and `.stabstr` sections.
- `m68k-amigaos-objdump -G` emits:
  - Function records: `FUN`, plus a second `FUN` line with no string that appears to carry the function size.
  - Scope records: `LBRAC` / `RBRAC` with `n_value` addresses.
  - Parameters: `PSYM` entries with an offset (e.g. `x:p1` at `0x0c`, `y:p1` at `0x10` in `_funtimes`).
  - Locals: `LSYM` entries with offsets (e.g. `j` at `0x04`, `k` at `0x00`).
  - Register symbols: `RSYM` entries of the form `_d1:r1` with a register number in `n_value`.
- Disassembly confirms the offsets are SP-based after the function prologue (and with `-fomit-frame-pointer`).

Design overview
---------------
We implement a second local-resolution backend:

1) DWARF backend (existing):
   - `print_eval_resolveLocal()` uses DWARF scope + CFI to compute CFA and evaluate DWARF location expressions.

2) STABS backend (new, Amiga hunk / no readelf):
   - Parse `objdump -G` to build:
     - Function ranges + nested lexical scopes (`LBRAC`/`RBRAC`)
     - For each scope: locals/params/register vars and their types
   - At resolve time:
     - Determine current PC (relative to text base, same normalization as `addr2line` uses).
     - Find innermost scope containing PC.
     - Look for a matching symbol name in that scope, then walk outward to parent scopes.
     - Compute address/value from *current* SP (A7) or from the indicated register number.

The STABS backend will be best-effort and is explicitly “current-frame only”.

Data model additions
--------------------
Extend `print_index_t` with STABS-local information (names are illustrative):

- `print_stabs_func_t *stabsFuncs; int stabsFuncCount, stabsFuncCap;`
  - `char *name;`
  - `uint32_t startPcRel;`  (file/text-relative address)
  - `uint32_t endPcRel;`    (exclusive)
  - `print_stabs_scope_t *rootScope;` (or an arena index)

- `print_stabs_scope_t`
  - `uint32_t startPcRel; uint32_t endPcRel;` (exclusive)
  - `print_stabs_scope_t *parent;`
  - `print_stabs_scope_t **children; int childCount;`
  - `print_stabs_var_t *vars; int varCount;`

- `print_stabs_var_t`
  - `char *name;`
  - `uint32_t typeRef;` (reuse existing `print_type_t` system; typeRef is the stabs “die offset” encoding already used)
  - `size_t byteSize; int hasByteSize;` (optional, for fallback)
  - `enum { stabs_var_stack, stabs_var_reg, stabs_var_const } kind;`
  - `int32_t stackOffset;` (for stack vars: `addr = sp + stackOffset`)
  - `uint8_t reg;`         (for reg vars: stabs register number)

We reuse the existing STABS type parsing infrastructure in `print_debuginfo_objdump_stabs.c` so locals get real types.

Parser plan (`objdump -G`)
--------------------------
Add a new loader that runs `objdump -G` (via `debugger_toolchainBuildBinary("objdump")`) and parses the same text
table that `print_debuginfo_objdump_stabs_loadSymbols()` already consumes, but we will *not* restrict ourselves
to `GSYM/LCSYM/STSYM`.

Key records we care about:
- `FUN name:F...`:
  - Start a new function context; record `startPcRel = n_value`.
- `FUN` with empty/0 string (observed right after PSYM list):
  - Use `n_value` as size; set `endPcRel = startPcRel + size` if size != 0.
  - If size is missing, fall back to end markers (`ENSYM`) or the last scope `RBRAC`.
- `BNSYM` / `ENSYM`:
  - Treat as additional boundary hints; prefer explicit size if present.
- `LBRAC` / `RBRAC`:
  - Maintain a scope stack for the current function:
    - On `LBRAC`, push a new scope with `startPcRel = n_value`.
    - On `RBRAC`, pop and set `endPcRel = n_value`.
  - Locals/params seen while a scope is active attach to the current scope; otherwise attach to the function root scope.
- `PSYM`:
  - Add parameter var: stack-based, `stackOffset = (int32_t)n_value`.
- `LSYM`:
  - Add local var: stack-based, `stackOffset = (int32_t)n_value`.
  - Note: in this toolchain, many locals are encoded as `name:485` or `name:487=B68` (no storage-class letter),
    so the existing `parseVarTypeId()` is insufficient.
    Plan: implement `parseVarTypeIdLoose()`:
      - Find `':'`, then skip any `[A-Za-z]` modifiers (e.g. `p`, `r`, `V`, etc.).
      - Parse a STABS type id using the existing `parseTypeId()` helper (digits or `(a,b)`).
- `RSYM`:
  - Add register var: `reg = (uint8_t)n_value`, `kind = stabs_var_reg`.

Type handling:
- Continue collecting LSYM type-def records (already implemented).
- When adding a var, use `typeId` -> `print_debuginfo_objdump_stabs_buildType()` to get a `print_type_t*`,
  store `typeRef = t->dieOffset`.
- If type resolution fails, fall back to default `uint32_t` and/or derive size from known base types when possible.

Addressing model (no CFI, omit-frame-pointer)
--------------------------------------------
We need a frame base to turn STABS stack offsets into memory addresses.

Phase 1 (minimal, matches `tests/amiga/locals/locals`):
- Use the *current* stack pointer as the base:
  - Read A7 via `machine_findReg("A7")`.
  - For stack vars: `addr = (uint32_t)(sp + stackOffset) & 0x00ffffff`.
- For register vars:
  - Map stabs register numbers 0..15 to D0..D7 / A0..A7 and return an immediate value.

Known limitation: SP can temporarily move during call sequences / spills. If we break mid-call-setup, offsets may
be wrong. This is acceptable for Phase 1; we’ll document it and add debug logging to help diagnose.

Phase 2 (optional robustness):
- Attempt to compute a “canonical frame SP” by detecting common prologue patterns from the function entry and/or
  using saved-A6 heuristics:
  - If we stop very early in a function (PC close to `startPcRel`), the prologue might not have established the
    stable SP layout yet.
  - We can special-case “within first N bytes of function” and use lightweight prologue simulation to adjust SP.
  - This can be done without `readelf` by disassembling a small window via existing code paths (or a tiny 68k decode).
  - This is explicitly deferred until Phase 1 is validated.

PC normalization (important)
----------------------------
The debugger’s `addr2line_resolve()` subtracts `debugger.machine.textBaseAddr` from the runtime PC before querying
the tool. We must do the same when matching PC against STABS `n_value` addresses.

Rule:
- `pcRel = pc; if (textBaseAddr != 0 && pcRel >= textBaseAddr) pcRel -= textBaseAddr;`
- Then use `pcRel` to pick the STABS function/scope.

Resolver integration points
---------------------------
Add a new resolver function used by `print_eval_parsePrimary()`:

- Keep current behavior:
  - globals (`index->vars`)
  - symbols (`index->symbols`)
  - registers by name (`machine_findReg`)
  - DWARF locals via `print_eval_resolveLocal()` (existing)

- New behavior when DWARF locals are unavailable:
  - If `index->nodeCount == 0` (no DWARF scope tree), try STABS locals:
    - `print_eval_resolveLocalStabs(name, index, out, typeOnly)`

`typeOnly` handling:
- Stack vars: return an address value with `hasAddress=0` when `typeOnly` is set (consistent with DWARF backend).
- Register vars: return an immediate value with `hasImmediate=0` when `typeOnly` is set.

Loading & caching
-----------------
Extend `print_eval_loadIndex()`:
- Today it always runs:
  - `readelf --syms` (via objdump) -> symbols
  - `readelf --debug-dump=info` -> DWARF nodes (fails for hunk)
  - `readelf --debug-dump=frames-interp` -> CFI (fails for hunk)
  - If `nodeCount == 0` -> fall back to `objdump -G` STABS globals/static symbols.

Plan change:
- In the STABS fallback path, additionally call a new loader:
  - `print_debuginfo_objdump_stabs_loadLocals(elfPathOrHunkPath, index)`
    - Reuses the same `objdump -G` invocation but parses `FUN/PSYM/LSYM/RSYM/LBRAC/RBRAC`.
    - Builds `index->stabsFuncs` for local resolution and completion.

Testing plan
------------
Manual verification using the known Amiga locals binary:

1) Ensure text base mapping is correct:
   - Stop at runtime inside `_funtimes` and `_main`.
   - Confirm `addr2line` continues to work (it already does) and `pcRel` mapping matches STABS function ranges.

2) Validate locals in `_funtimes`:
   - At a point after prologue:
     - `print j` should show the stack-stored value (matches `move.l 12(sp),4(sp)`).
     - `print k` should show stack-stored value.
     - `print x` / `print y` should show param values via offsets `0x0c` / `0x10`.

3) Validate locals in `_main`:
   - `print start`, `print one`, `print two`, `print apointer`, `print fun`.

4) Type sanity:
   - Confirm `apointer` prints as pointer-sized and can be dereferenced with `*apointer` when valid.
   - Confirm integer locals print as `uint32_t` if specific type resolution fails.

5) Debug instrumentation:
   - Add `E9K_PRINT_DEBUG` logs that print:
     - selected function name, `pcRel`, scope depth
     - chosen base SP, and computed address per variable

Known limitations / non-goals (Phase 1)
---------------------------------------
- No multi-frame locals (only the current frame).
- No guaranteed correctness if stopped in the middle of a call sequence where SP is temporarily adjusted.
- No location lists / optimized variable tracking beyond what STABS provides.
- No support for “variable not addressable” cases (we’ll just fail to resolve).

Deliverables
------------
- `print_debuginfo_objdump_stabs_loadLocals()` (new) that parses `objdump -G` into function/scope/var tables.
- `print_eval_resolveLocalStabs()` (new) that resolves a name at the current PC to an address/immediate.
- Minimal type parsing improvement (`parseVarTypeIdLoose`) so locals like `start:485` resolve types.
- Tests/notes anchored on `tests/amiga/locals/locals`.

