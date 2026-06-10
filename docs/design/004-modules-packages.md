# Design 004 — Modules & Packages

Status: **draft for approval** · Scope: `use` statement, project layout, manifest, package manager CLI

Synapse is modular by design: automation knowledge (an app's workflow, a
site's recipe, shared helpers) should be packageable and shareable — including
*by AI agents that install a capability pack on demand*.

## 1. Module system (language level — Phase 5)

### `use` forms

```syn
use mouse, keyboard            # stdlib namespaces (explicit import optional for stdlib, required for everything else)
use "lib/helpers.syn"          # local file, binds namespace `helpers`
use "lib/helpers.syn" as h     # explicit alias
use spotify_flows              # dependency declared in syn.toml
use spotify_flows.playback     # submodule
```

- A module is a file. A package is a directory of modules with a manifest.
- Importing binds a **namespace object** (`helpers.do_thing()`); there is no
  `from x import *` — wildcard imports are the #1 source of LLM hallucinated
  names and human grep-misery. Spec-pinned rejected aliases: `import`,
  `include`, `require` (diagnostic: "did you mean `use`?").
- Each module executes once, top to bottom, at first `use`; result cached per
  engine. Cycles are a compile-time error showing the cycle path.
- `pub fn` / `pub let` mark exports; everything else is module-private.
  (Explicit exports keep packages honest about their API surface.)

### Resolution order (deterministic, spec'd)

1. stdlib name (`mouse`, `math`, …)
2. project-relative path (quoted form only)
3. `syn_modules/<name>/` (installed dependencies)

No global/site-packages directory, no environment-variable search paths —
resolution depends only on project contents, so a project that works here
works anywhere.

## 2. Project & package layout

```
my-automation/
├── syn.toml             # manifest
├── syn.lock             # exact resolved versions (committed)
├── main.syn             # default entrypoint (`syn run` runs this)
├── src/                 # additional project modules
└── syn_modules/         # installed deps (gitignored; recreated by `syn install`)
```

### `syn.toml`

```toml
[package]
name = "my-automation"
version = "0.1.0"
description = "Logs into the dashboard and exports reports"
entry = "main.syn"

[dependencies]
spotify_flows = "1.2"                                  # registry (post-1.0) — semver
helpers = { path = "../shared-helpers" }               # path dep
browser_kit = { git = "https://github.com/x/browser-kit", tag = "v0.3.0" }

[permissions]                                          # capability gating (see 002 §7)
input = true
screen = false
```

`[permissions]` is the interesting part for the AI-agent story: a host can
refuse to run a package whose manifest demands more capability than granted,
and dependencies cannot exceed the root package's grants.

## 3. Package manager CLI (Phase 7)

Built into the single `syn` binary (no separate tool to install):

| Command | Behavior |
|---------|----------|
| `syn new <name>` | scaffold manifest + main.syn + tests dir |
| `syn add <dep>[@version]` | edit manifest, resolve, update lockfile, fetch |
| `syn install` | reproduce `syn_modules/` exactly from `syn.lock` |
| `syn update [dep]` | re-resolve within semver constraints, rewrite lock |
| `syn run [entry]` | run with project resolution active |
| `syn test` | run `tests/**/*.syn` conformance-style files in the package |

- **Resolution:** semver with a single version per package name in the graph
  (no nested duplicates — automation scripts don't need npm's hell; conflict
  = clear error telling the user which two constraints fight).
- **Lockfile:** content hashes (sha256) for every dep, including git deps;
  `syn install --locked` (CI mode) fails rather than re-resolves.
- **v2.0 scope:** path + git dependencies. **Registry:** protocol speced
  (static JSON index + tarballs over HTTPS, signature field reserved), hosted
  service is post-1.0.

## 4. Security posture (because packages run with OS-input powers)

Worth stating early, even though enforcement matures over time:

1. Permission manifest enforced at engine level (deps can't escalate).
2. Lockfile hashes verified on every install and on `--locked` runs.
3. No install-time scripts. Ever. Installing a package executes nothing.
4. `syn why <dep>` explains who pulled what into the graph.

## 5. Testing this subsystem (per design 003 rules)

- Resolver: unit tests over synthetic dependency graphs (conflicts, cycles,
  diamond deps) — pure logic, no network.
- Loader: conformance tests using fixture packages in `tests/fixtures/pkgs/`.
- CLI: golden tests on manifest/lockfile mutations (`syn add` output is
  byte-stable).
- Network fetch behind an interface; tests use a local file:// "registry".
