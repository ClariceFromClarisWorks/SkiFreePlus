# SkiFree+ build 27 — gameplay visibility improvements

## Delivered behaviour

- Top-left HUD: solid black tick for a passed gate; hollow outline cross for a miss. No flashing or dither. The existing 25-step (one-second) lifetime is retained. The timer shifts right and uses “T:” to make room.
- An 80×80 bordered inset sits at x 416,y 24:16 pixels from the right edge and 8 below the HUD. It shows the current skier sprite at exact 2× pixel scale, without smoothing or invented animation. It appears only when the entire airborne skier has cleared the visible playfield above y 16, and disappears on the first frame of re-entry.
- All 22 enlarged poses are prepared once during startup in a 17,600-byte cache; no pose enlargement occurs during gameplay. Fixed-stride 68000 assembly copies the inset. The existing ground shadow remains. Physics, collisions, camera and 40 ms simulation cadence are unchanged.

## Verification

8,844 host visibility tests cover 22 poses,201 heights andboth normal/yeti camera positions. Queries do not change game state. A separate click-cycle assertion verifies live frames 13→18→19→13 and their exact pose IDs.32,970 original-control comparisons pass. Address/undefined-behaviour sanitizers enabled for those tests. Emulator-side checks validate 22 enlarged poses against independent per-pixel sampling, compare C/assembly copies including guards, and verify underlying scenery restoration and dirty marking after dismissal.

C vs assembly copy and per-pixel vs table-expansion measurements are retained in the raw log. This measures copying only, not pose-cache rebuild, scenery restoration or total frame cost. The playable resource fork grows 757,638→758,679 bytes (+1,041). Final microbenchmarks:5,000 copies 604 ticks in C versus 233 in assembly;220 pose builds 777 ticks per-pixel versus 143 with the expansion table. The final cache uses 17,600 bytes of RAM plus small state variables. Enlarged artwork is generated at startup rather than bundled in the executable.

## Matched normal-speed Snow SE timing

Same 2 MiB/System 6.0.8 configuration and 42-route matrix as build 26, using legacy TIMING 26 CSV field names for comparison. New overlay is enabled in every case. All 12,600 fixed physics updates complete; all 42 terminal state hashes match build 26. Framebuffer and guard checks pass. The airborne route includes synthetic high jumps to exercise off-screen rendering.

| Default bounded scheduler | Build 26 | Build 27 |
|---|---:|---:|
| Normal routes: Timer FPS range | 24.986–25.008 | 24.985–25.022 |
| Normal routes: short / long gaps | 0 / 0 | 0 / 0 |
| Normal routes: combined drawings | 0 | 0 |
| Injected stalls: Timer FPS range | 24.912–25.012 | 24.900–25.010 |
| Injected stalls: short / long gaps | 0 / 42 | 0 / 42 |
| Injected stalls: combined drawings | 2 | 3 |

Short gaps are 0–1 ticks; long gaps 4+ticks. Normal cadence remains 2/3 ticks (~33/50 ms), averaging 25 fps. Injected five-tick stalls intentionally produce long gaps. Combined drawings preserve all physics steps. No real-SE performance guarantee is implied.

## Run integrity

An initial feature run failed the scene 0 state comparison and is retained as first-run-FAILED.txt; its timings are excluded. The replay input path previously allowed live mouseDown events to call game_click despite scripted coordinates. The diagnostic now ignores live gameplay clicks while replay is active; the playable input path is unchanged. Interference is a plausible explanation for the initial failure, not a proven root cause. A later interim run exposed a long jump frame while enlarging poses on demand. A table expansion measured 778→142 ticks for 220 builds; final code prepares every pose at startup to eliminate this live cost. Both interim logs are retained and excluded from final timing summaries. The accepted rerun must complete every correctness gate.

Build 26 binaries and evidence remain preserved. Build 27 native checks and saved-file integrity are recorded below after completion.

The completed saved HFS result file matches the serial capture byte-for-byte. Final shipping-scheduler normal routes:2,100 drawings,zero short/long gaps andzero combined drawings. Forced-stall routes:2,097 drawings for 2,100 updates,3 combined drawings versus 2 in build 26;zero short gaps and 42 deliberately induced long gaps.

## Native visual check

Snow mouse clicks visibly changed the inset through the upright, inverted and next flip poses, then back upright. The serial trace records pose/frame pairs 14/13 → 19/18 → 20/19 → 14/13. Screenshots are included. The test used a separate fixture that held the skier above the field and froze physics so each state could be inspected; it is not part of the playable build and is not a performance measurement. Clearing held altitude dismissed the box; restarting restored the normal field without a leftover inset. Precise re-entry boundaries and scenery restoration are covered by the automated checks above, not a claim of a manually flown landing.

An earlier visual-fixture attempt advanced physics while repeatedly replacing altitude; it reached an invalid actor state. It was discarded and replaced by the frozen-physics fixture. This did not change the shipping game or accepted timing run.

The test-only held-altitude key is not included in the delivered game. The original mouse controls, physics and build 26 clock remain unchanged.
