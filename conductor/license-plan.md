# License Migration Plan: MIT to AGPLv3 + Attribution

## Background & Motivation
Synapse currently uses the MIT License, which is highly permissive and allows proprietary forks. The project owner requires "complete authority" and visible recognition for their work. The goal is to enforce a strong copyleft model that prevents closed-source SaaS wrappers while mandating UI attribution.

## Scope & Impact
- **Impacted Files:** `LICENSE`, `README.md`, and all `*.cpp`/`*.h` headers in `src/` and `include/`.
- **License Model:** GNU Affero General Public License v3.0 (AGPLv3) combined with an Additional Terms clause under Section 7.

## Implementation Steps

### 1. Replace the Core License
- Remove the existing MIT `LICENSE` file.
- Download and write the standard GNU AGPLv3 text into `LICENSE`.

### 2. Append Section 7 Attribution Clause
- Append a custom "Additional Terms" section at the bottom of the `LICENSE` file.
- The clause will state: "Under Section 7 of the AGPLv3, you must preserve the following attribution: Any interactive user interfaces of the Covered Work or its derivatives must visibly display the text 'Powered by Synapse'."

### 3. Update README Documentation
- Add a new `## License` section to `README.md`.
- Clearly explain the AGPLv3 terms and explicitly mention the UI attribution requirement so downstream users are immediately aware.

### 4. Update Source Headers
- Search for any existing MIT copyright headers in `src/` and `include/` files.
- Prepend the standard AGPLv3 boilerplate header to all source and header files, including a pointer to the full license.

## Verification
- Verify `LICENSE` contains both the AGPLv3 text and the custom Section 7 clause.
- Verify `README.md` correctly communicates the new terms.
- Verify a sampling of `*.cpp` and `*.h` files to ensure the new AGPL boilerplate is present.

## Migration & Rollback
- Since the owner is the sole copyright holder (assumed), re-licensing future versions is fully within their rights. Old commits will technically remain under MIT, but all new commits and releases will be AGPLv3. Rollback is a simple `git revert`.
