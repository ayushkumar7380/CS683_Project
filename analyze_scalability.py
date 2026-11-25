#!/usr/bin/env python3
"""Fail closed on missing metrics/incorrect MACs; emit honest A/B CSV."""
import csv
import re
import sys
from pathlib import Path
folder=Path(sys.argv[1] if len(sys.argv)>1 else 'scalability_results')

def need(pattern,txt,label):
    m=re.search(pattern,txt)
    if not m: raise SystemExit(f'[SCALABILITY_ERROR] missing {label}')
    return int(m.group(1))

def read(out,inn,cache):
    stem=f'{out}x{inn}_cache{cache}'
    correct=(folder/f'{stem}_correct.log').read_text()
    perf=(folder/f'{stem}_perf.log').read_text()
    perf = (perf.replace('[STEP10_CONFIG]', '[TILE_CACHE_CONFIG]')
               .replace('[STEP10_PROFILE]', '[TILE_CACHE_PROFILE]')
               .replace('[BASELINE_PIM_CONFIG]', '[TILE_CACHE_CONFIG]')
               .replace('[CACHE_ENABLED_CONFIG]', '[TILE_CACHE_CONFIG]')
               .replace('[BASELINE_PIM_PROFILE]', '[TILE_CACHE_PROFILE]')
               .replace('[CACHE_ENABLED_PROFILE]', '[TILE_CACHE_PROFILE]'))
    passed=need(r'> passed :\s*(\d+)',correct,stem+' passed')
    failed=need(r'> failed :\s*(\d+)',correct,stem+' failed')
    if passed!=out or failed: raise SystemExit(f'[SCALABILITY_ERROR] {stem} correctness {passed}/{out}, failures={failed}')
    cycles=[int(x) for x in re.findall(r'> Cycle :\s*(\d+)',perf)]
    if len(cycles)<2: raise SystemExit(f'[SCALABILITY_ERROR] {stem} missing two cycles')
    macs=[int(x) for x in re.findall(r'mac_instructions=(\d+)',perf)]
    if len(macs)<2: raise SystemExit(f'[SCALABILITY_ERROR] {stem} missing MAC counter')
    en=need(r'\[TILE_CACHE_CONFIG\].*?enabled=(\d+)',perf,stem+' enabled')
    skips=need(r'\[TILE_CACHE_PROFILE\].*?skipped_grfa_transactions=(\d+)',perf,stem+' skipped writes')
    restores=need(r'\[TILE_CACHE_PROFILE\].*?restore_control_groups=(\d+)',perf,stem+' restore groups')
    match=re.search(r'\[GEMV_TILE_PROFILE\].*?output_tiles=(\d+) input_tiles=(\d+).*?grfa_write_transactions=(\d+)',perf)
    if not match: raise SystemExit(f'[SCALABILITY_ERROR] {stem} missing tile profile')
    output_tiles,input_tiles,writes=map(int,match.groups())
    return dict(cpu=cycles[0],pim=cycles[1],macs=macs[-1],enabled=en,skips=skips,restores=restores,tiles=output_tiles,input_tiles=input_tiles,writes=writes)

rows=[]
for basefile in sorted(folder.glob('*_cache0_perf.log')):
    m=re.fullmatch(r'(\d+)x(\d+)_cache0_perf.log',basefile.name)
    if not m: continue
    out,inn=map(int,m.groups())
    a,b=read(out,inn,0),read(out,inn,1)
    if a['macs']!=b['macs']:
        raise SystemExit(f'[SCALABILITY_ERROR] {out}x{inn} MAC mismatch baseline={a["macs"]} opt={b["macs"]}')
    if b['enabled']!=int(a['tiles']>1 and a['input_tiles']<=8):
        raise SystemExit(f'[SCALABILITY_ERROR] unexpected cache eligibility for {out}x{inn}')
    saved=a['pim']-b['pim']
    row={'output':out,'input':inn,'output_tiles':a['tiles'],'input_tiles':a['input_tiles'],
         'cpu_cycles':a['cpu'],'baseline_pim_cycles':a['pim'],'cache_pim_cycles':b['pim'],
         'cycles_saved':saved,'pim_cycle_reduction_pct':round(100*saved/a['pim'],3),
         'baseline_cpu_speedup':round(a['cpu']/a['pim'],5),
         'cache_cpu_speedup':round(b['cpu']/b['pim'],5),
         'grfa_writes_baseline':a['writes'],'grfa_writes_cache':b['writes'],
         'grfa_payload_writes_skipped':b['skips'],'restore_control_groups':b['restores'],
         'break_even_extra_serial_cycles_per_restore_group':round(saved/b['restores'],3) if b['restores'] else '',
         'mac_instructions_both':b['macs'],'correct_outputs_each':out}
    rows.append(row)
    print('[SCALABILITY_RESULT]', ' '.join(f'{k}={v}' for k,v in row.items()))
if not rows: raise SystemExit('[SCALABILITY_ERROR] no completed A/B comparisons')
with (folder/'scalability_summary.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
print('[SCALABILITY_SUMMARY]',folder/'scalability_summary.csv')
