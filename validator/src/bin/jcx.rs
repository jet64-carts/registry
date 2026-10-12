// validator/src/bin/jcx.rs
//! `jcx` — the Cartridge Exchange's command-line tool (docs/cart_exchange/cart_exchange.md E21).
//!
//! Five commands, for the two sides of the counter and the one moment that is
//! neither:
//!
//! * `jcx validate <archive.tar.gz>` — extract a local source archive under the
//!   community hygiene policy, run the full acceptance pipeline, print the
//!   deterministic report and both digests.
//! * `jcx prepare <repository-url> <revision>` — what an author runs: derive the
//!   canonical forge archive URL from the pinned revision, download it, run every
//!   check CI will run, and print the exact registry-entry TOML plus a PR body.
//!   Nobody ever hand-computes a SHA-256. `--entry-fragment` prints just the
//!   `[[releases]]` block on stdout for tooling to splice into an existing entry.
//! * `jcx stage <repository-url> <revision> <archive.tar.gz>` — `prepare` for a
//!   seed whose repository does not exist yet (§12 P1.3): the archive comes from
//!   disk instead of the forge, and the two facts only a forge can settle come
//!   out as marked placeholders. Prints the `[[releases]]` block alone, which is
//!   all a seed's hand-curated entry head is missing.
//! * `jcx index <registry-root>` — render the deterministic `index-v1.json`
//!   (§6.3): print it, `--write` it, or `--check` it against the committed copy.
//! * `jcx verify <registry-root>` — the registry CI pass (§6.5): every offline
//!   rule, plus `--base` for the immutability comparison against the accepted
//!   state and `--fetch` to download and witness new-or-changed releases.
//!
//! Network is touched exactly where the design says it may be: `prepare` fetches
//! one canonical HTTPS archive, and `verify --fetch` fetches the releases the
//! work-list names — nothing else, ever.

use std::path::{Path, PathBuf};
use std::process::ExitCode;

use jcx_validator::manifest::{CartId, ReleaseIdentity};
use jcx_validator::prepare::{
    accept_archive, pending_release_block, pr_body, registry_entry, release_block, today_utc,
    Acceptance,
};
use jcx_validator::registry::{
    canonical_archive_url, render_index, validate_revision, verify_registry, VerifyOptions,
    INDEX_FILE,
};
use jcx_validator::remote::{download, witness_release};

const USAGE: &str = "\
jcx — Jet64 Cartridge Exchange validator

USAGE:
    jcx validate <archive.tar.gz> [--id <cart-id> --version <semver>]
    jcx prepare  <repository-url> <full-40-hex-revision> [--entry-fragment]
    jcx stage    <repository-url> <full-40-hex-revision> <archive.tar.gz>
    jcx index    <registry-root>  [--write | --check]
    jcx verify   <registry-root>  [--base <accepted-checkout>]
                                  [--fetch [--keep-trees <dir>]]
                                  [--pr-author <login>] [--author-orgs <a,b,c>]

`validate` runs the registry acceptance pipeline over a local source archive.
`prepare` fetches the canonical forge archive for a pinned revision, validates
it, and prints the registry-entry TOML and pull-request text (nothing is ever
hand-computed).
`stage` is `prepare` for a seed whose repository is not pushed yet: the archive
is read from disk and the transport digests come out as marked placeholders.
`index` renders the deterministic index-v1.json from the entries and bulletin.
`verify` runs the registry CI pass: offline rules always; immutability against
--base; digest/content witnessing of new-or-changed releases with --fetch.";

fn main() -> ExitCode {
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    let outcome = match arguments.first().map(String::as_str) {
        Some("validate") => run_validate(&arguments[1..]),
        Some("prepare") => run_prepare(&arguments[1..]),
        Some("stage") => run_stage(&arguments[1..]),
        Some("index") => run_index(&arguments[1..]),
        Some("verify") => run_verify(&arguments[1..]),
        _ => Err(USAGE.to_owned()),
    };
    match outcome {
        Ok(clean) if clean => ExitCode::SUCCESS,
        Ok(_) => ExitCode::FAILURE,
        Err(message) => {
            eprintln!("{message}");
            ExitCode::FAILURE
        }
    }
}

/// A scratch directory that cleans up after itself, success or failure.
struct ScratchDir(PathBuf);

