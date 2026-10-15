// validator/src/registry.rs
//! The registry's own truth: entry schema, boards phonebook, bulletin bounds,
//! canonical source-URL derivation, the generated index, and the verification
//! pass CI runs on every pull request (docs/cart_exchange/cart_exchange.md §6).
//!
//! Everything here is judged **offline** from files in a registry checkout; the
//! network half of CI (recomputing digests against the fetched archive, §6.5.1)
//! lives in [`crate::remote`] and is fed by the [`ReleaseCheck`] work-list this
//! module emits, so the decision of *what* must be fetched is itself
//! deterministic, testable code.
//!
//! ## Where each fact lives (and why the entry carries more than §6.2 sketched)
//!
//! The design's §6.3 requires the generated index to render every screen without
//! ever fetching an archive — title, licence, and the author's DIZ card included.
//! Those three facts originate in the *cart's own* `cart.toml` / `file_id.diz`,
//! so the registry entry carries curator-committed copies (`name`, `license`,
//! `diz`) and CI proves them byte-honest against the fetched release (§6.5): the
//! curator cannot misquote the author, and the board never needs the archive to
//! draw an INFO screen. `provenance` records the E19 evidence for namespaces a
//! forge cannot prove mechanically.

use std::collections::BTreeMap;
use std::io;
use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::archive::COMMUNITY_ARCHIVE_POLICY;
use crate::manifest::{validate_diz, validate_licence_expression, CartId, NAME_MAX_BYTES};
use crate::report::{FindingCode, ValidationReport};
use crate::screenshot::{
    ScreenshotFacts, SCREENSHOT_FILE, SCREENSHOT_MAX_BYTES, SCREENSHOT_MAX_HEIGHT,
    SCREENSHOT_MAX_WIDTH,
};

/// The one registry-entry schema this crate accepts. Grows only by a new number.
pub const REGISTRY_SCHEMA: u32 = 1;

/// Canonical file and directory names inside a registry checkout (§6.1).
pub const CARTS_DIR: &str = "carts";
pub const INDEX_FILE: &str = "index-v1.json";
pub const BULLETIN_FILE: &str = "bulletin.txt";
pub const BOARDS_FILE: &str = "boards.toml";

/// The sysop's bulletin is one human voice, not a billboard (§3.2).
pub const BULLETIN_MAX_BYTES: usize = 1024;

/// Hard ceiling on the generated index (§6.3) — the board fetches this one file
/// to render everything, so it must stay a polite download forever.
pub const INDEX_MAX_BYTES: usize = 4 * 1024 * 1024;

/// Listing-row bounds: a summary is one plain sentence, an author is a name.
pub const SUMMARY_MAX_BYTES: usize = 200;
pub const AUTHOR_MAX_BYTES: usize = 64;

/// Category bounds. Categories are curator vocabulary — short lowercase slugs,
/// few per cart, so BROWSE screens stay legible.
pub const CATEGORIES_MAX: usize = 4;
pub const CATEGORY_MAX_BYTES: usize = 24;

/// Board-phonebook bounds (§6.6): a board is a name, an index URL, a trust note.
pub const BOARD_NAME_MAX_BYTES: usize = 64;
pub const BOARD_NOTE_MAX_BYTES: usize = 200;

/// Full-commit-hash length (E2): never a branch, never a short hash.
pub const REVISION_HEX_DIGITS: usize = 40;

/// Lowercase-hex length of a SHA-256 digest as stored in entries.
pub const SHA256_HEX_DIGITS: usize = 64;

/// The `generated` stamp when a board has no releases at all: the Unix epoch,
/// which is honest ("nothing has ever been published") and keeps the index a
/// pure function of its inputs.
const EPOCH_DATE: &str = "1970-01-01";

// --- schema -----------------------------------------------------------------

/// One release a registry entry pins (§6.2). `(id, version, tree_sha256)` is the
/// immutable fact (E20); `sha256`/`source`/`archive_bytes` are transport and may
/// be re-pinned by the audited §7.5 path; `yanked`/`unavailable` are moderation
/// state and may always change.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct RegistryRelease {
    pub version: semver::Version,
    /// Full 40-hex commit hash (E2).
    pub revision: String,
    /// The canonical forge archive URL derived from `repository` + `revision`
    /// (§6.5.2) — or, for an `unavailable` release only, an audited mirror (§6.7).
    pub source: String,
    /// SHA-256 of the archive bytes — transport integrity (E20).
    pub sha256: String,
    /// The E20 tree fingerprint of the release surface — content identity.
    pub tree_sha256: String,
    pub archive_bytes: u64,
    pub expanded_bytes: u64,
    /// Minimum `JET64_API_GENERATION` (E9), CI-verified against the manifest.
    pub jet64_api: u32,
    /// Network reach, CI-verified against the include tree (§6.5.7).
    pub network: bool,
    /// `YYYY-MM-DD`, the day the release was accepted.
    pub published: String,
    /// Removed from default discovery — something was wrong (§6.7).
    #[serde(default)]
    pub yanked: bool,
    /// Upstream gone, nothing wrong — mirror rules in §6.7.
    #[serde(default)]
    pub unavailable: bool,
    /// The attested `screenshot.png` facts, when the release ships one
    /// (§12 P6.2) — CI-verified against the archive, fetched lazily by the
    /// board from the canonical raw URL and judged against this digest.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub screenshot: Option<ScreenshotFacts>,
}

/// One cart's registry entry — the curator-owned file `carts/<id>.toml` (§6.2).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct RegistryEntry {
    pub schema: u32,
    pub id: CartId,
    /// Display title, CI-verified equal to the latest release's manifest.
    pub name: String,
    /// One plain sentence for the listing row.
    pub summary: String,
    pub author: String,
    /// SPDX expression, CI-verified equal to the latest release's manifest.
    pub license: String,
    pub categories: Vec<String>,
    pub repository: String,
    /// E19 evidence for a namespace no forge can prove mechanically, or for an
    /// entry a maintainer files on an author's behalf (§5.3's "voice call") —
    /// recorded here so the claim travels with the entry it justifies.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub provenance: Option<String>,
    /// The author's `file_id.diz`, CI-verified byte-honest against the latest
    /// release (§6.3: browsing never fetches archives).
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub diz: Option<String>,
    #[serde(rename = "releases")]
    pub releases: Vec<RegistryRelease>,
}

impl RegistryEntry {
    /// Parses and *shape*-checks an entry file; the semantic rules live in
    /// [`verify_registry`] so every rule lands in the one deterministic report.
    pub fn parse(source: &str) -> Result<Self, String> {
        let entry: RegistryEntry = toml::from_str(source).map_err(|error| error.to_string())?;
        if entry.schema != REGISTRY_SCHEMA {
            return Err(format!(
                "unsupported registry schema {} (this validator accepts schema {REGISTRY_SCHEMA})",
                entry.schema
            ));
        }
        Ok(entry)
    }

    /// The latest release by SemVer — the one whose manifest vouches for the
    /// entry-level `name`/`license`/`diz`.
    pub fn latest_release(&self) -> Option<&RegistryRelease> {
        self.releases
            .iter()
            .max_by(|a, b| a.version.cmp(&b.version))
    }
}

