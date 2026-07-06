"""History-conditioned diffusion denoiser for mouse trajectories.

The trajectory is a sequence of (dx, dy, dt) steps. A DDPM adds Gaussian noise to
the whole sequence; a Transformer learns to predict that noise. Self-attention
means every step attends to every other, so the model reasons about the ENTIRE
trajectory it is producing ("knows its past/future path"), not just the next
step in isolation.

Conditioning per denoising step:
  * diffusion timestep t   (sinusoidal embedding)
  * movement distance      (log-px, MLP)
  * persona                (learned embedding, one per participant) — fixing it
                            for a session gives a consistent movement style.

The persona embedding is where per-user LoRA calibration will later attach: the
base model is frozen and a small adapter shifts the persona/conditioning path.
"""
from __future__ import annotations
import math
import torch
import torch.nn as nn


def make_beta_schedule(T: int, device=None):
    betas = torch.linspace(1e-4, 0.02, T, device=device)
    alphas = 1.0 - betas
    acp = torch.cumprod(alphas, dim=0)
    return betas, alphas, acp


class SinusoidalEmbed(nn.Module):
    def __init__(self, dim: int):
        super().__init__()
        self.dim = dim

    def forward(self, t):  # t: (B,) int/float
        device = t.device
        half = self.dim // 2
        freqs = torch.exp(-math.log(10000) * torch.arange(half, device=device) / half)
        args = t.float()[:, None] * freqs[None]
        return torch.cat([torch.sin(args), torch.cos(args)], dim=-1)


class TrajDenoiser(nn.Module):
    def __init__(self, n_personas: int, seq_len: int, d_model: int = 128,
                 n_layers: int = 6, n_heads: int = 4):
        super().__init__()
        self.seq_len = seq_len
        self.in_proj = nn.Linear(3, d_model)
        self.pos = nn.Parameter(torch.randn(1, seq_len, d_model) * 0.02)

        self.t_embed = SinusoidalEmbed(d_model)
        self.t_mlp = nn.Sequential(nn.Linear(d_model, d_model), nn.SiLU(), nn.Linear(d_model, d_model))
        self.dist_mlp = nn.Sequential(nn.Linear(1, d_model), nn.SiLU(), nn.Linear(d_model, d_model))
        self.persona = nn.Embedding(n_personas, d_model)

        layer = nn.TransformerEncoderLayer(d_model, n_heads, dim_feedforward=4 * d_model,
                                           dropout=0.0, batch_first=True, activation="gelu")
        self.encoder = nn.TransformerEncoder(layer, n_layers)
        self.out = nn.Linear(d_model, 3)

    def forward(self, x, t, dist_log, persona):
        # x: (B,L,3) noisy steps; t: (B,); dist_log: (B,1); persona: (B,)
        cond = self.t_mlp(self.t_embed(t)) + self.dist_mlp(dist_log) + self.persona(persona)  # (B,d)
        h = self.in_proj(x) + self.pos + cond[:, None, :]
        h = self.encoder(h)
        return self.out(h)  # predicted noise (B,L,3)


class Diffusion:
    """DDPM training loss + DDIM sampling."""
    def __init__(self, model: TrajDenoiser, T: int = 1000, device="cpu"):
        self.model = model
        self.T = T
        self.device = device
        self.betas, self.alphas, self.acp = make_beta_schedule(T, device)

    def loss(self, x0, dist_log, persona, mask):
        B = x0.shape[0]
        t = torch.randint(0, self.T, (B,), device=self.device)
        noise = torch.randn_like(x0)
        a = self.acp[t][:, None, None]
        xt = a.sqrt() * x0 + (1 - a).sqrt() * noise
        pred = self.model(xt, t, dist_log, persona)
        se = (pred - noise) ** 2 * mask[:, :, None]  # ignore padding
        return se.sum() / mask.sum().clamp(min=1) / 3.0

    @torch.no_grad()
    def sample(self, dist_log, persona, seq_len, steps: int = 50):
        """DDIM sampling. Returns x0_hat (B, seq_len, 3)."""
        B = dist_log.shape[0]
        x = torch.randn(B, seq_len, 3, device=self.device)
        ts = torch.linspace(self.T - 1, 0, steps, device=self.device).long()
        for i in range(len(ts)):
            t = ts[i].expand(B)
            a = self.acp[t][:, None, None]
            eps = self.model(x, t, dist_log, persona)
            x0 = (x - (1 - a).sqrt() * eps) / a.sqrt().clamp(min=1e-5)
            if i < len(ts) - 1:
                a_next = self.acp[ts[i + 1]]  # scalar, broadcasts over (B,L,3)
                x = a_next.sqrt() * x0 + (1 - a_next).sqrt() * eps
            else:
                x = x0
        return x
