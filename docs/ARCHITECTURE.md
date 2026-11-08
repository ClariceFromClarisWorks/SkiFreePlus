# How the compact Mac port works

## Ownership and boundaries

`engine.c` holds the recovered gameplay rules. `sim.c` adapts these rules to fixed storage, input and a draw list. `gfx.c` owns the persistent 1-bit canvas and damage tracking. `app.c` owns Macintosh events, native dialogs, HUD composition and presenting frames. Historical decompiler names remain where their meaning is uncertain.

Keeping camera offsets out of physics prevents look-ahead and the yeti reveal from changing collisions. The skier normally appears high on the small screen; the camera eases toward a lower screen position when the yeti approaches. The original collision world is not shrunk to fit the screen.

## Time and perceived speed

Each gameplay update represents 40 ms. The compact Mac’s approximately 60.15 Hz tick cannot represent that interval exactly. `deadline_clock.h` distributes two- and three-tick deadlines with integer arithmetic. It retains overdue simulation steps, bounds catch-up work, and prevents consecutive presentations from bursting one tick apart. Modal dialogs and backgrounding reset the deadline epoch so returning does not fast-forward through time spent away.

Drawing fewer intermediate states under overload can still look choppy. Nominal 25 fps, observed presentations, and simulation updates are different quantities. Neither a short FPS sample nor emulator wall-clock speed establishes hardware pacing. The D report is intended to expose the distinction.

## Renderer and assembly

The screen is 64 bytes per row. Damage maps use 16 × 16 pixel tiles; old sprite footprints and removed overlays must restore the scenery beneath them. Horizontal and vertical runs reduce transfer overhead. The operating-system cursor is hidden around direct presentation and restored afterward.

Generated sprite code selects one of 16 bit phases using a relocatable resource offset table. Clipped edges use masked blitting. Precomputation costs memory but avoids shifting and decoding every frame. A separate wide-title cache avoids enlarging every ordinary sprite slot.

`kernels.S` contains hand-written baseline-68000 routines. Stack argument offsets include each routine’s register saves. Callers guarantee alignment, valid dimensions and byte-count multiples. Computed-entry unrolled loops depend on exact instruction sizes; inserting instructions in those sleds changes their meaning. Preserve C/reference paths and compare identical workloads before selecting assembly: assembly is not automatically faster.

The jumpcam displays the actual airborne pose once the whole skier leaves the playfield. Its 22 enlarged images are cached at startup (17,600 bytes). Steady-state work copies an 80 × 80 1-bit image; it neither scales nor allocates. Dirty tracking removes the box when the skier re-enters. Native System fonts are rasterised for the HUD cache, avoiding per-frame Toolbox text work.

## Maintaining generated art

`generate_assets.py` reads bitmap resources from a user-supplied original executable, using Pillow and pefile; it does not execute that program. `generate_banks.py` packages generated assembly into the relocatable resource. The generated files are committed so normal builds need neither the original executable nor those Python packages. Regenerating art is a functional change: inspect silhouettes and semantic monochrome marks, then rerun pixel-reference and timing comparisons. Do not format assembly with a C formatter.

## Scope

The compact edition prioritises baseline 68000 monochrome hardware. Colour, larger windows and more capable machines belong to separate development. Avoid imposing their memory and code costs on this edition. No generic graphics-engine extraction is required for this project.
