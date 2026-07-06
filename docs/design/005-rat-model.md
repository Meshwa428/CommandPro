# Design 005 — RAT: Realistic Automated Trajectories

Status: **draft** · Scope: human-like mouse movement model
Phase: 6.5 (between Automation Layer and Package Manager)

> **Fast. Precise. Stealthy. Hard to catch.**
> Hard to catch — because we're making it harder.

RAT is Synapse's bundled mouse movement model. Every `mouse x, y` command
routes through RAT by default. Instead of moving the cursor in a straight
line like every other automation tool, RAT generates trajectories that are
statistically indistinguishable from a real human hand.

---

## 1. The Problem

Modern bot detection (PerimeterX, Akamai, Cloudflare, BeCAPTCHA-Mouse) profiles:

- Movement linearity (humans never move in straight lines)
- Velocity curve shape (multi-peaked, not single bell curve)
- Overshoot rate (~35% of human movements overshoot the target)
- Micro-tremor (sub-pixel noise present throughout all human movement)
- Fitts' Law compliance (time ∝ log(distance/target_size))
- Inter-movement timing variance
- Signal-dependent noise (noise ∝ speed, not additive)

A naive `XTestFakeMotionEvent(x, y)` fails all checks instantly. RAT passes all.

---

## 2. The Reality of Human Mouse Movement

> The simple Bezier + Gaussian noise approach is a cartoon of human movement.
> This section documents what movement actually is.

### 2.1 Sub-movements — The Critical Property

A movement from A to B is NOT one smooth arc. It is a **sequence of overlapping
discrete impulses** (Milner 2004, Rohrer et al. 2002):

```
Real velocity profile:
v │    ╭──╮
  │   ╱    ╲  ╭╮      ← primary + correction sub-movements
  │  ╱      ╲╱  ╲╭
  │─╱             ╲──
  └──────────────────── time

What Bezier gives you (wrong):
v │       ╭────╮
  │──────╱      ╲─────
  └──────────────────── time
```

Detection systems specifically look for the multi-peaked velocity profile.
A single-peak trajectory is an immediate bot signal.

### 2.2 Signal-Dependent Noise (Harris & Wolpert 1998)

Motor command noise scales with magnitude: `σ = k · |u(t)|`

- Fast movements = more noise (large trajectory deviation)
- Slow movements = less noise (precise)

Additive Gaussian noise gets this backwards — it applies equal noise regardless
of speed. Signal-dependent noise is what creates the characteristic jitter
in fast arm sweeps and the precision in slow targeting.

### 2.3 Arm/Wrist Decomposition

| Distance | Mechanism | Trajectory |
|---|---|---|
| < 100px | Wrist only | Tight, low arc, fast |
| 100–400px | Wrist + forearm | Moderate arc, one correction |
| 400–800px | Full arm | Large arc, multi-correction |
| > 800px | Shoulder + arm | Very large, may have stop-restart |

### 2.4 Visual Feedback Loop (~150ms correction cycle)

Eye-to-hand latency: ~100–200ms. For a 400ms movement = 2–3 corrections.
Each correction is a new sub-movement impulse overlapping the previous.
Fast movements (<150ms) are purely ballistic — no correction possible.

### 2.5 Pre-Movement Hesitation

Reaction time: 100–300ms before movement starts, with ±80ms variance.
A bot that always starts at 0ms or at constant 150ms is immediately flagged.

### 2.6 Terminal Behavior (Last 10%)

As cursor approaches target:
- Speed drops 10x
- Path becomes nearly straight (visual guidance overrides)
- Multiple tiny sub-movements cluster around target
- May briefly pause before final twitch

### 2.7 What Bot Detectors Measure

```
Spatial:    curvature, path length ratio, angular velocity changes, direction reversals
Temporal:   velocity peak count, symmetry (rise vs fall time), sub-movement count
Statistical: velocity distribution shape (bimodal?), jerk smoothness, fractal dimension
Biological:  Fitts Law R², signal-dependent noise signature, reaction time variance
```

---

## 3. Algorithm Decision — DEFERRED TO PHASE 6.5

The actual model architecture will be decided when Phase 6.5 begins,
with real training data available to benchmark against detection systems.

### Candidate Architectures

| | WGAN-GP+LSTM | cVAE | Diffusion (DMTG) | Auto-GRU | Physics+ |
|---|---|---|---|---|---|
| Inference speed | ⚠️ ~5ms | ✅ ~2ms | ❌ ~50ms | ✅ ~1ms | ✅ <0.5ms |
| Model size | ⚠️ ~2MB | ✅ ~500KB | ❌ ~5MB | ✅ ~300KB | ✅ <10KB |
| Detection evasion | ✅ Best | ✅ Good | ✅ Best | ⚠️ Good | ⚠️ OK |
| Sub-movement modeling | ✅ Implicit | ⚠️ Partial | ✅ Best | ✅ Natural | ✅ Explicit |
| C++ complexity | ❌ High | ⚠️ Medium | ❌ Very High | ✅ Low | ✅ Low |

