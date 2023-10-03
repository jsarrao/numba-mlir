# SPDX-FileCopyrightText: 2010-2016 Ohio State University
# SPDX-FileCopyrightText: 2021 ETH Zurich and the NPBench authors
# SPDX-FileCopyrightText: 2022 - 2023 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause


from numba_mlir.mlir.benchmarking import parse_config, filter_presets
from os import path
import numpy as np

config = parse_config(path.join(path.dirname(__file__), "config.toml"))
parameters = dict(config["benchmark"]["parameters"])
presets = filter_presets(parameters.keys())


def initialize(TSTEPS, N, datatype=np.float64):
    A = np.fromfunction(lambda i: (i + 2) / N, (N,), dtype=datatype)
    B = np.fromfunction(lambda i: (i + 3) / N, (N,), dtype=datatype)

    return TSTEPS, A, B


def get_impl(ctx):
    jit = ctx.jit
    np = ctx.numpy
    prange = ctx.prange

    @jit
    def kernel(TSTEPS, A, B):
        for t in range(1, TSTEPS):
            B[1:-1] = 0.33333 * (A[:-2] + A[1:-1] + A[2:])
            A[1:-1] = 0.33333 * (B[:-2] + B[1:-1] + B[2:])

    def wrapper(TSTEPS, A, B):
        kernel(TSTEPS, A, B)
        return A, B

    return wrapper
