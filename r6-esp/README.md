# r6-esp

External Direct2D overlay ESP for Rainbow Six Siege. Runs as a separate
process, reads the game over `ReadProcessMemory`, projects bone positions to
screen with the engine's view-projection matrix, and draws boxes / skeletons /
health / names on a transparent top-most window.

## Architecture

```
main.cpp        frame loop: resolve -> read matrix -> walk entities -> W2S -> draw
memory.hpp      process attach, typed RPM, pointer-chain walk, signature scanner
offsets.hpp     per-patch offset config + signatures (the one file you update)
offsets.cpp     runtime signature resolution of the three global roots
entity.hpp      per-frame entity snapshot (health/team/bones/name)
overlay.hpp     layered click-through window + Direct2D/DirectWrite renderer
math.hpp        Vec2/Vec3, 4x4 matrix, WorldToScreen
```

Data model it reads:

```
GameManager ──▶ EntityList[count] ──▶ Entity
                                        ├─ health / maxHealth (float)
                                        ├─ teamId (int)
                                        ├─ deadFlag (byte)
                                        ├─ boneArray ──▶ Transform[bone] (+0x30 = pos)
                                        └─ namePtr ──▶ char[]
ViewMatrix (float[16], column-major VP)
LocalPlayer ──▶ teamId (for the team check)
```

## Build

Needs the Windows SDK (ships with Visual Studio). From a Developer prompt:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build\Release\r6_esp.exe`. Run it **after** the game is up. `END` quits.
Run from an elevated prompt so `OpenProcess` gets `PROCESS_VM_READ`.

## Offsets — the part that moves every patch

`offsets.hpp` is the only file you touch between game versions. Two modes:

- **`USE_SIGNATURES 1` (default):** `offsets.cpp` byte-scans the main module
  for the three roots (`GameManager`, `ViewMatrix`, `LocalPlayer`) and decodes
  the RIP-relative displacement to an absolute address. When a pattern stops
  resolving (it prints `0000000000000000`), open the current `RainbowSix.exe`
  in a disassembler, find the cross-reference to the singleton, copy ~12–18
  bytes with wildcards over the relative dword, and replace the string.

- **`USE_SIGNATURES 0`:** hardcode absolute RVAs you already dumped into the
  `RVA_*` constants.

The struct field offsets (`HEALTH`, `TEAM_ID`, `BONE_ARRAY`, bone indices,
etc.) are the values to re-verify against the live build: attach a debugger,
find the entity base, and walk the struct. They're grouped at the top of
`offsets.hpp` with comments on type and meaning so updating is mechanical.

## Finding offsets from scratch

1. **ViewMatrix:** standard trick — find the matrix by searching for the
   float set whose `WorldToScreen` output tracks an on-screen object as you
   rotate. Cheat Engine's pointer/structure dissect plus a known world point
   nails it fastest.
2. **EntityList:** scan for the array of pointers whose entries all land in
   the same heap region and expose a readable health float at a stable offset.
3. **Bones:** from an entity, the bone array pointer leads to a run of
   4x4 transforms; translation sits at `+0x30` of each. Index 8 is usually the
   head — confirm by drawing a dot at each index and watching which lands on
   the head model.

## Tuning

Feature toggles live in `Config` in `main.cpp` (boxes, skeleton, health,
names, snaplines, team check). Box aspect ratio, bar width, font, and colors
are inline in the draw loop and the overlay helpers.