impl ScratchDir {
    fn create(label: &str) -> Result<Self, String> {
        let path = std::env::temp_dir().join(format!("jcx-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&path);
        std::fs::create_dir_all(&path)
            .map_err(|error| format!("cannot create scratch directory: {error}"))?;
        Ok(Self(path))
    }

    fn path(&self) -> &Path {
        &self.0
    }
}

impl Drop for ScratchDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

// --- validate ---------------------------------------------------------------

fn run_validate(arguments: &[String]) -> Result<bool, String> {
    let (archive, expected) = parse_validate_arguments(arguments)?;
    let scratch = ScratchDir::create("validate")?;
    let acceptance = accept_archive(&archive, scratch.path(), expected.as_ref())?;
    print_acceptance(&acceptance);
    Ok(acceptance.validation.report.is_clean())
}

fn parse_validate_arguments(
    arguments: &[String],
) -> Result<(PathBuf, Option<ReleaseIdentity>), String> {
    let mut archive: Option<PathBuf> = None;
    let mut id: Option<CartId> = None;
    let mut version: Option<semver::Version> = None;
    let mut cursor = arguments.iter();
    while let Some(argument) = cursor.next() {
        match argument.as_str() {
            "--id" => {
                let raw = cursor.next().ok_or("--id needs a value")?;
                id = Some(CartId::parse(raw)?);
            }
            "--version" => {
                let raw = cursor.next().ok_or("--version needs a value")?;
                version = Some(raw.parse().map_err(|error| format!("--version: {error}"))?);
            }
            positional if archive.is_none() => archive = Some(PathBuf::from(positional)),
            unexpected => return Err(format!("unexpected argument {unexpected:?}\n\n{USAGE}")),
        }
    }
    let archive = archive.ok_or_else(|| USAGE.to_owned())?;
    let expected = match (id, version) {
        (Some(id), Some(version)) => Some(ReleaseIdentity { id, version }),
        (None, None) => None,
        _ => return Err("--id and --version must be given together".to_owned()),
    };
    Ok((archive, expected))
}

// --- prepare ----------------------------------------------------------------

fn run_prepare(arguments: &[String]) -> Result<bool, String> {
    let mut positionals: Vec<&String> = Vec::new();
    let mut fragment_only = false;
    for argument in arguments {
        match argument.as_str() {
            "--entry-fragment" => fragment_only = true,
            _ => positionals.push(argument),
        }
    }
    let [repository, revision] = positionals[..] else {
        return Err(USAGE.to_owned());
    };
    validate_revision(revision)?;
    let source_url = canonical_archive_url(repository, revision)?;

    let scratch = ScratchDir::create("prepare")?;
    let archive = scratch.path().join("release.tar.gz");
    eprintln!("fetching {source_url}");
    download(&source_url, &archive)?;

    let acceptance = accept_archive(&archive, scratch.path(), None)?;
    if fragment_only {
        // Machine mode: the report goes to stderr, the block alone to stdout.
        eprint!("{}", acceptance.validation.report);
        if !acceptance.validation.report.is_clean() {
            return Ok(false);
        }
        print!(
            "{}",
            release_block(&acceptance, revision, &source_url, &today_utc())
        );
        return Ok(true);
    }
    print_acceptance(&acceptance);
    if !acceptance.validation.report.is_clean() {
        eprintln!("fix the findings above, tag a new revision, and run prepare again.");
        return Ok(false);
    }
    print_registry_entry(&acceptance, repository, revision, &source_url);
    Ok(true)
}

// --- stage ------------------------------------------------------------------

/// `prepare`'s pre-push twin (§12 P1.3). The `source` it pins is the canonical
/// URL the forge *will* answer once the revision is pushed — derived by the same
/// rule, so nothing about the entry changes on push day except the transport
/// pair only a served gzip stream can supply.
///
/// Machine mode throughout, like `prepare --entry-fragment`: the report goes to
/// stderr and the block alone to stdout, so a script can splice it.
fn run_stage(arguments: &[String]) -> Result<bool, String> {
    let [repository, revision, archive] = arguments else {
        return Err(USAGE.to_owned());
    };
    validate_revision(revision)?;
    let source_url = canonical_archive_url(repository, revision)?;

    let scratch = ScratchDir::create("stage")?;
    let acceptance = accept_archive(Path::new(archive), scratch.path(), None)?;
    eprint!("{}", acceptance.validation.report);
    if !acceptance.validation.report.is_clean() {
        return Ok(false);
    }
    print!(
        "{}",
        pending_release_block(&acceptance, revision, &source_url, &today_utc())
    );
    Ok(true)
}

// --- index ------------------------------------------------------------------

fn run_index(arguments: &[String]) -> Result<bool, String> {
    let mut root: Option<PathBuf> = None;
    let mut write = false;
    let mut check = false;
    for argument in arguments {
        match argument.as_str() {
            "--write" => write = true,
            "--check" => check = true,
            positional if root.is_none() => root = Some(PathBuf::from(positional)),
            unexpected => return Err(format!("unexpected argument {unexpected:?}\n\n{USAGE}")),
        }
    }
    let root = root.ok_or_else(|| USAGE.to_owned())?;
    if write && check {
        return Err("--write and --check are exclusive".to_owned());
    }

    let rendered = match render_index(&root).map_err(|error| error.to_string())? {
        Ok(rendered) => rendered,
        Err(report) => {
            print!("{report}");
            return Ok(false);
        }
    };
    let index_path = root.join(INDEX_FILE);
    if write {
        std::fs::write(&index_path, &rendered)
            .map_err(|error| format!("cannot write {}: {error}", index_path.display()))?;
        eprintln!("wrote {} ({} bytes)", index_path.display(), rendered.len());
    } else if check {
        let committed = std::fs::read_to_string(&index_path).unwrap_or_default();
        if committed != rendered {
            eprintln!(
                "{INDEX_FILE} differs from the regenerated index — run `jcx index . --write`"
            );
            return Ok(false);
        }
        eprintln!("{INDEX_FILE} is up to date");
    } else {
        print!("{rendered}");
    }
    Ok(true)
}

// --- verify -----------------------------------------------------------------

fn run_verify(arguments: &[String]) -> Result<bool, String> {
    let mut root: Option<PathBuf> = None;
    let mut base: Option<PathBuf> = None;
    let mut fetch = false;
    let mut keep_trees: Option<PathBuf> = None;
    let mut options = VerifyOptions::default();
    let mut cursor = arguments.iter();
    while let Some(argument) = cursor.next() {
        match argument.as_str() {
            "--base" => base = Some(PathBuf::from(cursor.next().ok_or("--base needs a value")?)),
            "--fetch" => fetch = true,
            "--keep-trees" => {
                keep_trees = Some(PathBuf::from(
                    cursor.next().ok_or("--keep-trees needs a value")?,
                ));
            }
            "--pr-author" => {
                options.pr_author = Some(cursor.next().ok_or("--pr-author needs a value")?.clone());
            }
            "--author-orgs" => {
                let raw = cursor.next().ok_or("--author-orgs needs a value")?;
                options.author_orgs = raw
                    .split(',')
                    .map(str::trim)
                    .filter(|org| !org.is_empty())
                    .map(str::to_owned)
                    .collect();
            }
            positional if root.is_none() => root = Some(PathBuf::from(positional)),
            unexpected => return Err(format!("unexpected argument {unexpected:?}\n\n{USAGE}")),
        }
    }
    let root = root.ok_or_else(|| USAGE.to_owned())?;
    if keep_trees.is_some() && !fetch {
        return Err("--keep-trees only means something with --fetch".to_owned());
    }

    let mut verification =
        verify_registry(&root, base.as_deref(), &options).map_err(|error| error.to_string())?;

    if fetch {
        eprintln!(
            "witnessing {} release(s) against their records",
            verification.to_fetch.len()
        );
        let scratch = ScratchDir::create("verify")?;
        for check in &verification.to_fetch {
            eprintln!(
                "  {} {} <- {}",
                check.id, check.release.version, check.release.source
            );
            let findings = witness_release(check, scratch.path(), keep_trees.as_deref())?;
            verification.report.absorb(findings);
        }
        verification.report.finalize();
    } else if base.is_some() && !verification.to_fetch.is_empty() {
        eprintln!(
            "note: {} new-or-changed release(s) still need `--fetch` witnessing before merge",
            verification.to_fetch.len()
        );
    }

    print!("{}", verification.report);
    Ok(verification.report.is_clean())
}

// --- shared printing ---------------------------------------------------------

fn print_acceptance(acceptance: &Acceptance) {
    println!("archive   sha256 {}", acceptance.sha256);
    println!(
        "archive   {} bytes down, {} bytes expanded",
        acceptance.archive_bytes, acceptance.expanded_bytes
    );
    match &acceptance.validation.fingerprint {
        Some(fingerprint) => println!("tree      sha256 {fingerprint}"),
        None => println!("tree      (not fingerprinted — the report has findings)"),
    }
    if let Some(screenshot) = &acceptance.validation.screenshot {
        println!(
            "screenshot {}×{}, {} bytes, sha256 {}",
            screenshot.width, screenshot.height, screenshot.bytes, screenshot.sha256
        );
    }
    if !acceptance.validation.pruned.is_empty() {
        println!(
            "release   {} file(s) outside [package] include were pruned",
            acceptance.validation.pruned.len()
        );
    }
    print!("{}", acceptance.validation.report);
}

/// Prints the exact registry entry (§6.2) plus PR text — the shared rendering
/// of `jcx_validator::prepare` (E21: the Workstation's Prepare Submission emits
/// the identical text), wrapped in the CLI's copy-paste markers. The CLI knows
/// no author summary, so the entry carries the marked blank for the reviewer.
fn print_registry_entry(
    acceptance: &Acceptance,
    repository: &str,
    revision: &str,
    source_url: &str,
) {
    let manifest = acceptance
        .validation
        .manifest
        .as_ref()
        .expect("a clean report implies a parsed manifest");
    println!();
    println!(
        "--- registry entry: carts/{}.toml -----------------------------",
        manifest.id
    );
    print!(
        "{}",
        registry_entry(
            acceptance,
            repository,
            revision,
            source_url,
            &today_utc(),
            None
        )
    );
    println!("--- end registry entry ---------------------------------------");
    println!();
    println!("--- pull-request body ----------------------------------------");
    print!("{}", pr_body(acceptance));
    println!("--- end pull-request body ------------------------------------");
}
