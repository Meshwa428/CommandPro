# Script 3 — "Speedrun the build" (series glue)

- **Length:** ~55s
- **Format:** Faceless — fast montage, code flythrough, git log scroll, voiceover
- **Hook pattern:** Achievement with Constraint / Behind-the-Scenes
- **Goal:** Maps the whole series; every step becomes its own future video. Post THIRD.

## Shot list

| Time | Visual | VO |
|---|---|---|
| 0-2s | Fast montage teaser: final benchmark table flash | "I built a programming language from scratch. Here's the whole thing in 50 seconds." |
| 2-6s | Catchphrase typed in terminal | "Well hello ladies and mentlegen. Speedrun. Go." |
| 6-14s | Parser code flying by (`src/frontend/parser.cpp`), `.syn` file side-by-side | "Step one: parser. Text becomes a tree." |
| 14-22s | Bytecode dump scrolling | "Step two: compiler. Tree becomes bytecode." |
| 22-32s | VM loop code (`src/runtime/vm.cpp`), then JIT file (`src/backend/jit.cpp`) | "Step three: a VM executes it. Then a JIT turns hot code into raw machine code. This is where the speed lives." |
| 32-42s | `git log --oneline` scroll: interning, flat int hash commits | "Then months of this. String interning. Hash table rewrites. Each commit, a little faster." |
| 42-52s | Bars race: Python vs Synapse. 602x lands. | "Result: six hundred times faster than Python on loops. Sometimes faster than C++." |
| 52-55s | Black. OSD: **"Every step gets its own video. Follow."** | "Every one of these steps gets its own video." |

## Notes

- Real commits to show in git log scroll: `perf: flat int hash for ObjMap + INLINE_CAP=4 for ListItems`, `perf: string interning via open-addressing hash table`.
- "Sometimes faster than C++" — qualifier stays. gc_pressure only.
- Each step (parser / compiler / VM / JIT / perf commits) = future standalone video. This script is the series table of contents.
