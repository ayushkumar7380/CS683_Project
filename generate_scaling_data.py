#!/usr/bin/env python3
"""Deterministic FP16 GEMV reference inputs for one scalability workload.
Use output/input shape arguments, e.g. python3 generate_scaling_data.py 8192 1024.
"""
import argparse
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser()
p.add_argument('output', type=int)
p.add_argument('input', type=int)
a=p.parse_args()
if a.output not in (4096,8192,12288,16384) or a.input not in (512,1024):
    p.error('Supported output: 4096/8192/12288/16384; input: 512/1024')
folder=Path(__file__).resolve().parent/'data'/'gemv'
folder.mkdir(parents=True, exist_ok=True)
rng=np.random.default_rng(1113+a.output*17+a.input)
x=rng.normal(0,0.25,size=(1,a.input)).astype(np.float16)
w=rng.normal(0,0.25,size=(a.output,a.input)).astype(np.float16)
y=(w.astype(np.float32)@x[0].astype(np.float32)).astype(np.float16)[None,:]
for kind,array in [('input',x),('weight',w),('output',y)]:
    np.save(folder/f'gemv_{kind}_{a.output}x{a.input}.npy',array)
print('[SCALABILITY_DATA]',a.output,a.input,'shapes',x.shape,w.shape,y.shape)
