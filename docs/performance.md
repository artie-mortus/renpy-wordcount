# Performance checks

The opt-in `say-count-performance` executable measures representative core workloads without
adding timing-sensitive assertions to the normal unit-test suite. Its generated data covers 500
scripts, 5,000 labels, project analysis, diagnostics, and completion indexing. Navigation and route
analysis use representative subsets to keep the CI check bounded. The completion measurements cover
both a cold 500-file rebuild and the cached single-document update used while typing.

Build and report measurements:

```sh
cmake --build build -j2 --target say-count-performance
./build/say-count-performance
```

Run the generous regression thresholds used by CI:

```sh
cmake --build build -j2 --target performance-check
```

The limits are intended to catch order-of-magnitude regressions on shared CI runners, not small
machine-to-machine variation. Update a limit only with before/after measurements and an explanation
in the change that requires it.
