# The Jet64 Cartridge Exchange — registry of record

This repository is the catalogue behind **DAWN CHORUS BBS**, the official board
of the [Jet64](https://73v3.itch.io) Cartridge Exchange: a curated, auditable
phonebook of open-source cartridge releases. It is a *registry*, not a
warehouse — every cart lives in its author's own repository; this repo holds
one small reviewed TOML entry per cart and a generated index the Workstation
fetches.

## What being listed here means

> This exact source snapshot passed the registry's published automated checks
> and review policy.

Nothing more, nothing less. Carts remain their authors' property under their
own licences. Carts on the Exchange are free, forever — the board handles no
money, no accounts, no ratings, and collects no telemetry.

## How it works

* An author publishes an ordinary public source repository with a `cart.toml`
  and (ideally) a `file_id.diz` at its root, and pins a release by **full
  commit hash** — never a branch or floating tag.
* One TOML entry under [`carts/`](carts/) records the release: its canonical
  archive URL, the archive digest (transport integrity), and the tree
  fingerprint (content identity). Accepted releases are immutable; mistakes
  are *yanked*, never rewritten.
* CI — the open [`validator/`](validator/) crate, the same code the Jet64
  Workstation runs before installing anything — re-derives every digest,
  enforces archive hygiene and the sandbox's forbidden-symbol policy
  ([`policy.toml`](policy.toml)), holds each cart's declared `jet64_api` to
  the generation of every API name its source uses
  ([`api_generations.toml`](api_generations.toml)), verifies namespace ownership, and
  syntax-checks every C unit against the published runtime headers in
  [`headers/`](headers/).
* The committed [`index-v1.json`](index-v1.json) is regenerated
  deterministically from the entries; clients fetch that one file. Any HTTPS
  host can serve this repository's files — GitHub is transport, not format.

## Publishing a cart

See [`CONTRIBUTING.md`](CONTRIBUTING.md). The short version: run
`jcx prepare <your-repo> <revision>` — or let the Workstation's own
**Prepare Uplink** screen run the identical code and hand you the entry on
your clipboard — open a pull request adding one file, and a human plays your
cart before it lands. The board is tended on a hobbyist's
clock — there is no SLA, and that is a feature.

## The rest of the shelf

* [`boards.toml`](boards.toml) — the phonebook of known community boards. Its
  listings ride the generated index, and the Workstation's DIAL screen renders
  them — a board is added here by pull request, like everything else. Community
  boards are unreviewed by this board's sysop and are badged as such on every
  screen that shows their carts.
* [`bulletin.txt`](bulletin.txt) — the sysop's note, shown on the welcome screen.
* [`schema/`](schema/) — JSON Schemas documenting `cart.toml` and the entries.
* [`SECURITY.md`](SECURITY.md) · [`LEGAL.md`](LEGAL.md) — reporting routes and
  takedown/yank policy.

Registry metadata, tooling, and the validator are MIT ([`LICENSE`](LICENSE));
each cart keeps its own licence, named in its entry.

*The dawn is given freely, and the birds still sing back.*
