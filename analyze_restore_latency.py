#!/usr/bin/env python3
"""Validate and summarize actual per-channel RESTORE issuance/cooldown effects."""
from pathlib import Path
import csv
import re
import sys

folder = Path(sys.argv[1] if len(sys.argv) > 1 else 'restore_latency_results')
pat = re.compile(r'(?P<out>\d+)x(?P<inp>\d+)_cache(?P<cache>[01])_penalty(?P<penalty>\d+)_perf\.log$')

def get_last(field, content):
    vals = re.findall(r'\b' + re.escape(field) + r'=(\d+)', content)
    if not vals:
        raise RuntimeError(f'Missing {field}')
    return int(vals[-1])

runs = {}
for fp in folder.glob('*_perf.log'):
    m = pat.fullmatch(fp.name)
    if not m: continue
    out, inp, cache, penalty = map(int, m.groups())
    content = fp.read_text(errors='replace')
    content = content.replace('[STEP13_RUNTIME]', '[RESTORE_LATENCY_RUNTIME]')
    cycles = list(map(int, re.findall(r'^> Cycle\s*:\s*(\d+)', content, re.M)))
    if len(cycles) != 2:
        raise RuntimeError(f'Expected host and PIM cycles in {fp}, got {cycles}')
    profile = re.findall(r'^\[BASELINE_PROFILE\].*$', content, re.M)
    if len(profile) != 2: raise RuntimeError(f'Expected CPU and PIM runtime profiles: {fp}')
    mac = get_last('mac_instructions', profile[-1])
    runtime = re.findall(r'^\[RESTORE_LATENCY_RUNTIME\].*$', content, re.M)
    if len(runtime) != 2: raise RuntimeError(f'Expected CPU/PIM cooldown profiles: {fp}')
    d = runtime[-1]
    issues = get_last('restore_issues', d)
    stalls = get_last('channel_stall_cycles', d)
    with_queue = get_last('channel_stall_with_queue', d)
    configured = get_last('penalty_per_restore', d)
    if configured != penalty: raise RuntimeError(f'Wrong configured penalty in {fp}')
    if cache and penalty and (issues == 0 or stalls == 0):
        raise RuntimeError(f'Penalty is not actually applied! restore_issues={issues} stalls={stalls}: {fp}')
    if not cache and (issues or stalls):
        raise RuntimeError(f'Baseline erroneously using cache cooldown: {fp}')
    correct = Path(str(fp).replace('_perf.log', '_correct.log')).read_text(errors='replace')
    if not re.search(r'>\s*passed\s*:\s*' + str(out) + r'\b', correct) or not re.search(r'>\s*failed\s*:\s*0\b', correct):
        raise RuntimeError(f'Incorrect outputs: {fp}')
    runs[(out,inp,cache,penalty)] = dict(workload=f'{out}x{inp}', output=out, input=inp, cache=cache,
        penalty=penalty, cpu_cycles=cycles[0], pim_cycles=cycles[1], mac_instructions=mac,
        restore_issues=issues, channel_stall_cycles=stalls, channel_stall_with_queue=with_queue)

rows=[]; bounds=[]
for out, inp in sorted({(r['output'],r['input']) for r in runs.values()}):
    base = runs[(out,inp,0,0)]
    relevant = sorted((r for r in runs.values() if r['output']==out and r['input']==inp and r['cache']), key=lambda x:x['penalty'])
    first_loss = None; last_win=None
    for r in relevant:
        if r['mac_instructions'] != base['mac_instructions']:
            raise RuntimeError(f'MAC mismatch on {r["workload"]}, penalty {r["penalty"]}')
        if r['cpu_cycles'] != base['cpu_cycles']:
            raise RuntimeError(f'Host timing changed on {r["workload"]}, penalty {r["penalty"]}')
        saved = base['pim_cycles']-r['pim_cycles']
        row = {**r, 'baseline_pim_cycles':base['pim_cycles'], 'cycles_saved':saved,
            'reduction_pct':round(100*saved/base['pim_cycles'],3)}
        rows.append(row)
        if saved > 0: last_win=r['penalty']
        elif first_loss is None: first_loss=r['penalty']
        print('[RESTORE_LATENCY_RESULT]', *(f'{k}={row[k]}' for k in ('workload','penalty','baseline_pim_cycles','pim_cycles','cycles_saved','reduction_pct','restore_issues','channel_stall_cycles','channel_stall_with_queue','mac_instructions')))
    bounds.append(dict(workload=f'{out}x{inp}', last_tested_beneficial_penalty=last_win,
        first_tested_nonbeneficial_penalty=first_loss, note='discrete simulator sensitivity; not physical SRAM latency'))

if not rows: raise RuntimeError(f'No usable performance logs in {folder}')
with (folder/'restore_latency_results.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
with (folder/'restore_latency_bounds.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f, fieldnames=list(bounds[0])); writer.writeheader(); writer.writerows(bounds)
lines=['# RESTORE latency — Verified RESTORE issuance sensitivity', '',
       'The command-issue penalty is a model, not measured SRAM latency. All runs passed numerical correctness and MAC-count equality.', '',
       '| Workload | Extra cycles/restore/channel | PIM cycles | Saved vs. baseline | Restore issues (all channels) | Stall cycles (all channels) |',
       '|---|---:|---:|---:|---:|---:|']
for r in rows:
    lines.append(f'| {r["workload"]} | {r["penalty"]} | {r["pim_cycles"]} | {r["cycles_saved"]} | {r["restore_issues"]} | {r["channel_stall_cycles"]} |')
lines+=['', 'Sum-of-channel stall cycles are NOT end-to-end critical-path cycle counts.', '',
        'A crossover is reported only when the measured PIM cycles are not lower than the matched baseline.', '',
        '## Tested crossover bounds', '']
for b in bounds: lines.append(f'- {b["workload"]}: last beneficial={b["last_tested_beneficial_penalty"]}, first nonbeneficial={b["first_tested_nonbeneficial_penalty"]}')
(folder/'restore_latency_report.md').write_text('\n'.join(lines)+'\n')
print('[RESTORE_LATENCY_FILES]', folder/'restore_latency_results.csv', folder/'restore_latency_bounds.csv', folder/'restore_latency_report.md')
