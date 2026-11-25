#!/usr/bin/env python3
"""Generate deterministic nonconstant GEMV reference data.

Creates only data/gemv/*_16384x1024.npy. Unlike performance dummy data,
this verifies the PIM output against a genuine numerical reference.
"""
from pathlib import Path
import numpy as np

dim_out, dim_in = 16384, 1024
rng = np.random.default_rng(1113)
folder = Path(__file__).resolve().parent / 'data' / 'gemv'
folder.mkdir(parents=True, exist_ok=True)
x = rng.normal(0, 0.25, size=(1, dim_in)).astype(np.float16)
w = rng.normal(0, 0.25, size=(dim_out, dim_in)).astype(np.float16)
y = (w.astype(np.float32) @ x[0].astype(np.float32)).astype(np.float16)[None, :]
np.save(folder / f'gemv_input_{dim_out}x{dim_in}.npy', x)
np.save(folder / f'gemv_weight_{dim_out}x{dim_in}.npy', w)
np.save(folder / f'gemv_output_{dim_out}x{dim_in}.npy', y)
print('Generated deterministic nonconstant reference data: ', x.shape, w.shape, y.shape)