**Current lean: Auto-GRU or enhanced Physics+**
State-of-the-art (2024): WGAN-GP+LSTM and Diffusion (DMTG arxiv 2024).

---

## 4. Syntax

```syn
mouse 300, 400           # RAT model, movement time auto-predicted (Fitts)
mouse 300, 400 2s        # take 2 seconds (slow, deliberate)
mouse 300, 400 250ms     # take 250ms (fast)

drag 10, 10, 400, 300    # RAT-path drag (button held during trajectory)

# Stdlib API (method-call forms, for the rarer knobs)
mouse.speed(1.5)                 # global speed bias for this script
mouse.mode("linear")             # bypass RAT (testing only)
mouse.mode("rat")                # back to default
let t = mouse.preview(300, 400)  # inspect without executing
say "~{t.duration_ms}ms, {t.waypoints} waypoints"
```

Movement time is set inline with a trailing **duration literal** (`3s`, `500ms`)
— no comma, no method call — to keep the common case token-cheap. Omit it and
the model predicts a natural time from Fitts' law and the user's calibration.
`mouse.speed` remains a global multiplier for the auto-predicted case.

---

## 5. Per-Session Seed Randomization

Each process start generates a unique seed from hardware entropy
(RDRAND + timestamp + PID → splitmix64). Same `mouse 300, 400` produces a
different curve every run. The model's own trajectory pattern cannot be
fingerprinted across sessions.

---

## 6. Calibration — `syn rat calibrate`

```bash
syn rat calibrate          # full session (100 movements, ~5 min)
syn rat calibrate --quick  # 30-movement fast session
syn rat reset              # delete user weights, back to base model
syn rat status             # show calibration info
```

A native fullscreen overlay appears (X11 on Linux, Win32 on Windows).
Dots appear one at a time. Click them naturally — no pressure, no rush.

After 100 clicks:
- Analytical parameter estimation runs in C++ (O(N), < 100ms)
- Fits Fitts' Law constants (a, b) via linear regression
- Estimates curvature bias, tremor sigma, overshoot rate
- Saves 5 floats (~20 bytes) to `~/.config/synapse/rat_user.bin`

### Weight layers

| Layer | File | Contents |
|---|---|---|
| Base | `$SYNAPSE_INSTALL/models/rat_base.bin` | Trained on general human data, ships with `syn` |
| Personal | `~/.config/synapse/rat_user.bin` | Your calibration — overrides base parameters |

Personal calibration makes movements match **your specific** arm mechanics.

---

## 7. Train/Run Split

Training happens once (developer machine). Inference happens on every
`mouse` command (user machine, no Python required).

```
Training:
  movements.jsonl (recorded human data)
    → tools/rat/train.py (Python/numpy)
    → rat_base.bin (~3KB float16 weights)
    → xxd → rat_weights.h (baked into syn at build time)

Inference:
  mouse 300, 400
    → RatModel::predict()    < 1ms, MLP forward pass
    → RatModel::generate()   Bezier/GRU/VAE trajectory
    → Platform::replay_waypoints()  uinput/X11/Win32
```

C++ inference: ~300 lines, zero dependencies. Weights baked into binary
via `xxd` at build time — no file I/O at runtime for base model.

---

## 8. Phase 6.5 Build Plan

| Step | Work |
|---|---|
| 6.5.1 | Decide algorithm (see §3) — requires training data |
| 6.5.2 | `tools/rat/record.py` — movement data collector (Linux + Windows) |
| 6.5.3 | Record dataset (target: 5,000+ movements) |
| 6.5.4 | `tools/rat/train.py` — training pipeline |
| 6.5.5 | Train + validate `rat_base.bin` |
| 6.5.6 | C++ inference engine `src/rat/rat_model.cpp` |
| 6.5.7 | Platform layer: `replay_waypoints()` on Linux + mock |
| 6.5.8 | `syn rat calibrate` — native fullscreen overlay (Linux) |
| 6.5.9 | Fine-tuning pipeline: user weights → `rat_user.bin` |
| 6.5.10 | `mouse.preview`, `mouse.mode`, `mouse.speed` stdlib API |
| 6.5.11 | Drag command RAT integration |
| 6.5.12 | Conformance tests: assert non-linear, different seeds → different curves |
| 6.5.13 | Benchmark: inference < 5ms, model < 1MB |

---

## 9. Open Questions (resolve at Phase 6.5 start)

1. Windows calibration overlay: Phase 6.5 or post-1.0?
2. Training data: self-collected only, or open public dataset later?
   (Decision: yes, open-source after everything is finalized)
3. Drag: confirmed — uses RAT with button held during trajectory
