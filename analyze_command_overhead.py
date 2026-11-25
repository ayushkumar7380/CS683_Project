#!/usr/bin/env python3
"""Command overhead: paired, correctness-gated GEMV bottleneck comparison.

Reads scalability performance and correctness logs; no new gem5 or DRAM commands needed.
Counters are aggregate per channel and/or overlapping diagnostics; differences
are observational, not an exact cycle decomposition.
"""
import argparse
import csv
import re
import sys
from pathlib import Path

SECTIONS = ('BASELINE_PROFILE', 'SCHED_DIAG', 'SCHED_COUNTERFACTUAL',
            'DRAM_TIMING_DIAG', 'ROW_BANK_PROFILE', 'SCHED_OPPORTUNITY',
            'PIM_STREAM_PROFILE', 'PIM_PHASE_PROFILE', 'GRF_A_RUNTIME',
            'TILE_CACHE_PROFILE', 'GEMV_TILE_PROFILE')
SELECTED = {
 'BASELINE_PROFILE': ['controller_channel_cycles','queued_no_issue_channel_cycles',
   'issued_read','issued_write','issued_activate','issued_precharge',
   'issued_refresh','issued_pim_mode_read','issued_pim_mode_write',
   'pim_rank_mode_cycles','pim_rank_mac_issue_cycles','mac_instructions',
   'bank_operand_reads','other_pim_instructions'],
 'SCHED_DIAG':['queued_no_issue_channel_cycles','barrier','rw_timing',
               'needs_activate','wrong_open_row','activate_timing','precharge_timing'],
 'SCHED_COUNTERFACTUAL':['barrier_blocks_ready_command','timing_only_blocked',
                         'any_dram_ready_queued_command'],
 'DRAM_TIMING_DIAG':['wait_tRCD','wait_tCCD_or_burst','wait_bus_turnaround',
                     'wait_row_not_open','wait_wrong_row','wait_bank_state'],
 'ROW_BANK_PROFILE':['enqueued_requests','enqueue_row_open_hit','enqueue_wrong_open_row',
                     'enqueue_row_not_open','activates','precharges'],
 'PIM_STREAM_PROFILE':['issued','tagged','barrier_tagged','grf_tagged','mac_tagged',
                       'mac_runs','max_mac_run_per_channel'],
 'PIM_PHASE_PROFILE':['mac_runs_completed','mac_gap_events','mac_gap_commands',
                      'mac_gap_cycles','max_mac_gap_cycles','mac_break_grf',
                      'mac_break_actpre','gap_grf','gap_barrier','gap_actpre',
                      'phase_first_to_last_mac_cycles'],
 'GRF_A_RUNTIME':['grfa_broadcast_writes','mac_operand_reads','mac_reads_before_load'],
 'TILE_CACHE_PROFILE':['fill_control_groups','restore_control_groups','skipped_grfa_transactions'],
 'GEMV_TILE_PROFILE':['grfa_write_transactions','mac_read_transactions',
                      'grfa_barrier_calls','mac_barrier_calls']
}

def parse_kvs(line):
    return {key:int(value) for key,value in re.findall(r'\b([A-Za-z][A-Za-z0-9_]*)=(-?\d+)\b',line)}

def parse_perf(path):
    text=path.read_text(errors='replace')
    text = (text.replace('[STEP10_CONFIG]', '[TILE_CACHE_CONFIG]')
                .replace('[STEP10_PROFILE]', '[TILE_CACHE_PROFILE]')
                .replace('[BASELINE_PIM_CONFIG]', '[TILE_CACHE_CONFIG]')
                .replace('[CACHE_ENABLED_CONFIG]', '[TILE_CACHE_CONFIG]')
                .replace('[BASELINE_PIM_PROFILE]', '[TILE_CACHE_PROFILE]')
                .replace('[CACHE_ENABLED_PROFILE]', '[TILE_CACHE_PROFILE]'))
    cycles=list(map(int,re.findall(r'>\s*Cycle\s*:\s*(\d+)',text)))
    if len(cycles)!=2: raise ValueError(f'{path}: expected exactly 2 cycle counts, got {cycles}')
    if '[  PASSED  ] 1 test.' not in text: raise ValueError(f'{path}: performance test not PASSED')
    data={}
    for line in text.splitlines():
        m=re.match(r'^\[([A-Z0-9_]+)\]\s*(.*)$',line)
        if m and m.group(1) in SECTIONS:
            data.setdefault(m.group(1),[]).append(parse_kvs(m.group(2)))
                                                                    
                                                                            
    for section in SECTIONS:
        if section not in data: raise ValueError(f'{path}: missing [{section}]')
        if section in ('BASELINE_PROFILE','GRF_A_RUNTIME','PIM_PHASE_PROFILE',
                       'PIM_STREAM_PROFILE','SCHED_DIAG','DRAM_TIMING_DIAG',
                       'ROW_BANK_PROFILE','SCHED_COUNTERFACTUAL','SCHED_OPPORTUNITY'):
                                                                                      
            if len(data[section])!=2:
                raise ValueError(f'{path}: expected 2 [{section}] profiles (CPU then PIM), found {len(data[section])}')
            data[section]=data[section][1]
        else:
            if len(data[section])!=1:
                raise ValueError(f'{path}: expected 1 [{section}], found {len(data[section])}')
            data[section]=data[section][0]
    cfg=re.search(r'^\[TILE_CACHE_CONFIG\].*\benabled=(\d+)\b',text,re.M)
    if not cfg: raise ValueError(f'{path}: missing TILE_CACHE_CONFIG enabled')
    return {'cpu_cycles':cycles[0],'pim_cycles':cycles[1],
            'enabled':int(cfg.group(1)),'metrics':data}

