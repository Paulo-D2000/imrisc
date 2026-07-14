#!/usr/bin/env python3
"""Read a smtbmc counterexample VCD (vcdvcd) as a per-timestep table."""
import sys, re
from vcdvcd import VCDVCD

vcd = VCDVCD(sys.argv[1])
sigs = {s: vcd[s] for s in vcd.signals}

def val_at(sigobj, t):
    last = 'x'
    for (tt, vv) in sigobj.tv:
        if tt <= t:
            last = vv
        else:
            break
    return last

all_times = set()
for s in sigs.values():
    for (tt, _) in s.tv:
        all_times.add(tt)
times = sorted(all_times)

wanted = ['clk', 'reset', 'i_axi_reset_n', 'smt_step',
          'awvalid', 'awready', 'wvalid', 'wready', 'bvalid', 'bready',
          'arvalid', 'arready', 'rvalid', 'rready', 'awready_reg',
          'f_axi_awstall', 'f_axi_wstall', 'f_axi_awr_outstanding', 'f_axi_wr_outstanding']
cols = []
for w in wanted:
    for s in vcd.signals:
        if re.search(rf'(^|[.<>]){re.escape(w)}$', s) and s not in cols:
            cols.append(s)

def fmt(v):
    if len(v) > 1 and all(c in '01' for c in v):
        return str(int(v, 2))  # binary -> decimal for wide counters
    return v

names = [c.replace('axilite_slave_top.', '').replace('axilite_master_top.', '') for c in cols]
print('step\t' + '\t'.join(names))
for t in times:
    # only print on clock high (one row per cycle)
    if val_at(sigs.get('axilite_slave_top.clk') or sigs.get('axilite_master_top.clk', list(sigs)[0]), t) not in ('1',):
        continue
    row = []
    for c in cols:
        row.append(fmt(val_at(sigs[c], t)))
    print('\t'.join(row))
