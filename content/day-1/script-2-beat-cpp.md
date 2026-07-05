# Script 2 — "It beat C++" (fight-starter)

- **Length:** ~35s
- **Format:** Faceless — terminal capture, side-by-side comparison, voiceover
- **Hook pattern:** Opposite/Contrarian
- **Goal:** Engagement engine. Angry C++ devs quote/comment; shown losses turn them into defenders. Post SECOND.

## Verified numbers used

| Benchmark | Result |
|---|---|
| gc_pressure | Synapse 2.44ms vs C++ 3.99ms — **1.64x faster than C++** |
| binary_trees | Synapse 22.95ms vs C++ 21.68ms — 1.06x slower (6% behind) |

Full table (release build, median of 5, Python 3.14.6):

| Benchmark | C++(ms) | Syn(ms) | Py(ms) | Syn vs C++ | Syn vs Py |
|---|---|---|---|---|---|
| fib | 2.59 | 4.12 | 121.91 | 1.59x slower | 29.6x faster |
| loop | 1.30 | 2.53 | 1525.72 | 1.94x slower | 602x faster |
| strings | 1.23 | 2.99 | 45.82 | 2.44x slower | 15.3x faster |
| binary_trees | 21.68 | 22.95 | 85.32 | 1.06x slower | 3.7x faster |
| json_like | 1.59 | 3.96 | 18.09 | 2.49x slower | 4.6x faster |
| string_building | 2.78 | 7.35 | 19.51 | 2.64x slower | 2.7x faster |
| gc_pressure | 3.99 | 2.44 | 110.99 | **1.64x FASTER** | 45.5x faster |

## Shot list

| Time | Visual | VO |
|---|---|---|
| 0-2s | gc_pressure row, huge. OSD: **"my toy language beat C++"** | "My homemade programming language just beat C++ and I can't fully explain it." |
| 2-6s | Terminal types the catchphrase, then `./syn gc_pressure.syn` | "Ladies and mentlegen — receipts." |
| 6-15s | Side-by-side: C++ 3.99ms vs Synapse 2.44ms. Rerun live. | "GC stress test. C++: about 4 milliseconds. Mine: 2.4. Every. Single. Run." |
| 15-24s | Full table scrolls, losses visible, red highlights on slower rows | "Before you type the angry comment — no, it doesn't beat C++ everywhere. Here's every loss." |
| 24-32s | Zoom on binary_trees row: 1.06x | "But look how close the rest is. 6% behind C++. In a language I wrote in my bedroom." |
| 32-35s | Black. OSD: **"How? Next video."** | "How? That's the next one." |

## Notes

- **Honesty is the engagement engine.** Losses ON SCREEN = comment section defends you instead of debunking you.
- Langdev crowd audits benchmarks for sport. Show the full table, always.
- Title bait works because the claim is TRUE for gc_pressure — never trim that qualifier out of the video body.