def check_correctness(path,expected):
    text=path.read_text(errors='replace')
    matches=re.findall(r'>\s*(passed|failed)\s*:\s*(\d+)',text)
    items=dict((k,int(v)) for k,v in matches)
    if (items.get('passed')!=expected or items.get('failed')!=0 or
        '[  PASSED  ] 1 test.' not in text):
        raise ValueError(f'{path}: failed correctness gate: {items}')

def safeget(prof,sect,key):
    if key not in prof['metrics'][sect]:
        raise ValueError(f'Missing metric {sect}.{key}')
    return prof['metrics'][sect][key]

def pct_change(a,b):
    return round(100*(b-a)/a,3) if a else ''

def analyze(folder):
    rows=[]; details=[]
    for base in sorted(folder.glob('*_cache0_perf.log')):
        match=re.fullmatch(r'(\d+)x(\d+)_cache0_perf.log',base.name)
        if not match: continue
        out,inn=map(int,match.groups())
        stem=f'{out}x{inn}'
        bpath=folder/f'{stem}_cache1_perf.log'
        if not bpath.exists(): raise ValueError(f'Missing optimized log for {stem}')
        check_correctness(folder/f'{stem}_cache0_correct.log',out)
        check_correctness(folder/f'{stem}_cache1_correct.log',out)
        a,b=parse_perf(base),parse_perf(bpath)
        if a['enabled']!=0: raise ValueError(f'{stem}: baseline has cache enabled')
        should_enable=int(out>4096 and inn<=1024)
        if b['enabled']!=should_enable: raise ValueError(f'{stem}: unexpected cache enabled={b["enabled"]}, expected {should_enable}')
        if a['cpu_cycles']!=b['cpu_cycles']:
            raise ValueError(f'{stem}: conventional cycles differ between A/B')
        for sec,metric in [('BASELINE_PROFILE','mac_instructions'),
                           ('BASELINE_PROFILE','issued_pim_mode_read'),
                           ('GRF_A_RUNTIME','mac_operand_reads')]:
            va,vb=safeget(a,sec,metric),safeget(b,sec,metric)
            if va!=vb: raise ValueError(f'{stem}: functional work mismatch {sec}.{metric}: {va} vs {vb}')
        save=a['pim_cycles']-b['pim_cycles']
        restore=safeget(b,'TILE_CACHE_PROFILE','restore_control_groups')
        row={'workload':stem,'output':out,'input':inn,
             'output_tiles':safeget(b,'GEMV_TILE_PROFILE','output_tiles'),
             'input_tiles':safeget(b,'GEMV_TILE_PROFILE','input_tiles'),
             'cpu_cycles':a['cpu_cycles'],'baseline_pim_cycles':a['pim_cycles'],
             'cache_pim_cycles':b['pim_cycles'],'cycles_saved':save,
             'cycle_reduction_pct':round(100*save/a['pim_cycles'],3),
             'speedup_baseline':round(a['cpu_cycles']/a['pim_cycles'],4),
             'speedup_cache':round(a['cpu_cycles']/b['pim_cycles'],4),
             'mac_instructions_both':safeget(a,'BASELINE_PROFILE','mac_instructions'),
             'grfa_writes_saved':safeget(a,'GEMV_TILE_PROFILE','grfa_write_transactions')-safeget(b,'GEMV_TILE_PROFILE','grfa_write_transactions'),
             'restore_control_groups':restore,
             'break_even_extra_serial_cycles_per_restore_group':round(save/restore,3) if restore else '',
             'issued_pim_mode_writes_saved':safeget(a,'BASELINE_PROFILE','issued_pim_mode_write')-safeget(b,'BASELINE_PROFILE','issued_pim_mode_write'),
             'mac_gap_cycles_saved_aggregate':safeget(a,'PIM_PHASE_PROFILE','mac_gap_cycles')-safeget(b,'PIM_PHASE_PROFILE','mac_gap_cycles'),
             'grf_gap_commands_saved_aggregate':safeget(a,'PIM_PHASE_PROFILE','gap_grf')-safeget(b,'PIM_PHASE_PROFILE','gap_grf'),
             'timing_only_blocked_saved_aggregate':safeget(a,'SCHED_COUNTERFACTUAL','timing_only_blocked')-safeget(b,'SCHED_COUNTERFACTUAL','timing_only_blocked'),
             'tCCD_wait_samples_saved_aggregate':safeget(a,'DRAM_TIMING_DIAG','wait_tCCD_or_burst')-safeget(b,'DRAM_TIMING_DIAG','wait_tCCD_or_burst')}
        rows.append(row)
        for sec,fields in SELECTED.items():
            for key in fields:
                av,bv=safeget(a,sec,key),safeget(b,sec,key)
                details.append({'workload':stem,'section':sec,'metric':key,
                                'baseline':av,'cache':bv,'delta_cache_minus_baseline':bv-av,
                                'pct_change':pct_change(av,bv)})
        print('[COMMAND_OVERHEAD_RESULT]', ' '.join(f'{k}={v}' for k,v in row.items()))
    if not rows: raise ValueError(f'No paired scalability logs found in {folder}')
    for name,items in [('command_overhead_summary.csv',rows),('command_overhead_counter_deltas.csv',details)]:
        with (folder/name).open('w',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=list(items[0]));writer.writeheader();writer.writerows(items)
    report=['# Command overhead — command and timing bottleneck comparison',
      '', 'Measured simulator counters from matched scalability runs; all results gated on correctness and MAC-count equality.',
      '', '| GEMV | PIM cycles (base→cache) | Saved | % | GRF-A writes saved | Aggregate MAC-gap samples saved | Aggregate tCCD/burst wait samples saved |',
      '|---|---:|---:|---:|---:|---:|---:|']
    for r in rows:
        report.append(f"| {r['workload']} | {r['baseline_pim_cycles']}→{r['cache_pim_cycles']} | {r['cycles_saved']} | {r['cycle_reduction_pct']}% | {r['grfa_writes_saved']} | {r['mac_gap_cycles_saved_aggregate']} | {r['tCCD_wait_samples_saved_aggregate']} |")
    report += ['', '## Interpretation',
      'The optimization changes GRF-A loading and the number of PIM-mode writes; it does not reduce GEMV MAC work.',
      'Timing diagnostics such as `wait_tCCD_or_burst` and `mac_gap_cycles` are aggregate channel-level observations, not exclusive portions of end-to-end wall-clock cycles.',
      'Do not add these counters together or equate their difference to saved GEMV cycles. Cache-enabled runs include control traffic and a simplified timing/area model.',
      'Inspect `command_overhead_counter_deltas.csv` to compare individual counter differences, including wait, row/bank, and PIM-stream activity.',
      '', '## Next questions',
      'Do the reductions in GRF-related gap commands coincide with fewer PIM mode write commands?',
      'Do the tCCD/burst and row/bank wait sample changes correlate with cycle savings across workloads?',
      'Why is the 3-output-tile workload faster in percentage terms than the 4-output-tile workload despite greater reuse?',
      '', 'This diagnostic comparison cannot by itself establish a causal breakdown of cycle savings or physical SRAM area/energy.']
    (folder/'command_overhead_report.md').write_text('\n'.join(report)+'\n')
    print('[COMMAND_OVERHEAD_SUMMARY]',folder/'command_overhead_summary.csv')
    print('[COMMAND_OVERHEAD_COUNTERS]',folder/'command_overhead_counter_deltas.csv')
    print('[COMMAND_OVERHEAD_REPORT]',folder/'command_overhead_report.md')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',nargs='?',default='scalability_results'); args=p.parse_args()
    try: analyze(Path(args.folder))
    except (ValueError,FileNotFoundError) as exc: print('[COMMAND_OVERHEAD_ERROR]',exc,file=sys.stderr);sys.exit(1)