/// The phonebook of known community boards (§6.6). Empty at launch by design —
/// the file existing *is* the federation promise.
#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct BoardsFile {
    pub schema: u32,
    #[serde(default)]
    pub boards: Vec<BoardListing>,
}

/// One board: a name, a root index URL, and a trust note.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct BoardListing {
    pub name: String,
    pub index: String,
    pub note: String,
}

// --- forge derivation (E2, §6.5.2) ------------------------------------------

/// A parsed `https://<forge>/<owner>/<name>` repository coordinate.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ForgeRepo {
    pub host: String,
    pub owner: String,
    pub name: String,
}

/// Parses a repository URL into its forge coordinate. Trailing `/` and `.git`
/// are tolerated on input; everything else must be exactly three parts.
pub fn parse_forge_repository(repository: &str) -> Result<ForgeRepo, String> {
    let trimmed = repository.trim_end_matches('/').trim_end_matches(".git");
    let Some(rest) = trimmed.strip_prefix("https://") else {
        return Err("repository URL must be https://".to_owned());
    };
    let mut segments = rest.split('/');
    let host = segments.next().unwrap_or_default();
    let owner = segments.next().unwrap_or_default();
    let name = segments.next().unwrap_or_default();
    if host.is_empty() || owner.is_empty() || name.is_empty() || segments.next().is_some() {
        return Err(format!(
            "repository URL must be https://<forge>/<owner>/<name>, got {repository:?}"
        ));
    }
    Ok(ForgeRepo {
        host: host.to_owned(),
        owner: owner.to_owned(),
        name: name.to_owned(),
    })
}

/// The canonical source-archive URL for a pinned revision (§6.5.2): derived from
/// the repository URL, per supported forge — a release can never point at a
/// third host, and CI re-derives this same string to prove it.
pub fn canonical_archive_url(repository: &str, revision: &str) -> Result<String, String> {
    let repo = parse_forge_repository(repository)?;
    match repo.host.as_str() {
        "github.com" => Ok(format!(
            "https://github.com/{}/{}/archive/{revision}.tar.gz",
            repo.owner, repo.name
        )),
        "gitlab.com" => Ok(format!(
            "https://gitlab.com/{}/{}/-/archive/{revision}/{}-{revision}.tar.gz",
            repo.owner, repo.name, repo.name
        )),
        other => Err(format!(
            "unsupported forge {other:?} (supported: github.com, gitlab.com)"
        )),
    }
}

/// The canonical raw-file URL of a release's `screenshot.png` at its pinned
/// revision (§12 P6.2) — the same derivation discipline as
/// [`canonical_archive_url`]: the picture is fetched from the forge the release
/// already lives on, never a third host, and the entry's pinned digest keeps
/// the bytes honest wherever they are served from.
pub fn canonical_screenshot_url(repository: &str, revision: &str) -> Result<String, String> {
    let repo = parse_forge_repository(repository)?;
    match repo.host.as_str() {
        "github.com" => Ok(format!(
            "https://raw.githubusercontent.com/{}/{}/{revision}/{SCREENSHOT_FILE}",
            repo.owner, repo.name
        )),
        "gitlab.com" => Ok(format!(
            "https://gitlab.com/{}/{}/-/raw/{revision}/{SCREENSHOT_FILE}",
            repo.owner, repo.name
        )),
        other => Err(format!(
            "unsupported forge {other:?} (supported: github.com, gitlab.com)"
        )),
    }
}

/// E2: a revision is a full lowercase commit hash — never a branch, never a
/// floating tag, never a short prefix.
pub fn validate_revision(revision: &str) -> Result<(), String> {
    let full_hex = revision.len() == REVISION_HEX_DIGITS
        && revision
            .chars()
            .all(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase());
    if full_hex {
        Ok(())
    } else {
        Err(format!(
            "revision must be a full {REVISION_HEX_DIGITS}-character lowercase commit hash \
             (E2: never a branch, never a floating tag)"
        ))
    }
}

/// The forge a reverse-DNS namespace prefix claims, when it claims one at all
/// (E19): `io.github.<account>.*` → `("github.com", account)`, likewise GitLab.
pub fn forge_scoped_account(id: &CartId) -> Option<(&'static str, &str)> {
    let mut segments = id.as_str().split('.');
    match (segments.next(), segments.next(), segments.next()) {
        (Some("io"), Some("github"), Some(account)) => Some(("github.com", account)),
        (Some("io"), Some("gitlab"), Some(account)) => Some(("gitlab.com", account)),
        _ => None,
    }
}

// --- the generated index (§6.3) ----------------------------------------------

/// One cart as the index renders it — exactly what the board needs, nothing the
/// board should not carry (no `provenance`: that is review evidence, not
/// catalogue data; the entry file remains public for anyone auditing it).
#[derive(Serialize)]
struct IndexCart<'a> {
    id: &'a str,
    name: &'a str,
    summary: &'a str,
    author: &'a str,
    license: &'a str,
    categories: &'a [String],
    repository: &'a str,
    #[serde(skip_serializing_if = "Option::is_none")]
    diz: Option<&'a str>,
    releases: Vec<&'a RegistryRelease>,
}

#[derive(Serialize)]
struct IndexFile<'a> {
    schema: u32,
    /// The most recent `published` date on the board — a pure function of the
    /// entries, so regeneration is byte-identical on every machine (§6.5.10).
    generated: &'a str,
    bulletin: &'a str,
    /// The phonebook (§6.6), embedded so the DIAL screen renders from the one
    /// index every screen already renders from (§6.3) — `boards.toml` stays the
    /// human-edited source of truth, reviewed by pull request like everything
    /// else; this is its wire form.
    boards: &'a [BoardListing],
    carts: Vec<IndexCart<'a>>,
}

/// Renders `index-v1.json`: deterministic ordering (carts by id, releases newest
/// first), stable pretty formatting, trailing newline, hard-capped size.
pub fn generate_index(
    entries: &[RegistryEntry],
    bulletin: &str,
    boards: &[BoardListing],
) -> Result<String, String> {
    let mut sorted: Vec<&RegistryEntry> = entries.iter().collect();
    sorted.sort_by(|a, b| a.id.cmp(&b.id));

    let generated = sorted
        .iter()
        .flat_map(|entry| entry.releases.iter())
        .map(|release| release.published.as_str())
        .max()
        .unwrap_or(EPOCH_DATE);

    let carts = sorted
        .iter()
        .map(|entry| {
            let mut releases: Vec<&RegistryRelease> = entry.releases.iter().collect();
            releases.sort_by(|a, b| b.version.cmp(&a.version));
            IndexCart {
                id: entry.id.as_str(),
                name: &entry.name,
                summary: &entry.summary,
                author: &entry.author,
                license: &entry.license,
                categories: &entry.categories,
                repository: &entry.repository,
                diz: entry.diz.as_deref(),
                releases,
            }
        })
        .collect();

    let index = IndexFile {
        schema: REGISTRY_SCHEMA,
        generated,
        bulletin,
        boards,
        carts,
    };
    let mut rendered = serde_json::to_string_pretty(&index)
        .map_err(|error| format!("index serialisation failed: {error}"))?;
    rendered.push('\n');
    if rendered.len() > INDEX_MAX_BYTES {
        return Err(format!(
            "generated index is {} bytes (maximum {INDEX_MAX_BYTES})",
            rendered.len()
        ));
    }
    Ok(rendered)
}

