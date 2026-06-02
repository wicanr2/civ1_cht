# CTest Verification — Round 3

Background headless verification run for branch `track-b-cpp-sdl2-port`.

## Environment
- Working tree: `/home/anr2/civ1_cht/civ1_cht/openciv1pp`
- Branch: `track-b-cpp-sdl2-port`
- HEAD commit: `def1dd51be79286ea851cbba0a0adea82e734e6d`
- Compiler: GNU 11.4.0
- SDL2: 2.0.20, FreeType2: 24.1.18

## 1. Clean rebuild
- `rm -rf build && cmake -S . -B build && cmake --build build -j`
- Result: build succeeded, target `openciv1pp` linked.
- **Build warnings: 0** (grep `-iE "warning:|warning "` against full build log → 0 lines)

## 2. ctest run 1 (verbose, --output-on-failure)
- Log: `/tmp/ctest_r1.txt`
- **Result: 54/54 passed, 0 failed**
- Total wall time: **0.79 sec**

## 3. ctest run 2 — stability (`--repeat until-fail:5`)
- Log: `/tmp/ctest_r2.txt`
- ctest version supports `--repeat until-fail:N`; each of the 54 tests ran 5 times.
- "Passed" lines in output: **270** (= 54 × 5)
- "Failed" lines: **0**
- Total wall time: **5.72 sec**
- **Flakes: none**

## 4. Per-test timing — 5 slowest
(parsed from `/tmp/ctest_timing.txt`)

| Rank | Test          | Time (sec) |
|------|---------------|------------|
| 1    | commontest    | 0.07       |
| 2    | wondertest    | 0.05       |
| 2    | slidertest    | 0.05       |
| 2    | huttest       | 0.05       |
| 2    | barbtest      | 0.05       |

(`aiexpandtest` also at 0.05; four-way tie for the #2 slot.)

All tests complete in under 0.10 sec — the suite is comfortably fast and uniform.

## 5. Final verdict
**PASS**

- 0 build warnings.
- 54/54 tests green on a single run and across 5 repeat iterations (270/270).
- No flakes, no slow outliers.
- HEAD: `def1dd51be79286ea851cbba0a0adea82e734e6d`.
