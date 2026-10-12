// validator/src/lib.rs
//! # The Cartridge Exchange validator
//!
//! One crate holding everything the Jet64 Cartridge Exchange's registry CI and the
//! Workstation installer must **agree** on (docs/cart_exchange/cart_exchange.md E6):
//!
//! * [`manifest`] — the `cart.toml` schema (v1), the [`CartId`] identity rules, the
//!   SPDX licence allowlist, and the `file_id.diz` bounds;
//! * [`archive`] — streaming `.tar.gz` extraction under the hard hygiene caps of
//!   §7.3: size, count, depth and ratio limits, hostile-path rejection, and the rule
//!   that no permission bit ever survives extraction;
//! * [`policy`] — the file-extension allowlist and the forbidden-symbol scan, driven
//!   by `policy.toml`, whose one authoritative source is the closed workstation's
//!   compiler (E7 — an equality test there fails the build on any drift);
//! * [`api_generations`] — which API generation brought each `jet_` name, generated
//!   from the runtime's own history in the closed repo, so a cart's declared
//!   `jet64_api` is a minimum the validator can hold its source to (E9);
//! * [`fingerprint`] — the canonical tree fingerprint (E20): the content identity of
//!   a release, computed identically by CI (`tree_sha256`) and by the installer
//!   (baseline fingerprints), so "unmodified" and "content-authentic" are the same
//!   number;
//! * [`validate`] — the orchestration that turns an extracted tree into a
//!   deterministic [`report::ValidationReport`];
//! * [`prepare`] — the author's half of the counter (E21): archive acceptance plus
//!   the rendered registry-entry TOML, PR body, and generated `cart.toml`, shared
//!   verbatim by the `jcx` CLI and the Workstation's Prepare Submission panel.
//!
//! The crate performs **no network I/O**: it judges bytes it is handed. The `jcx`
//! binary (feature `cli`) layers `validate` and `prepare` commands on top for CI and
//! for cart authors (E21).
//!
//! The defining principle of the whole Exchange applies here with full force: the
//! registry distributes reviewable source, Jet64 owns validation and compilation,
//! and a downloaded repository never supplies commands for anything to execute.

pub mod api_generations;
pub mod archive;
pub mod fingerprint;
pub mod manifest;
pub mod policy;
pub mod prepare;
pub mod registry;
#[cfg(feature = "cli")]
pub mod remote;
pub mod report;
pub mod screenshot;
pub mod testkit;
pub mod validate;

pub use api_generations::{ApiGenerations, ApiUse};
pub use archive::{ArchiveError, ArchivePolicy, ExtractedTree, COMMUNITY_ARCHIVE_POLICY};
pub use fingerprint::{TreeFingerprint, TreeManifest};
pub use manifest::{CartId, CartManifest, ReleaseIdentity};
pub use policy::PolicySet;
pub use prepare::{accept_archive, Acceptance};
pub use registry::{RegistryEntry, RegistryRelease, VerifyOptions};
pub use report::{Finding, FindingCode, ValidationReport};
pub use screenshot::ScreenshotFacts;
pub use validate::{accept_tree, prune_to_release_surface, TreeValidation};