/// Loads the registry at `root` and renders its index — the `jcx index` path.
/// Load problems come back as the deterministic report (`Err`); a clean load
/// yields the rendered bytes ready to write or compare.
pub fn render_index(root: &Path) -> io::Result<Result<String, ValidationReport>> {
    let mut report = ValidationReport::new();
    let entries = load_entries(root, &mut report)?;
    let bulletin = verify_bulletin(root, &mut report)?;
    let boards = verify_boards(root, &mut report)?;
    let rendered = match generate_index(&entries, &bulletin, &boards.boards) {
        Ok(rendered) => rendered,
        Err(message) => {
            report.push(
                FindingCode::RegistryInvalid,
                Some(INDEX_FILE.to_owned()),
                message,
            );
            String::new()
        }
    };
    report.finalize();
    Ok(if report.is_clean() {
        Ok(rendered)
    } else {
        Err(report)
    })
}

// --- verification ------------------------------------------------------------

/// What the caller knows about the change under review.
#[derive(Clone, Debug, Default)]
pub struct VerifyOptions {
    /// The submitting user's forge login, when running in CI on a pull request.
    pub pr_author: Option<String>,
    /// Organisations the submitting user publicly belongs to (resolved by the
    /// workflow via the forge API) — how an org-owned namespace accepts a PR
    /// from one of its members.
    pub author_orgs: Vec<String>,
}

/// One release the network half of CI must fetch and prove (§6.5.1/.5–.9).
#[derive(Clone, Debug)]
pub struct ReleaseCheck {
    pub id: CartId,
    /// Entry-level facts the latest release's manifest must vouch for.
    pub name: String,
    pub license: String,
    pub diz: Option<String>,
    /// Whether this release is the entry's latest — only then do the entry-level
    /// facts above apply (an old release keeps its own manifest history).
    pub is_latest: bool,
    pub release: RegistryRelease,
}

/// Everything one offline verification produced.
#[derive(Debug)]
pub struct RegistryVerification {
    pub report: ValidationReport,
    /// Entries that parsed, sorted by id (parse failures are findings instead).
    pub entries: Vec<RegistryEntry>,
    /// The fetch work-list: every release when no base is given, otherwise the
    /// new-or-changed ones (a transport re-pin included — that is the §7.5 path,
    /// and it is only acceptable *because* the fetch will prove the tree).
    pub to_fetch: Vec<ReleaseCheck>,
}

/// Runs every offline check over the registry checkout at `root` — schemas,
/// identity namespaces (E19), source derivation (§6.5.2), bulletin and boards
/// bounds, index byte-identity (§6.5.10), and, when `base_root` names a checkout
/// of the accepted state, the E20/§6.5.11 immutability rules.
pub fn verify_registry(
    root: &Path,
    base_root: Option<&Path>,
    options: &VerifyOptions,
) -> io::Result<RegistryVerification> {
    let mut report = ValidationReport::new();

    let entries = load_entries(root, &mut report)?;
    let base_entries = match base_root {
        Some(base) => Some(load_entries(base, &mut ValidationReport::new())?),
        None => None,
    };
    for entry in &entries {
        // E19's submitter half weighs what this change submits: an entry the base already holds,
        // byte for byte, was weighed when it was merged, and a pull request that only touches
        // the board's own files must never be refused for who merged a cart before it.
        let submitted = base_entries
            .as_ref()
            .is_none_or(|base| !base.contains(entry));
        verify_entry(entry, options, submitted, &mut report);
    }

    let bulletin = verify_bulletin(root, &mut report)?;
    let boards = verify_boards(root, &mut report)?;
    verify_index(root, &entries, &bulletin, &boards.boards, &mut report)?;

    let to_fetch = match &base_entries {
        Some(base) => {
            verify_immutability(base, &entries, &mut report);
            changed_release_checks(base, &entries)
        }
        None => all_release_checks(&entries),
    };

    report.finalize();
    Ok(RegistryVerification {
        report,
        entries,
        to_fetch,
    })
}

/// Parses every `carts/<id>.toml`, in sorted order. A file whose name is not an
/// id, whose contents do not parse, or whose id disagrees with its name becomes
/// a finding; the well-formed rest still verify, so one bad file cannot hide
/// another's problems.
fn load_entries(root: &Path, report: &mut ValidationReport) -> io::Result<Vec<RegistryEntry>> {
    let carts_dir = root.join(CARTS_DIR);
    let mut entries = Vec::new();
    if !carts_dir.is_dir() {
        return Ok(entries);
    }
    let mut names: Vec<String> = Vec::new();
    for entry in std::fs::read_dir(&carts_dir)? {
        names.push(entry?.file_name().to_string_lossy().into_owned());
    }
    names.sort();
    for file_name in names {
        let location = format!("{CARTS_DIR}/{file_name}");
        let Some(stem) = file_name.strip_suffix(".toml") else {
            report.push(
                FindingCode::RegistryInvalid,
                Some(location),
                "only <cart-id>.toml files belong in carts/",
            );
            continue;
        };
        let source = std::fs::read_to_string(carts_dir.join(&file_name))?;
        let parsed = CartId::parse(stem).and_then(|id| {
            RegistryEntry::parse(&source).and_then(|entry| {
                if entry.id == id {
                    Ok(entry)
                } else {
                    Err(format!(
                        "file name says {id} but the entry says {} — one cart, one name",
                        entry.id
                    ))
                }
            })
        });
        match parsed {
            Ok(entry) => entries.push(entry),
            Err(message) => report.push(FindingCode::RegistryInvalid, Some(location), message),
        }
    }
    entries.sort_by(|a, b| a.id.cmp(&b.id));
    Ok(entries)
}

/// The semantic rules for one entry. Every finding names the entry's file so a
/// PR review can jump straight to it.
/// `submitted` is whether this change adds or alters the entry (always, without a base).
fn verify_entry(
    entry: &RegistryEntry,
    options: &VerifyOptions,
    submitted: bool,
    report: &mut ValidationReport,
) {
    let location = || Some(format!("{CARTS_DIR}/{}.toml", entry.id));
    let mut fail = |message: String| {
        report.push(FindingCode::RegistryInvalid, location(), message);
    };

    if entry.name.trim().is_empty() || entry.name.len() > NAME_MAX_BYTES {
        fail(format!(
            "name must be non-empty and at most {NAME_MAX_BYTES} bytes"
        ));
    }
    if entry.summary.trim().is_empty()
        || entry.summary.len() > SUMMARY_MAX_BYTES
        || entry.summary.contains('\n')
    {
        fail(format!(
            "summary must be one non-empty line of at most {SUMMARY_MAX_BYTES} bytes"
        ));
    }
    if entry.author.trim().is_empty() || entry.author.len() > AUTHOR_MAX_BYTES {
        fail(format!(
            "author must be non-empty and at most {AUTHOR_MAX_BYTES} bytes"
        ));
    }
    if let Err(message) = validate_licence_expression(&entry.license) {
        fail(message);
    }
    verify_categories(&entry.categories, &mut fail);
    if let Some(diz) = &entry.diz {
        if let Err(message) = validate_diz(diz) {
            fail(format!("diz: {message}"));
        }
    }

    let repo = match parse_forge_repository(&entry.repository) {
        Ok(repo) => Some(repo),
        Err(message) => {
            fail(message);
            None
        }
    };

    verify_namespace(entry, repo.as_ref(), options, submitted, report);
    verify_releases(entry, report);
}

