"""Diffusion denoiser for the deviation-from-line trajectory (DMTG-inspired).

The signal is a fixed-length, endpoint-pinned 2-channel sequence (dx,dy deviation
from the straight line) — 2*N = 128 numbers, smooth and low-frequency. That is a
tiny, low-rank signal, so a plain residual MLP with FiLM conditioning denoises it
with ~1000x fewer FLOPs than a conv/attention net and trains in seconds on CPU.
No translation-equivariance is needed here: positions carry meaning (the endpoints
are special), which suits a dense net fine.

Conditioning (t, log-distance, alpha-complexity, persona) is summed into one
vector and injected as per-block FiLM (scale+shift). Persona is where per-user
calibration attaches later.

Endpoint pinning: the true deviation is 0 at both ends, so at every sampling step
we clamp the reconstructed x0 endpoints to 0 -> the path is guaranteed to start at
the source and land exactly on the target.
"""
from __future__ import annotations
import math
import torch
import torch.nn as nn
import torch.nn.functional as F


def make_beta_schedule(T: int, device=None):
    # cosine schedule (Nichol & Dhariwal): better low-noise behavior than linear
    s = 0.008
    steps = torch.arange(T + 1, device=device, dtype=torch.float64)
    f = torch.cos(((steps / T) + s) / (1 + s) * math.pi / 2) ** 2
    acp = (f / f[0]).clamp(1e-6, 1.0)
    betas = (1 - acp[1:] / acp[:-1]).clamp(1e-6, 0.999)
    alphas = 1.0 - betas
    return betas.float(), alphas.float(), torch.cumprod(alphas, 0).float()


class SinusoidalEmbed(nn.Module):
    def __init__(self, dim: int):
        super().__init__()
        self.dim = dim

    def forward(self, t):
        half = self.dim // 2
        freqs = torch.exp(-math.log(10000) * torch.arange(half, device=t.device) / half)
        args = t.float()[:, None] * freqs[None]
        return torch.cat([torch.sin(args), torch.cos(args)], dim=-1)


class ResBlock(nn.Module):
    """FiLM-conditioned residual MLP block."""
    def __init__(self, width: int, cond_dim: int):
        super().__init__()
        self.norm = nn.LayerNorm(width)
        self.fc1 = nn.Linear(width, width)
        self.film = nn.Linear(cond_dim, 2 * width)
        self.fc2 = nn.Linear(width, width)

    def forward(self, h, cond):
        x = self.fc1(self.norm(h))
        scale, shift = self.film(cond).chunk(2, dim=-1)
        x = F.silu(x * (1 + scale) + shift)
        return h + self.fc2(x)


class TrajDenoiser(nn.Module):
    def __init__(self, n_personas: int, seq_len: int, d_model: int = 256,
                 n_layers: int = 4, n_heads: int = 4):  # n_heads kept for ckpt compat
        super().__init__()
        self.seq_len = seq_len
        c = d_model
        self.t_embed = SinusoidalEmbed(c)
        self.t_mlp = nn.Sequential(nn.Linear(c, c), nn.SiLU(), nn.Linear(c, c))
        self.dist_mlp = nn.Sequential(nn.Linear(1, c), nn.SiLU(), nn.Linear(c, c))
        self.alpha_mlp = nn.Sequential(nn.Linear(1, c), nn.SiLU(), nn.Linear(c, c))
        self.persona = nn.Embedding(n_personas, c)

        self.n_ch = 2  # dev_x, dev_y (timing dt is handled separately, see data/generate)
        self.in_proj = nn.Linear(self.n_ch * seq_len, c)
        self.blocks = nn.ModuleList(ResBlock(c, c) for _ in range(n_layers))
        self.out_norm = nn.LayerNorm(c)
        self.out = nn.Linear(c, self.n_ch * seq_len)
        nn.init.zeros_(self.out.weight); nn.init.zeros_(self.out.bias)

    def forward(self, x, t, dist_log, alpha, persona):
        B = x.shape[0]
        cond = (self.t_mlp(self.t_embed(t)) + self.dist_mlp(dist_log)
                + self.alpha_mlp(alpha) + self.persona(persona))          # (B,c)
        h = self.in_proj(x.reshape(B, -1))
        for b in self.blocks:
            h = b(h, cond)
        return self.out(F.silu(self.out_norm(h))).reshape(B, self.seq_len, self.n_ch)


class Diffusion:
    """DDPM training loss + DDIM sampling with endpoint pinning."""
    def __init__(self, model: TrajDenoiser, T: int = 1000, device="cpu"):
        self.model = model
        self.T = T
        self.device = device
        self.betas, self.alphas, self.acp = make_beta_schedule(T, device)

    def loss(self, x0, dist_log, alpha, persona):
        B = x0.shape[0]
        t = torch.randint(0, self.T, (B,), device=self.device)
        noise = torch.randn_like(x0)
        a = self.acp[t][:, None, None]
        xt = a.sqrt() * x0 + (1 - a).sqrt() * noise
        pred = self.model(xt, t, dist_log, alpha, persona)
        return ((pred - noise) ** 2).mean()

    @torch.no_grad()
    def sample(self, dist_log, alpha, persona, seq_len, steps: int = 50):
        B = dist_log.shape[0]
        x = torch.randn(B, seq_len, 2, device=self.device)
        x[:, 0] = 0.0; x[:, -1] = 0.0
        ts = torch.linspace(self.T - 1, 0, steps, device=self.device).long()
        for i in range(len(ts)):
            t = ts[i].expand(B)
            a = self.acp[t][:, None, None]
            eps = self.model(x, t, dist_log, alpha, persona)
            x0 = (x - (1 - a).sqrt() * eps) / a.sqrt().clamp(min=1e-5)
            x0 = x0.clamp(-3.0, 3.0)                        # threshold: stops high-noise
            x0[:, 0] = 0.0; x0[:, -1] = 0.0                 # blowup; pin endpoints
            if i < len(ts) - 1:
                a_next = self.acp[ts[i + 1]]
                x = a_next.sqrt() * x0 + (1 - a_next).sqrt() * eps
            else:
                x = x0
        return x
