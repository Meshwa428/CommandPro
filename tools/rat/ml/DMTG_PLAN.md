# DMTG-inspired redesign — plan

## Why the current model fails (measured)

`sample.py` on the trained checkpoint:

| metric        | generated | real  | verdict |
|---------------|-----------|-------|---------|
| curvature     | **1.006** | 0.145 | 7× too wild |
| overshoot     | 0.690     | 0.344 | 2× |
| peak_time     | 0.490     | 0.282 | peaks mid-path, humans peak early |
| dt_median_ms  | 18.3      | 17.0  | fine |

The generated plot shows paths shooting to x≈30 with a fat vertical spread — they
**never converge on the target**. Real paths sit tidily in [(0,0),(1,0)].

Root cause: we predict **absolute canonical (x,y)** for all 96 points with a
whole-sequence noise transformer, but *nothing constrains the endpoint*. With only
6715 movements the model can't learn "always land exactly at (1,0)", so it doesn't.
`generate.py` papers over it by force-overwriting the last point, but the path
leading there is garbage. This is the classic diffusion-trajectory failure the
DMTG paper solves structurally.

## What DMTG actually does (the 3 load-bearing ideas)

1. **Endpoints are pinned into the input, not learned.** Their Eq.(3) builds the
   diffusion input `X_R = {⟨x0,y0⟩} ‖ {p_c+ε_i}ᵢ ‖ {⟨xm,ym⟩} ‖ {⟨0,0⟩}` — start and
   end nodes are hard-set; only the *middle* is noised (around the midpoint `p_c`).
   Diffusion never has to invent the endpoints → paths converge. This is the fix.

2. **α = a complexity/entropy knob for controllable randomness.** They prove
   trajectory entropy `H ≈ log(MST length) ≈ log(path length)` (Eq.7–9), so a single
   scalar α controls how wiggly the curve is. Sample α∈[0.3,0.8] → human-like spread;
   α→1 straight, α→0 scrawl (their Fig.4). For an *ordered* single trajectory the MST
   length **is** the path length, so we get their exact handle with **α = pathlen/chord**
   — no U-Net, no entropy graph needed.

3. **Direction/acceleration asymmetry** (their Fig.5): humans accelerate differently
   up vs down. Falls out for free once we model per-axis deviation + real dt; not a
   separate mechanism.

## The redesign (minimal, DMTG-grounded)

Keep: the small transformer-diffusion backbone (0.34M params is right-sized for 6.7k
samples), `data.py` canonicalization, persona embedding, the `generate()` API shape.

Change three things:

### 1. Representation: deviation from the straight line (biggest fix)
Instead of absolute (x,y), reparametrize each movement to a fixed N points and model:

```
point_i = lerp(start, target, s_i) + n_i * perp        # s_i ∈ [0,1] along-progress
model predicts: (Δs_i residual, perp_offset_i, dt_i)
with s_0=0, s_N=1, perp_0=perp_N=0 pinned by construction
```

Endpoints are exact **by construction** (this is DMTG idea #1, done in the
representation instead of at sample time — simpler than RePaint clamping). The learning
target is now small bounded deviations, not "hit an absolute point 800px away", which is
learnable from 6.7k samples. Curvature is naturally bounded → should land near real 0.145.

*Fallback if reparametrization is fiddly:* keep absolute (x,y) but **clamp x[0]=start,
x[L-1]=target at every DDIM step** (inpainting). ~10 lines, no data change. Try this
first as a smoke test; go to deviation-rep if curvature is still high.

### 2. α complexity conditioning (controllable randomness)
- Add `α = clip(pathlen/chord, 1, 4)` per movement as a **conditioning input** (alongside
  dist + persona), same MLP treatment as dist.
- At inference sample α from the real empirical distribution (median 1.21, p90 3.8), or
  expose it as the `movement_intent` knob the user asked for (low α = decisive, high α =
  hesitant/searching).
- This is the paper's whole contribution, reduced to one extra scalar. It also gives the
  per-persona/per-intent style control for free.

### 3. Timing: recover the early velocity peak
Peak-time is off (0.49 vs 0.28) because dt is modeled independently of progress. Condition
dt on along-progress `s_i` (already available in the new rep) so the model learns the
Fitts profile: fast ballistic phase early, slow correction near the target. No new
mechanism — just feed `s_i` to the dt head.

## Explicitly skip (YAGNI / not our use case)
- **Full U-Net + entropy/MST computation graph** — α=pathlen/chord replaces it. Add only
  if deviation-rep curvature control proves insufficient.
- **1M-sample augmentation on A100** — we have 6.7k on CPU; the representation fix is what
  makes 6.7k enough, not more data.
- **Browser proxy bot / CAPTCHA black-box eval / fingerprint spoofing** (their §3.4, Fig.1
  bottom) — that's their *attack* goal. Ours is realistic automation; our eval is the
  `sample.py` metric table (curvature/overshoot/peak_time vs real). Keep that as the DoD.

## Inputs / outputs (unchanged API)
```
generate(x0,y0, x1,y1, persona=id, alpha=None, ddim_steps=50) -> [(x,y,t_ms), ...]
```
`alpha=None` → sample from real distribution. Screen size / DPI: only needed if we later
train cross-resolution; canonical frame already makes the model resolution-independent, so
skip unless a persona is DPI-specific.

## Order of work + Definition of Done
1. Add α to `data.py` + inpainting-clamp in `model.sample` → retrain → check curvature drops.
2. If still high, switch to deviation-from-line representation.
3. Add progress-conditioned dt → check peak_time ≈ 0.28.
4. **DoD:** generated vs real within ~30% on all four `sample.py` metrics, and the
   generated plot shows paths that visibly converge on (1,0). Wire α through `generate.py`.