fn verify_categories(categories: &[String], fail: &mut impl FnMut(String)) {
    if categories.is_empty() || categories.len() > CATEGORIES_MAX {
        fail(format!("an entry carries 1–{CATEGORIES_MAX} categories"));
    }
    for category in categories {
        let well_formed = !category.is_empty()
            && category.len() <= CATEGORY_MAX_BYTES
            && category
                .chars()
                .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '-')
            && category
                .chars()
                .next()
                .is_some_and(|c| c.is_ascii_alphanumeric())
            && category
                .chars()
                .last()
                .is_some_and(|c| c.is_ascii_alphanumeric());
        if !well_formed {
            fail(format!(
                "category {category:?} must be a lowercase slug of at most \
                 {CATEGORY_MAX_BYTES} bytes"
            ));
        }
        if categories.iter().filter(|other| *other == category).count() > 1 {
            fail(format!("category {category:?} is listed twice"));
        }
    }
}

/// E19: the ID's prefix is a claim, and the registry checks it. Forge-scoped
/// prefixes are proven mechanically against the repository owner (always) and
/// the PR author (when CI knows one); any other prefix needs recorded
/// provenance a human reviewer weighs. The PR author is weighed only against an entry the change
/// `submitted`.
fn verify_namespace(
    entry: &RegistryEntry,
    repo: Option<&ForgeRepo>,
    options: &VerifyOptions,
    submitted: bool,
    report: &mut ValidationReport,
) {
    let location = Some(format!("{CARTS_DIR}/{}.toml", entry.id));
    match forge_scoped_account(&entry.id) {
        Some((forge_host, account)) => {
            if let Some(repo) = repo {
                let owner_matches =
                    repo.host == forge_host && repo.owner.to_ascii_lowercase() == account;
                if !owner_matches {
                    report.push(
                        FindingCode::NamespaceUnproven,
                        location.clone(),
                        format!(
                            "id claims the {forge_host}/{account} namespace but the \
                             repository is {}/{} (E19)",
                            repo.host, repo.owner
                        ),
                    );
                }
            }
            if let Some(author) = options.pr_author.as_ref().filter(|_| submitted) {
                let author = author.to_ascii_lowercase();
                let submitter_proven = author == account
                    || options
                        .author_orgs
                        .iter()
                        .any(|org| org.to_ascii_lowercase() == account)
                    || entry.provenance.is_some();
                if !submitter_proven {
                    report.push(
                        FindingCode::NamespaceUnproven,
                        location,
                        format!(
                            "submitted by {author:?}, who is not {account:?} or a member \
                             of it — record provenance in the entry, or submit from the \
                             owning account (E19)"
                        ),
                    );
                }
            }
        }
        None => {
            if entry.provenance.is_none() {
                report.push(
                    FindingCode::NamespaceUnproven,
                    location,
                    "a non-forge namespace needs a provenance record for a reviewer to \
                     weigh — a well-known file at the domain, or ownership that is public \
                     record (E19)",
                );
            }
        }
    }
}

fn verify_releases(entry: &RegistryEntry, report: &mut ValidationReport) {
    let location = || Some(format!("{CARTS_DIR}/{}.toml", entry.id));
    if entry.releases.is_empty() {
        report.push(
            FindingCode::RegistryInvalid,
            location(),
            "an entry lists at least one release",
        );
        return;
    }
    let mut seen_versions: Vec<&semver::Version> = Vec::new();
    for release in &entry.releases {
        let mut fail = |message: String| {
            report.push(
                FindingCode::RegistryInvalid,
                location(),
                format!("release {}: {message}", release.version),
            );
        };
        if seen_versions.contains(&&release.version) {
            fail("version listed twice".to_owned());
        }
        seen_versions.push(&release.version);

        if let Err(message) = validate_revision(&release.revision) {
            fail(message);
        }
        verify_source_url(entry, release, &mut fail);
        for (digest, label) in [
            (&release.sha256, "sha256"),
            (&release.tree_sha256, "tree_sha256"),
        ] {
            let well_formed = digest.len() == SHA256_HEX_DIGITS
                && digest
                    .chars()
                    .all(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase());
            if !well_formed {
                fail(format!(
                    "{label} must be {SHA256_HEX_DIGITS} lowercase hex digits"
                ));
            }
        }
        if release.archive_bytes == 0
            || release.archive_bytes > COMMUNITY_ARCHIVE_POLICY.archive_max_bytes
        {
            fail(format!(
                "archive_bytes must be 1–{}",
                COMMUNITY_ARCHIVE_POLICY.archive_max_bytes
            ));
        }
        if release.expanded_bytes == 0
            || release.expanded_bytes > COMMUNITY_ARCHIVE_POLICY.expanded_max_bytes
        {
            fail(format!(
                "expanded_bytes must be 1–{}",
                COMMUNITY_ARCHIVE_POLICY.expanded_max_bytes
            ));
        }
        if release.jet64_api == 0 {
            fail("jet64_api must be at least 1".to_owned());
        }
        if let Err(message) = validate_published_date(&release.published) {
            fail(message);
        }
        if let Some(screenshot) = &release.screenshot {
            verify_screenshot_shape(screenshot, &mut fail);
        }
    }
}

/// Shape rules for a pinned screenshot record (§12 P6.2) — the offline half;
/// the fetch witness proves the facts against the archive itself.
fn verify_screenshot_shape(screenshot: &ScreenshotFacts, fail: &mut impl FnMut(String)) {
    let digest_well_formed = screenshot.sha256.len() == SHA256_HEX_DIGITS
        && screenshot
            .sha256
            .chars()
            .all(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase());
    if !digest_well_formed {
        fail(format!(
            "screenshot sha256 must be {SHA256_HEX_DIGITS} lowercase hex digits"
        ));
    }
    if screenshot.bytes == 0 || screenshot.bytes > SCREENSHOT_MAX_BYTES {
        fail(format!("screenshot bytes must be 1–{SCREENSHOT_MAX_BYTES}"));
    }
    let in_bounds = (1..=SCREENSHOT_MAX_WIDTH).contains(&screenshot.width)
        && (1..=SCREENSHOT_MAX_HEIGHT).contains(&screenshot.height);
    if !in_bounds {
        fail(format!(
            "screenshot must be within {SCREENSHOT_MAX_WIDTH}×{SCREENSHOT_MAX_HEIGHT} \
             ({SCREENSHOT_FILE} caps)"
        ));
    }
}

