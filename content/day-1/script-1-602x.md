# Script 1 — "602x" (the banger)

- **Length:** ~40s
- **Format:** Faceless — terminal capture, benchmark bar race, voiceover
- **Hook pattern:** The Unexpected + Data-Driven
- **Goal:** Series opener. Strongest cold-audience hook. Post FIRST.

## Verified numbers used

| Benchmark | Result |
|---|---|
| loop | Synapse 2.53ms vs Python 1525.72ms — **602x faster** |
| gc_pressure | Synapse 2.44ms vs C++ 3.99ms — **1.64x faster than C++** |

Outputs verified: JIT = SYN_NO_JIT=1 = Python on all claimed benchmarks.

## Shot list

| Time | Visual | VO |
|---|---|---|
| 0-2s | Black terminal. Cursor blinks. Type: `well hello ladies and mentlegen` | "Well hello ladies and mentlegen." |
| 2-5s | Hard cut: two progress bars labeled PYTHON / SYNAPSE. OSD: **"602x"** | "I built my own programming language. And it's six hundred times faster than Python." |
| 5-12s | Bars race. Synapse bar slams to 100%. Python bar crawls. Real timer numbers on screen. | "Same loop. One billion iterations. Python: 1.5 seconds. Mine: two f*cking milliseconds." (bleep the swear in edit; screen shows exact 2.53ms) |
| 12-20s | Quick cuts: code editor with `.syn` file, `git log` scrolling, `build OK` | "Fifteen thousand lines of code. A bytecode VM. A JIT compiler. Built from scratch." (verified: 15,116 lines in src/ + include/. Say "code" not "C++" — saves first C++ mention for the payoff at 20-30s) |
| 20-30s | Benchmark table scrolls. Freeze on gc_pressure row. Red circle. | "And on this benchmark — YES — it beat C++. Actual. C++." |
| 30-38s | Terminal: rerun command live, same result prints. | "I didn't believe it either. So I ran it five times. Verified against Python. Same answer." |
| 38-42s | Cut to black. OSD: **"Part 1. It gets worse for Python."** | "Follow — because next video, I show you how." |

## Notes

- **Loop trick:** end frame (black + cursor) matches opening frame — rewatches count as retention.
- OSD "602x" must be huge, on screen by second 3. VO says "six hundred" (comfortable to speak); screen shows exact 602x — voice understates, screen proves more. Credibility bonus.
- Timer numbers on bar race must be real benchmark output, not mocked.
