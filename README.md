# SkiFree+

A monochrome compact-Mac continuation of SkiFree, targeting the Macintosh SE and System 6 at a nominal 25 simulation steps per second. The fullscreen playfield is 512 × 342. Physical SE testing is encouraging; consistent timing on every supported configuration is not established.

This repository preserves the history of [jeff-1amstudios/skifree_sdl](https://github.com/jeff-1amstudios/skifree_sdl), which derives from [Eric Fry’s decompilation](https://github.com/yuv422/skifree_decomp). Original SkiFree is by [Chris Pirih](https://ski.ihoc.net/). This is a community continuation, not an official release by those authors.

The Macintosh implementation is in `ports/macintosh/`. The original upstream tree is retained for history and comparison; its README is `README.upstream.md`.

## System requirements

- **2 MB RAM minimum installed; 1 MB configurations are not supported.**
- 512 × 342 monochrome compact-Mac display; Macintosh SE is the primary target. Macintosh Plus compatibility still needs current-build hardware confirmation.
- System 6.0.8 is the tested operating-system configuration.

The application requests a 1.5 MB memory partition (both minimum and preferred). The remaining RAM must accommodate the System and Finder. The 2 MB configuration has been exercised in emulation; extensions and MultiFinder applications can leave too little free memory, so additional installed RAM may be needed in those setups.

## Build and test

Run `./scripts/build-macintosh.sh` with Docker available. It uses a pinned Retro68 image and creates `build/macintosh/SkiFreePlus.bin` (MacBinary application) and `SkiAuto.bin` (automated diagnostic application), with raw Retro68 disk outputs alongside them. Use the MacBinary files for installation: the script repairs their Finder bundle flags for icon recognition; it does not retrofit those flags inside the raw generated floppy images. ROMs and operating-system disks are not needed to compile and are not included. Asset regeneration is optional; the generated sprite banks are checked in.

Run `python3 scripts/test-host.py` with Clang installed for deterministic simulation, controls, camera, HUD, and scheduler checks. These are correctness checks, not Macintosh performance measurements.

See [architecture](docs/ARCHITECTURE.md), [validation status](docs/VALIDATION.md), and [provenance and rights status](docs/PROVENANCE.md) before preparing a public release.

## Controls

Mouse or arrows steer. Mouse click jumps or changes the airborne trick; Space uses the original distinct keyboard hop/trick behaviour. F enables fast skiing. P/F3 pauses, R/F2 restarts, 1–4 choose courses, H opens scores, I toggles FPS, D starts/stops a diagnostic recording, F1 opens About, and Escape quits.

## FPS and community timing reports

The playable release retains both diagnostics—no special benchmark build or serial connection is needed:

- Press **I** to toggle the live FPS display.
- Press **D** to begin recording timing, play normally, then press **D again** to open the timing report. Photograph the report before selecting **Resume**. Paused time is excluded.
- Press **F1** for About, which also lists the diagnostic shortcuts.

If you would like to help with measurements, a minute or two of representative play is useful. Please include diagonal skiing, jumps and chairlift sections where possible, and describe what you played. Reports from a Macintosh Plus are particularly welcome; the SE remains the primary target.

Alongside a clear photograph of the complete report, please provide:

- Macintosh model and CPU speed, including any accelerator.
- Installed RAM and System version.
- Whether MultiFinder, extensions or other applications were running.
- Game build number, course and approximate recording duration.
- Any visible stuttering and what was happening when you noticed it.
- Whether this was physical hardware or an emulator; for emulation, include its name and speed settings.

### Reading the report

Average FPS describes the whole recorded sample; FPS windows show shorter-term variation. The game advances in fixed 40 ms simulation steps, but the Mac's tick clock means normal presentations alternate between two- and three-tick intervals. The live number therefore need not remain exactly 25.

Long gaps indicate presentations separated by four or more ticks; short gaps indicate zero or one tick. Combined updates mean the simulation advanced more than once before displaying the next picture, preserving game progression while omitting an intermediate drawing. “Behind schedule” is a count of frames with scheduling debt, **not a count of dropped frames**. A photograph is more useful than reporting only the lowest live FPS number.

A physical SE build 27 recording measured **24.3 fps over 65.3 seconds**, with short-window readings of **22.3–25.5 fps**, **47 long gaps** (about 3% of measured intervals), and **zero short gaps**. The longest gap was about 83 ms. This is an example from one session, not a performance guarantee for every machine or course. Occasional stutters remain; the current performance is accepted for this release. Further community measurements can guide future work.

## Credits

- Chris Pirih: original SkiFree game and artwork.
- Eric Fry: C decompilation; jeff-1amstudios: SDL source port.
- ClariceFromClarisWorks: project direction, design feedback and physical Macintosh testing.
- OpenAI Codex: Macintosh port implementation, optimisation, tooling and documentation, through AI-assisted development directed and tested by ClariceFromClarisWorks.

This is a source-release candidate. See the validation and provenance notes for outstanding checks; it is not a claim of complete original-game equivalence.