/// §6.5.2 with the §6.7 exception: a release points at the canonical forge
/// archive for its pinned revision — unless upstream is gone (`unavailable`),
/// when an audited HTTPS mirror is permitted and E20's tree fingerprint keeps
/// the mirror honest.
fn verify_source_url(
    entry: &RegistryEntry,
    release: &RegistryRelease,
    fail: &mut impl FnMut(String),
) {
    if release.unavailable {
        if !release.source.starts_with("https://") {
            fail("a mirror source must be https://".to_owned());
        }
        return;
    }
    match canonical_archive_url(&entry.repository, &release.revision) {
        Ok(canonical) => {
            if release.source != canonical {
                fail(format!(
                    "source must be the canonical forge archive URL {canonical} (§6.5.2)"
                ));
            }
        }
        Err(message) => fail(message),
    }
}

/// `YYYY-MM-DD`, calendar-plausible. A date is registry history — it never needs
/// timezone precision, and it must never be a parse adventure.
pub fn validate_published_date(date: &str) -> Result<(), String> {
    let well_formed = || -> Option<()> {
        let bytes = date.as_bytes();
        if bytes.len() != 10 || bytes[4] != b'-' || bytes[7] != b'-' {
            return None;
        }
        let year: u32 = date.get(0..4)?.parse().ok()?;
        let month: u32 = date.get(5..7)?.parse().ok()?;
        let day: u32 = date.get(8..10)?.parse().ok()?;
        const UNIX_EPOCH_YEAR: u32 = 1970;
        const MONTHS_PER_YEAR: u32 = 12;
        const MAX_DAYS_PER_MONTH: u32 = 31;
        let plausible = year >= UNIX_EPOCH_YEAR
            && (1..=MONTHS_PER_YEAR).contains(&month)
            && (1..=MAX_DAYS_PER_MONTH).contains(&day);
        plausible.then_some(())
    };
    well_formed().ok_or_else(|| format!("published must be a YYYY-MM-DD date, got {date:?}"))
}

/// The bulletin: optional, small, printable. Returns the text (empty when the
/// file is absent) for the index regeneration.
fn verify_bulletin(root: &Path, report: &mut ValidationReport) -> io::Result<String> {
    let path = root.join(BULLETIN_FILE);
    if !path.is_file() {
        return Ok(String::new());
    }
    let bytes = std::fs::read(&path)?;
    if bytes.len() > BULLETIN_MAX_BYTES {
        report.push(
            FindingCode::RegistryInvalid,
            Some(BULLETIN_FILE.to_owned()),
            format!(
                "bulletin is {} bytes (maximum {BULLETIN_MAX_BYTES}) — one human voice, \
                 at most a few lines",
                bytes.len()
            ),
        );
        return Ok(String::new());
    }
    match String::from_utf8(bytes) {
        Ok(text) => {
            if text
                .chars()
                .any(|c| c.is_control() && c != '\n' && c != '\r')
            {
                report.push(
                    FindingCode::RegistryInvalid,
                    Some(BULLETIN_FILE.to_owned()),
                    "bulletin must be plain printable text",
                );
                Ok(String::new())
            } else {
                Ok(text)
            }
        }
        Err(_) => {
            report.push(
                FindingCode::RegistryInvalid,
                Some(BULLETIN_FILE.to_owned()),
                "bulletin must be valid UTF-8",
            );
            Ok(String::new())
        }
    }
}

/// The phonebook: parsed, bounds-checked, and returned for the index
/// regeneration (§6.6). Findings leave the phonebook empty — an index is never
/// rendered from listings that failed their own rules.
fn verify_boards(root: &Path, report: &mut ValidationReport) -> io::Result<BoardsFile> {
    let path = root.join(BOARDS_FILE);
    if !path.is_file() {
        return Ok(BoardsFile::default());
    }
    let source = std::fs::read_to_string(&path)?;
    let boards: BoardsFile = match toml::from_str(&source) {
        Ok(boards) => boards,
        Err(error) => {
            report.push(
                FindingCode::RegistryInvalid,
                Some(BOARDS_FILE.to_owned()),
                error.to_string(),
            );
            return Ok(BoardsFile::default());
        }
    };
    let mut clean = true;
    if boards.schema != REGISTRY_SCHEMA {
        report.push(
            FindingCode::RegistryInvalid,
            Some(BOARDS_FILE.to_owned()),
            format!("unsupported boards schema {}", boards.schema),
        );
        clean = false;
    }
    for board in &boards.boards {
        let well_formed = !board.name.trim().is_empty()
            && board.name.len() <= BOARD_NAME_MAX_BYTES
            && board.index.starts_with("https://")
            && board.note.len() <= BOARD_NOTE_MAX_BYTES;
        if !well_formed {
            report.push(
                FindingCode::RegistryInvalid,
                Some(BOARDS_FILE.to_owned()),
                format!(
                    "board {:?}: a listing is a name (≤{BOARD_NAME_MAX_BYTES} bytes), an \
                     https:// index URL, and a note (≤{BOARD_NOTE_MAX_BYTES} bytes)",
                    board.name
                ),
            );
            clean = false;
        }
    }
    Ok(if clean { boards } else { BoardsFile::default() })
}

/// §6.5.10: the committed index is byte-identical to what the entries render.
fn verify_index(
    root: &Path,
    entries: &[RegistryEntry],
    bulletin: &str,
    boards: &[BoardListing],
    report: &mut ValidationReport,
) -> io::Result<()> {
    let expected = match generate_index(entries, bulletin, boards) {
        Ok(expected) => expected,
        Err(message) => {
            report.push(
                FindingCode::RegistryInvalid,
                Some(INDEX_FILE.to_owned()),
                message,
            );
            return Ok(());
        }
    };
    let path = root.join(INDEX_FILE);
    let committed = if path.is_file() {
        std::fs::read_to_string(&path)?
    } else {
        String::new()
    };
    if committed != expected {
        report.push(
            FindingCode::IndexDrift,
            Some(INDEX_FILE.to_owned()),
            "committed index differs from the regenerated one — run `jcx index . --write`",
        );
    }
    Ok(())
}

// --- immutability (E20, §6.5.11) ---------------------------------------------

/// A `(id, version)` map for base-vs-head comparison.
fn release_map(entries: &[RegistryEntry]) -> BTreeMap<(CartId, semver::Version), &RegistryRelease> {
    let mut map = BTreeMap::new();
    for entry in entries {
        for release in &entry.releases {
            map.insert((entry.id.clone(), release.version.clone()), release);
        }
    }
    map
}

