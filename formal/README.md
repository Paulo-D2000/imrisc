# AXI-Lite Formal Verification

Formal verification of the MyHDL AXI-Lite master and slave ports
(`gpgpu.py`: `AXILiteMasterPort`, `AXILiteSlavePort`) against ZipCPU's
`faxil_master` / `faxil_slave` property checkers, plus functional testbenches.

## Results (k-induction, unbounded)

| Task | Mode | Result |
|------|------|--------|
| `axilite_slave.sby prf` | prove  | PASS (basecase + induction) |
| `axilite_master.sby prf` | prove | PASS (basecase + induction) |
| `axilite_slave.sby cvr`  | cover  | PASS (read+write transactions reachable) |
| `axilite_master.sby cvr` | cover  | PASS |
| `tb_axilite.py`          | sim    | ALL PASS (read/write/read-back/SLVERR/strobe) |
| `tb_cpu_ram.py`          | sim    | ALL PASS (CPU<->AXI-Lite<->RAM end-to-end) |

Run with: `wsl -d Ubuntu -e bash -c "source ~/oss-cad-suite/environment && cd /mnt/d/Dev/imrisc/formal && sby -f axilite_slave.sby prf"` (likewise `master`, and `cvr`).

## Layout

- `rtl/`         - auto-generated DUT Verilog (regenerate with `python ../convert_formal.py`)
- `lib/`         - ZipCPU property checkers (faxil_slave.v, faxil_master.v) - see NOTICE below
- `top/`         - formal top-level wrappers (DUT + property checker + DUT invariants)
- `axilite_*.sby` - SymbiYosys configs
- `parse_vcd.py` - VCD counterexample reader (uses the `vcdvcd` package)

## How the DUTs are made formal-ready

`../convert_formal.py` converts the MyHDL blocks to Verilog with
`initial_values=True` (power-on register init) and a post-processor that adds
`= 0` init to **output-port** regs (MyHDL's `initial_values` only initializes
internal regs, not port regs - verified empirically). The wrappers mirror
ZipCPU's proven `easyaxil` (slave) and `axilops` (master) patterns: sync reset,
default `faxil` params, and state-dependent outstanding-count asserts.

## NOTICE - third-party sources

The following files are © Dan Gisselquist, Gisselquist Technology, LLC, licensed
under the Apache License, 2.0 (license headers preserved in each file):

- `lib/faxil_slave.v`, `lib/faxil_master.v` - from https://github.com/ZipCPU/wb2axip (`bench/formal/`)
- Reference designs studied in `../docs/` (`easyaxil.v`, `skidbuffer.v`, `faxil_register.v`, `wbm2axilite.v` from `ZipCPU/wb2axip`; `axilops.v` from `ZipCPU/zipcpu`)

Methodology and protocol properties follow Dan Gisselquist's article
"Using a formal property file to verify an AXI-lite peripheral"
(https://zipcpu.com/formal/2018/12/28/axilite.html) and the `easyaxil`/`axilops`
reference designs. All credit for the property-checking methodology belongs to
the ZipCPU/wb2axip project.
