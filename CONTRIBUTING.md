# Publishing a cart on the Exchange

You keep your repository, your history, your identity, and your licence; the registry takes one small reviewed file.

## The author's workflow

1. **Make a cart.** Put `cart.toml` and (ideally) `file_id.diz` in its root,
   in your own public repository on a supported forge (github.com or
   gitlab.com). Ship a `LICENSE` file; the registry admits the SPDX licences
   listed in `validator/src/manifest.rs` (extendable by policy PR).

   **Optionally, one picture.** A `screenshot.png` beside `cart.toml` — a real
   PNG, at most 256 KiB and 1280×720 — becomes the cart's picture on the
   board's INFO screen. It is presentation, not source: leave it out of your
   `[package] include` patterns (the default set already excludes it), and it
   is never installed or compiled. `jcx prepare` attests its digest and
   dimensions into the entry; the board fetches it lazily from your repository
   at the pinned revision and verifies it against that digest. One picture,
   never a gallery — the board stays text-first.
2. **Pin a release.** Tag it if you like, but note the **full 40-hex commit
   hash** — the registry never installs from a branch or floating tag.
3. **Run the tool.** From this repository:

   ```
   cargo run --manifest-path validator/Cargo.toml -- prepare <your-repo-url> <revision>
   ```

   `jcx prepare` downloads the canonical archive for that revision, runs every
   check CI will run, and prints the exact registry-entry TOML plus a
   pull-request body. Fix anything it flags; hand-compute nothing.

   **The comfortable path:** the Jet64 Workstation wraps this whole step. On
   the board's welcome screen, `[P] PREPARE UPLINK` generates `cart.toml`
   from your project's own `publish.ron` (the include tree itself decides the
   `network` declaration), runs the identical `jcx` code in-process, and puts
   the entry TOML and PR body on your clipboard. The two paths share one
   implementation, so their output can never differ.
4. **Open a pull request** adding `carts/<your-cart-id>.toml`, then run
   `cargo run --manifest-path validator/Cargo.toml -- index . --write` so the
   committed index matches. The PR template asks you to confirm namespace
   ownership, licence coverage, and capability honesty.
5. **CI validates; a human plays it.** Merge means published — the next
   catalogue refresh, your cart is on the board with your own words on its
   INFO screen.

Don't speak pull-request? Email the sysop a repository link and a revision;
a maintainer runs `jcx prepare` and files the entry, credited to you, with the
provenance recorded in the entry. A board has always taken voice calls.

## Identity namespaces (E19)

Your cart ID is permanent and reverse-DNS-style, e.g. `io.github.mira.rockets`.

* **Forge-scoped prefixes** (`io.github.<account>.*`, `io.gitlab.<account>.*`)
  are verified mechanically: the entry's `repository` must live under that
  account, and the PR must come from it (or from a member of it, for an
  organisation).
* **Any other prefix** (a personal domain, an organisation) needs provenance a
  reviewer can weigh, recorded in the entry's `provenance` field: a
  well-known file at the domain, or ownership that is public record.
* A prefix, once used, maps to one owner. IDs are never recycled; ownership
  transfers are registry history, never ID changes.

## What review means

Two layers:

* **Machines check what machines can check** — digests reproduced, archive
  hygiene, schema, licence allowlist, the forbidden-symbol policy
  (`policy.toml`, generated from the Jet64 sandbox compiler itself),
  capability declarations against the include tree, the declared
  `jet64_api` against the generation of every API name the source uses
  (`api_generations.toml`, generated from the Jet64 runtime's own history),
  DIZ bounds, index
  byte-identity, and per-unit `clang -fsyntax-only` against `headers/`.
* **A maintainer checks what they cannot** — that the cart is what it says,
  installs through the Workstation's own pipeline, and plays. The Workstation
  has a room for exactly this: HELP → `[M] MAINTAINER REVIEW MODE` dials the
  PR branch's `index-v1.json` as a session-scoped, visibly-badged board, so
  the submission is installed and played through the production pipeline —
  same download, same validation, same sandbox — before it is ever merged.
  Review is release-specific: a trusted author never makes future bytes
  trusted.

Higher-risk changes — new network capability, licence change, ownership
transfer — take two reviewers.

## Immutability and the one exception

An accepted `(id, version)` never changes. The single audited exception:
forges occasionally regenerate their archives with different compression, and
the archive digest breaks through no fault of anyone's. The repair is a one-
line PR re-pinning `sha256`/`archive_bytes`, which CI accepts **only** when
the replacement's tree fingerprint is byte-identical to the reviewed release.
Content immutability holds absolutely; transport bytes are repairable, in
public, with an audit trail.

## The sysop's clock (E22)

Review happens on batched days. The queue may pause publicly — "the board is
full this month" — without apology, and nothing here assumes prompt review.
Automated gates always run in full; the board is tended, not served. Please
be kind to its keeper.