/// §6.5.11: no previously accepted `(id, version)` record changes in any way —
/// with E20's one audited exception, the transport re-pin, which is *listed for
/// fetching* rather than trusted (the fetch proves the tree is byte-identical
/// because `tree_sha256` itself may never change). Moderation state
/// (`yanked`/`unavailable`) is the one freely mutable pair.
fn verify_immutability(
    base: &[RegistryEntry],
    head: &[RegistryEntry],
    report: &mut ValidationReport,
) {
    let head_ids: Vec<&CartId> = head.iter().map(|entry| &entry.id).collect();
    for base_entry in base {
        if !head_ids.contains(&&base_entry.id) {
            report.push(
                FindingCode::ImmutabilityViolation,
                Some(format!("{CARTS_DIR}/{}.toml", base_entry.id)),
                "an accepted entry is never deleted — yank its releases instead (§6.7)",
            );
        }
    }

    let head_map = release_map(head);
    for base_entry in base {
        for base_release in &base_entry.releases {
            let coordinate = (base_entry.id.clone(), base_release.version.clone());
            let location = Some(format!("{CARTS_DIR}/{}.toml", base_entry.id));
            let Some(head_release) = head_map.get(&coordinate) else {
                if head_ids.contains(&&base_entry.id) {
                    report.push(
                        FindingCode::ImmutabilityViolation,
                        location,
                        format!(
                            "release {} was removed — an accepted release is yanked, \
                             never rewritten (E2)",
                            base_release.version
                        ),
                    );
                }
                continue;
            };
            let immutable_facts_hold = head_release.revision == base_release.revision
                && head_release.tree_sha256 == base_release.tree_sha256
                && head_release.jet64_api == base_release.jet64_api
                && head_release.network == base_release.network
                && head_release.expanded_bytes == base_release.expanded_bytes
                && head_release.published == base_release.published;
            if !immutable_facts_hold {
                report.push(
                    FindingCode::ImmutabilityViolation,
                    location,
                    format!(
                        "release {} changed an immutable fact — only the transport pair \
                         (sha256/source/archive_bytes, §7.5) and moderation flags may \
                         ever move",
                        base_release.version
                    ),
                );
            }
        }
    }
}

/// The fetch work-list when a base is known: releases that are new, transport
/// re-pinned, or whose entry-level facts (`name`/`license`/`diz`) changed and
/// therefore need the latest release's manifest re-witnessed.
fn changed_release_checks(base: &[RegistryEntry], head: &[RegistryEntry]) -> Vec<ReleaseCheck> {
    let base_map = release_map(base);
    let base_facts: BTreeMap<&CartId, (&str, &str, Option<&str>)> = base
        .iter()
        .map(|entry| {
            (
                &entry.id,
                (
                    entry.name.as_str(),
                    entry.license.as_str(),
                    entry.diz.as_deref(),
                ),
            )
        })
        .collect();

    let mut checks = Vec::new();
    for entry in head {
        let latest = entry
            .latest_release()
            .map(|release| release.version.clone());
        let facts_changed = base_facts
            .get(&entry.id)
            .is_none_or(|(name, license, diz)| {
                *name != entry.name || *license != entry.license || *diz != entry.diz.as_deref()
            });
        for release in &entry.releases {
            let coordinate = (entry.id.clone(), release.version.clone());
            let is_latest = latest.as_ref() == Some(&release.version);
            // A moderation flip (yank, unavailable) changes what the board says,
            // not what the bytes are — it never needs a re-witness on its own.
            let unchanged = base_map.get(&coordinate).is_some_and(|base_release| {
                let mut moderated = (*base_release).clone();
                moderated.yanked = release.yanked;
                moderated.unavailable = release.unavailable;
                moderated == *release
            });
            if !unchanged || (is_latest && facts_changed) {
                checks.push(release_check(entry, release, is_latest));
            }
        }
    }
    checks
}

fn all_release_checks(entries: &[RegistryEntry]) -> Vec<ReleaseCheck> {
    let mut checks = Vec::new();
    for entry in entries {
        let latest = entry
            .latest_release()
            .map(|release| release.version.clone());
        for release in &entry.releases {
            let is_latest = latest.as_ref() == Some(&release.version);
            checks.push(release_check(entry, release, is_latest));
        }
    }
    checks
}

fn release_check(
    entry: &RegistryEntry,
    release: &RegistryRelease,
    is_latest: bool,
) -> ReleaseCheck {
    ReleaseCheck {
        id: entry.id.clone(),
        name: entry.name.clone(),
        license: entry.license.clone(),
        diz: entry.diz.clone(),
        is_latest,
        release: release.clone(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const REVISION_A: &str = "0123456789abcdef0123456789abcdef01234567";
    const REVISION_B: &str = "89abcdef0123456789abcdef0123456789abcdef";
    const DIGEST_A: &str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const DIGEST_B: &str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    const DIGEST_TREE: &str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";

    fn release(version: &str) -> RegistryRelease {
        RegistryRelease {
            version: version.parse().unwrap(),
            revision: REVISION_A.to_owned(),
            source: format!("https://github.com/mira/space-hauler/archive/{REVISION_A}.tar.gz"),
            sha256: DIGEST_A.to_owned(),
            tree_sha256: DIGEST_TREE.to_owned(),
            archive_bytes: 862_208,
            expanded_bytes: 2_236_416,
            jet64_api: 1,
            network: false,
            published: "2026-08-14".to_owned(),
            yanked: false,
            unavailable: false,
            screenshot: None,
        }
    }

    fn entry() -> RegistryEntry {
        RegistryEntry {
            schema: REGISTRY_SCHEMA,
            id: CartId::parse("io.github.mira.space-hauler").unwrap(),
            name: "Space Hauler".to_owned(),
            summary: "Two-player orbital delivery game.".to_owned(),
            author: "mira".to_owned(),
            license: "MIT".to_owned(),
            categories: vec!["arcade".to_owned(), "multiplayer".to_owned()],
            repository: "https://github.com/mira/space-hauler".to_owned(),
            provenance: None,
            diz: Some("SPACE HAULER -- orbital delivery.\n".to_owned()),
            releases: vec![release("1.2.0")],
        }
    }

    /// Writes a registry checkout (carts/ + bulletin + regenerated index) so
    /// the filesystem path of `verify_registry` is exercised, not simulated.
    fn write_registry(
        label: &str,
        entries: &[RegistryEntry],
        bulletin: &str,
    ) -> std::path::PathBuf {
        let root =
            std::env::temp_dir().join(format!("jcx-registry-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join(CARTS_DIR)).unwrap();
        for entry in entries {
            let rendered = toml::to_string(entry).unwrap();
            std::fs::write(
                root.join(CARTS_DIR).join(format!("{}.toml", entry.id)),
                rendered,
            )
            .unwrap();
        }
        if !bulletin.is_empty() {
            std::fs::write(root.join(BULLETIN_FILE), bulletin).unwrap();
        }
        let index = generate_index(entries, bulletin, &[]).unwrap();
        std::fs::write(root.join(INDEX_FILE), index).unwrap();
        root
    }

    fn verify_report(root: &Path, base: Option<&Path>) -> RegistryVerification {
        verify_registry(root, base, &VerifyOptions::default()).unwrap()
    }

    #[test]
    fn a_well_formed_registry_verifies_clean() {
        let root = write_registry("clean", &[entry()], "Be kind. -- D.\n");
        let verification = verify_report(&root, None);
        assert!(verification.report.is_clean(), "{}", verification.report);
        assert_eq!(verification.entries.len(), 1);
        assert_eq!(
            verification.to_fetch.len(),
            1,
            "no base: everything is witnessed"
        );
        assert!(verification.to_fetch[0].is_latest);
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn entry_files_must_be_named_by_their_id() {
        let root = write_registry("misnamed", &[entry()], "");
        let carts = root.join(CARTS_DIR);
        std::fs::rename(
            carts.join("io.github.mira.space-hauler.toml"),
            carts.join("io.github.mira.other.toml"),
        )
        .unwrap();
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::RegistryInvalid
                && finding.message.contains("one cart, one name")));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn unknown_keys_in_an_entry_are_a_parse_failure() {
        // The E1 discipline extends to the registry: a field that would smuggle
        // behaviour into the curator's side cannot be written down either.
        let mut rendered = toml::to_string(&entry()).unwrap();
        rendered.push_str("\ninstall_hook = \"run.sh\"\n");
        assert!(RegistryEntry::parse(&rendered).is_err());
    }

    #[test]
    fn source_must_be_the_canonical_forge_url_unless_unavailable() {
        let mut smuggled = entry();
        smuggled.releases[0].source = "https://evil.example/archive.tar.gz".to_owned();
        let root = write_registry("smuggled", &[smuggled.clone()], "");
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.message.contains("canonical forge archive URL")));
        std::fs::remove_dir_all(&root).unwrap();

        // The §6.7 exception: an unavailable release may point at an audited
        // HTTPS mirror — E20's tree fingerprint keeps the mirror honest.
        smuggled.releases[0].unavailable = true;
        let root = write_registry("mirrored", &[smuggled], "");
        let verification = verify_report(&root, None);
        assert!(verification.report.is_clean(), "{}", verification.report);
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn forge_namespaces_are_proven_against_the_repository_owner() {
        let mut squatted = entry();
        squatted.repository = "https://github.com/somebody-else/space-hauler".to_owned();
        let root = write_registry("squatted", &[squatted], "");
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::NamespaceUnproven));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn submitter_checks_accept_owner_org_member_or_provenance() {
        let root = write_registry("submitter", &[entry()], "");
        let mut options = VerifyOptions {
            pr_author: Some("Mira".to_owned()),
            author_orgs: Vec::new(),
        };
        let clean = verify_registry(&root, None, &options).unwrap();
        assert!(clean.report.is_clean(), "owner submits: {}", clean.report);

        options.pr_author = Some("stranger".to_owned());
        let flagged = verify_registry(&root, None, &options).unwrap();
        assert!(flagged
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::NamespaceUnproven));

        options.author_orgs = vec!["mira".to_owned()];
        let org_member = verify_registry(&root, None, &options).unwrap();
        assert!(
            org_member.report.is_clean(),
            "org member: {}",
            org_member.report
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_submitter_is_weighed_only_against_entries_the_change_submits() {
        // The board's own files changed by someone outside the namespace: the entry is the base's,
        // byte for byte, so its author is not this change's question.
        let base = write_registry("submitter-base", &[entry()], "");
        let head = write_registry("submitter-head", &[entry()], "");
        let options = VerifyOptions {
            pr_author: Some("stranger".to_owned()),
            author_orgs: Vec::new(),
        };
        let untouched = verify_registry(&head, Some(&base), &options).unwrap();
        assert!(
            !untouched
                .report
                .findings()
                .iter()
                .any(|finding| finding.code == FindingCode::NamespaceUnproven),
            "{}",
            untouched.report
        );

        // The same stranger altering the entry is weighed as before.
        std::fs::remove_dir_all(&head).unwrap();
        let mut altered = entry();
        altered.name = "Space Hauler Deluxe".to_owned();
        let head = write_registry("submitter-altered", &[altered], "");
        let weighed = verify_registry(&head, Some(&base), &options).unwrap();
        assert!(weighed
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::NamespaceUnproven));
        for root in [base, head] {
            std::fs::remove_dir_all(root).unwrap();
        }
    }

    #[test]
    fn non_forge_namespaces_need_recorded_provenance() {
        let mut domain_scoped = entry();
        domain_scoped.id = CartId::parse("au.org.jet64.space-hauler").unwrap();
        let root = write_registry("domain-bare", &[domain_scoped.clone()], "");
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::NamespaceUnproven));
        std::fs::remove_dir_all(&root).unwrap();

        domain_scoped.provenance =
            Some("jet64.org.au/.well-known/jet64-carts verified 2026-08-14".to_owned());
        let root = write_registry("domain-proven", &[domain_scoped], "");
        let verification = verify_report(&root, None);
        assert!(verification.report.is_clean(), "{}", verification.report);
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn immutability_admits_only_transport_repins_and_moderation_flags() {
        let accepted = entry();
        let base = write_registry("imm-base", std::slice::from_ref(&accepted), "");

        // A moderation flip is free.
        let mut yanked = accepted.clone();
        yanked.releases[0].yanked = true;
        let head = write_registry("imm-yank", &[yanked], "");
        let verification = verify_report(&head, Some(&base));
        assert!(verification.report.is_clean(), "{}", verification.report);
        assert!(
            verification.to_fetch.is_empty(),
            "a flag flip re-witnesses nothing"
        );
        std::fs::remove_dir_all(&head).unwrap();

        // A transport re-pin (§7.5) passes offline but lands on the fetch list.
        let mut repinned = accepted.clone();
        repinned.releases[0].sha256 = DIGEST_B.to_owned();
        repinned.releases[0].unavailable = true; // mirror source stays legal
        repinned.releases[0].source = "https://mirror.example/space-hauler.tar.gz".to_owned();
        let head = write_registry("imm-repin", &[repinned], "");
        let verification = verify_report(&head, Some(&base));
        assert!(verification.report.is_clean(), "{}", verification.report);
        assert_eq!(
            verification.to_fetch.len(),
            1,
            "the re-pin must be witnessed"
        );
        std::fs::remove_dir_all(&head).unwrap();

        // Content may never move.
        let mut rewritten = accepted.clone();
        rewritten.releases[0].revision = REVISION_B.to_owned();
        rewritten.releases[0].source =
            format!("https://github.com/mira/space-hauler/archive/{REVISION_B}.tar.gz");
        let head = write_registry("imm-rewrite", &[rewritten], "");
        let verification = verify_report(&head, Some(&base));
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::ImmutabilityViolation));
        std::fs::remove_dir_all(&head).unwrap();

        // A release (or a whole entry) never vanishes.
        let mut emptied = accepted.clone();
        emptied.releases.clear();
        let head = write_registry("imm-empty", &[emptied], "");
        let verification = verify_report(&head, Some(&base));
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::ImmutabilityViolation));
        std::fs::remove_dir_all(&head).unwrap();

        let head = write_registry("imm-gone", &[], "");
        let verification = verify_report(&head, Some(&base));
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::ImmutabilityViolation
                && finding.message.contains("never deleted")));
        std::fs::remove_dir_all(&head).unwrap();
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn changed_entry_facts_rewitness_the_latest_release() {
        let accepted = entry();
        let base = write_registry("facts-base", std::slice::from_ref(&accepted), "");
        let mut renamed = accepted.clone();
        renamed.name = "Space Hauler DX".to_owned();
        let head = write_registry("facts-head", &[renamed], "");
        let verification = verify_report(&head, Some(&base));
        assert!(verification.report.is_clean(), "{}", verification.report);
        assert_eq!(verification.to_fetch.len(), 1);
        assert!(verification.to_fetch[0].is_latest);
        std::fs::remove_dir_all(&head).unwrap();
        std::fs::remove_dir_all(&base).unwrap();
    }

    #[test]
    fn the_index_is_deterministic_sorted_and_drift_checked() {
        let mut second = entry();
        second.id = CartId::parse("io.github.mira.aardvark").unwrap();
        second.name = "Aardvark".to_owned();
        second.releases = vec![release("0.1.0"), release("0.2.0")];
        let entries = [entry(), second];
        let bulletin = "The board is young. -- D.\n";

        let first_render = generate_index(&entries, bulletin, &[]).unwrap();
        let second_render = generate_index(&entries, bulletin, &[]).unwrap();
        assert_eq!(first_render, second_render);
        assert!(first_render.ends_with('\n'));
        assert!(
            first_render.contains("\"boards\": []"),
            "the phonebook rides the index even when empty (§6.6)"
        );

        let aardvark = first_render.find("aardvark").unwrap();
        let hauler = first_render.find("space-hauler").unwrap();
        assert!(aardvark < hauler, "carts are sorted by id");
        let v2 = first_render.find("\"0.2.0\"").unwrap();
        let v1 = first_render.find("\"0.1.0\"").unwrap();
        assert!(v2 < v1, "releases are newest-first");
        assert!(first_render.contains("\"generated\": \"2026-08-14\""));

        let root = write_registry("drift", &entries, bulletin);
        std::fs::write(root.join(INDEX_FILE), "{}\n").unwrap();
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::IndexDrift));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn bulletin_and_boards_bounds_hold() {
        let root = write_registry(
            "bulletin",
            &[entry()],
            "x".repeat(BULLETIN_MAX_BYTES + 1).as_str(),
        );
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.path.as_deref() == Some(BULLETIN_FILE)));
        std::fs::remove_dir_all(&root).unwrap();

        let root = write_registry("boards", &[entry()], "");
        std::fs::write(
            root.join(BOARDS_FILE),
            "schema = 1\n\n[[boards]]\nname = \"Night Owl BBS\"\nindex = \"http://insecure.example/index-v1.json\"\nnote = \"community board\"\n",
        )
        .unwrap();
        let verification = verify_report(&root, None);
        assert!(verification
            .report
            .findings()
            .iter()
            .any(|finding| finding.path.as_deref() == Some(BOARDS_FILE)));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn canonical_urls_are_derived_per_forge_and_nothing_else() {
        assert_eq!(
            canonical_archive_url("https://github.com/mira/space-hauler", REVISION_A).unwrap(),
            format!("https://github.com/mira/space-hauler/archive/{REVISION_A}.tar.gz")
        );
        assert_eq!(
            canonical_archive_url("https://gitlab.com/mira/space-hauler.git", REVISION_A).unwrap(),
            format!(
                "https://gitlab.com/mira/space-hauler/-/archive/{REVISION_A}/space-hauler-{REVISION_A}.tar.gz"
            )
        );
        assert!(canonical_archive_url("http://github.com/mira/cart", REVISION_A).is_err());
        assert!(canonical_archive_url("https://evil.example/mira/cart", REVISION_A).is_err());
        assert!(canonical_archive_url("https://github.com/mira", REVISION_A).is_err());
        assert!(canonical_archive_url("https://github.com/mira/cart/extra", REVISION_A).is_err());

        // The screenshot's raw-file URL follows the same forge discipline (P6.2).
        assert_eq!(
            canonical_screenshot_url("https://github.com/mira/space-hauler", REVISION_A).unwrap(),
            format!(
                "https://raw.githubusercontent.com/mira/space-hauler/{REVISION_A}/screenshot.png"
            )
        );
        assert_eq!(
            canonical_screenshot_url("https://gitlab.com/mira/space-hauler", REVISION_A).unwrap(),
            format!("https://gitlab.com/mira/space-hauler/-/raw/{REVISION_A}/screenshot.png")
        );
        assert!(canonical_screenshot_url("https://evil.example/mira/cart", REVISION_A).is_err());
    }

    /// §12 P6.2's offline half: a pinned screenshot record must be shaped like
    /// attested facts — digest, byte cap, dimension caps.
    #[test]
    fn screenshot_records_hold_their_shape() {
        let mut with_screenshot = entry();
        with_screenshot.releases[0].screenshot = Some(ScreenshotFacts {
            sha256: "not-hex".to_owned(),
            bytes: 0,
            width: SCREENSHOT_MAX_WIDTH + 1,
            height: 400,
        });
        let root = write_registry("shot-shape", &[with_screenshot], "");
        let verification = verify_report(&root, None);
        let screenshot_findings = verification
            .report
            .findings()
            .iter()
            .filter(|finding| finding.message.contains("screenshot"))
            .count();
        assert_eq!(
            screenshot_findings, 3,
            "digest, bytes, and dimensions each named"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    /// §6.6: the phonebook rides the generated index, verbatim and in file order.
    #[test]
    fn the_phonebook_rides_the_index() {
        let boards = vec![BoardListing {
            name: "Night Owl BBS".to_owned(),
            index: "https://example.org/jet64/index-v1.json".to_owned(),
            note: "community board, unreviewed by the official sysop".to_owned(),
        }];
        let rendered = generate_index(&[entry()], "", &boards).unwrap();
        assert!(rendered.contains("\"Night Owl BBS\""), "{rendered}");
        assert!(
            rendered.contains("\"https://example.org/jet64/index-v1.json\""),
            "{rendered}"
        );
        assert!(
            rendered.contains("unreviewed by the official sysop"),
            "{rendered}"
        );
    }

    #[test]
    fn revisions_must_be_full_lowercase_hashes() {
        assert!(validate_revision(REVISION_A).is_ok());
        assert!(validate_revision("main").is_err());
        assert!(validate_revision("0123abc").is_err(), "short hash");
        assert!(validate_revision(&REVISION_A.to_uppercase()).is_err());
    }

    #[test]
    fn published_dates_are_plain_and_plausible() {
        assert!(validate_published_date("2026-08-18").is_ok());
        for invalid in [
            "2026-8-18",
            "18-08-2026",
            "2026-13-01",
            "2026-00-10",
            "yesterday",
        ] {
            assert!(
                validate_published_date(invalid).is_err(),
                "accepted {invalid}"
            );
        }
    }

    #[test]
    fn forge_scoped_accounts_parse_and_only_for_known_forges() {
        let github = CartId::parse("io.github.jet64-carts.asteroids").unwrap();
        assert_eq!(
            forge_scoped_account(&github),
            Some(("github.com", "jet64-carts"))
        );
        let gitlab = CartId::parse("io.gitlab.mira.cart").unwrap();
        assert_eq!(forge_scoped_account(&gitlab), Some(("gitlab.com", "mira")));
        let domain = CartId::parse("au.org.jet64.luminvasion").unwrap();
        assert_eq!(forge_scoped_account(&domain), None);
    }
}
