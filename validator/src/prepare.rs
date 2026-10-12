// validator/src/prepare.rs
//! The author's half of the counter (docs/cart_exchange/cart_exchange.md E21): the one
//! implementation of *prepare* — accept a release archive, then render the exact
//! registry-entry TOML and pull-request body a submission needs — shared by the
//! `jcx prepare` command and the Workstation's Prepare Submission panel, so the
//! two can never drift. "Nobody should ever hand-compute a SHA-256" is enforced
//! here by construction: every digest in the rendered entry is tool output.
//!
//! The module is **network-free**, like the rest of the library: callers hand it
//! an archive already on disk (the `jcx` binary downloads with `remote::download`;
//! the Workstation with its own policy-governed fetcher). It also owns the
//! author-side manifest generator ([`render_cart_manifest`]) — the `cart.toml`
//! the Workstation writes from `publish.ron` is rendered and re-parsed through
//! the same schema types CI enforces, so a generated manifest cannot be invalid.

use std::path::{Path, PathBuf};

use crate::archive::{extract_tar_gz, COMMUNITY_ARCHIVE_POLICY};
use crate::fingerprint::{collect_relative_file_paths, sha256_hex_of_file};
use crate::manifest::{
    Capabilities, CartId, CartManifest, PackageSpec, ReleaseIdentity, MANIFEST_SCHEMA,
};
use crate::policy::{extract_include_names, CLIPBOARD_HEADERS, NETWORK_HEADERS};
use crate::registry::{parse_forge_repository, SUMMARY_MAX_BYTES};
use crate::validate::{accept_tree, TreeValidation};
use crate::PolicySet;

/// Read-buffer size for hashing and downloading.
pub const IO_BUFFER_BYTES: usize = 64 * 1024;

/// Everything one archive acceptance produced — the shared substance of
/// `jcx validate`, `jcx prepare`, the Workstation's Prepare Submission, and the
/// per-release CI witness.
pub struct Acceptance {
    pub validation: TreeValidation,
    pub sha256: String,
    pub archive_bytes: u64,
    pub expanded_bytes: u64,
    /// Where the (pruned) release surface now lives, for callers that keep it.
    pub surface: PathBuf,
}

/// Hashes, extracts, and accepts a local archive: the one pipeline every caller
/// runs, network or not.
pub fn accept_archive(
    archive: &Path,
    scratch: &Path,
    expected: Option<&ReleaseIdentity>,
) -> Result<Acceptance, String> {
    let mut buffer = vec![0u8; IO_BUFFER_BYTES];
    let sha256 = sha256_hex_of_file(archive, &mut buffer)
        .map_err(|error| format!("cannot read {}: {error}", archive.display()))?;
    let archive_bytes = std::fs::metadata(archive)
        .map_err(|error| format!("cannot stat {}: {error}", archive.display()))?
        .len();

    let staging = scratch.join("staging");
    let tree = extract_tar_gz(archive, &staging, &COMMUNITY_ARCHIVE_POLICY)
        .map_err(|error| format!("archive rejected: {error}"))?;
    let validation = accept_tree(&staging, PolicySet::builtin(), expected)
        .map_err(|error| format!("validation I/O failure: {error}"))?;
    Ok(Acceptance {
        validation,
        sha256,
        archive_bytes,
        expanded_bytes: tree.expanded_bytes,
        surface: staging,
    })
}

// --- the generated cart.toml (§5.3 step 1, Workstation-assisted from P5) -------

/// Whether any C source under `root` includes a networking header — the same
/// judgement CI's capability check applies to a release surface (§6.5.7),
/// offered here so a *generator* can declare `network` honestly before the
/// tree is ever archived. Same rule as validation: any include of a
/// [`NETWORK_HEADERS`] name anywhere in the tree is reach, because the guest
/// compiler builds every unit under a project.
pub fn tree_reaches_network(root: &Path) -> std::io::Result<bool> {
    tree_includes_any(root, NETWORK_HEADERS)
}

/// Whether any C source under `root` includes `jet_clipboard.h` — the `clipboard`
/// capability's verdict, by the same rule as [`tree_reaches_network`].
pub fn tree_reaches_clipboard(root: &Path) -> std::io::Result<bool> {
    tree_includes_any(root, CLIPBOARD_HEADERS)
}

/// Whether any `.c` or `.h` under `root` includes one of `headers`.
fn tree_includes_any(root: &Path, headers: &[&str]) -> std::io::Result<bool> {
    for relative in collect_relative_file_paths(root)? {
        if !std::path::Path::new(&relative)
            .extension()
            .is_some_and(|extension| extension == "c" || extension == "h")
        {
            continue;
        }
        let source = std::fs::read_to_string(root.join(&relative))?;
        if extract_include_names(&source)
            .iter()
            .any(|name| headers.contains(&name.as_str()))
        {
            return Ok(true);
        }
    }
    Ok(false)
}

/// The forge-scoped cart ID a repository URL earns under E19 —
/// `https://github.com/Mira/Space_Hauler` → `io.github.mira.space-hauler`.
/// Forge accounts are case-insensitive, so lowercasing loses nothing; `_` maps
/// to `-` because an ID's alphabet has no underscore. The result is parsed
/// through [`CartId`], so a repository whose name cannot become a valid ID is
/// an `Err` naming the rule, never a malformed identity.
pub fn forge_scoped_id(repository: &str) -> Result<CartId, String> {
    let repo = parse_forge_repository(repository)?;
    let prefix = match repo.host.as_str() {
        "github.com" => "io.github",
        "gitlab.com" => "io.gitlab",
        other => {
            return Err(format!(
                "no forge-scoped namespace for {other:?} (supported: github.com, gitlab.com) — \
                 choose your own reverse-DNS ID and record provenance in the PR (E19)"
            ));
        }
    };
    let normalise = |part: &str| part.to_ascii_lowercase().replace('_', "-");
    CartId::parse(&format!(
        "{prefix}.{}.{}",
        normalise(&repo.owner),
        normalise(&repo.name)
    ))
}

/// The include set a generated manifest starts from: source anywhere in the
/// tree, the `data/` assets, the licence family, the README, and the author's
/// card. Files that match nothing simply are not part of the release (§5.2) —
/// an author narrows or widens this by editing the file the generator wrote.
pub const DEFAULT_INCLUDE: &[&str] = &[
    "**/*.c",
    "**/*.h",
    "data/**",
    "LICENSE",
    "COPYING",
    "NOTICE",
    "README.md",
    "file_id.diz",
];

/// The facts a `cart.toml` is rendered from. The caller owns where they come
/// from (`publish.ron` in the Workstation); this module owns that what they
/// become is schema-valid.
pub struct ManifestFields<'a> {
    pub id: &'a CartId,
    pub name: &'a str,
    pub version: &'a semver::Version,
    /// Minimum `JET64_API_GENERATION` the cart requires (E9).
    pub jet64_api: u32,
    /// SPDX expression from the registry allowlist (§6.4).
    pub license: &'a str,
    /// Whether the cart's include tree reaches a networking header — the caller
    /// scans; the declaration must match the tree in both directions (§6.5.7).
    pub network: bool,
    /// Whether it reaches `jet_clipboard.h` — scanned and matched the same way.
    pub clipboard: bool,
}

/// Renders a schema-1 `cart.toml` from `fields` with the [`DEFAULT_INCLUDE`]
/// surface, then proves the result by re-parsing it through the same
/// [`CartManifest`] schema CI enforces — a generated manifest that would not
/// survive validation is an `Err` here, never a file on disk.
pub fn render_cart_manifest(fields: &ManifestFields) -> Result<String, String> {
    let manifest = CartManifest {
        schema: MANIFEST_SCHEMA,
        id: fields.id.clone(),
        name: fields.name.trim().to_owned(),
        version: fields.version.clone(),
        jet64_api: fields.jet64_api,
        license: fields.license.to_owned(),
        package: PackageSpec {
            include: DEFAULT_INCLUDE
                .iter()
                .map(|pattern| pattern.to_string())
                .collect(),
        },
        capabilities: Capabilities {
            network: fields.network,
            clipboard: fields.clipboard,
        },
    };
    let body =
        toml::to_string(&manifest).map_err(|error| format!("cannot render cart.toml: {error}"))?;
    let rendered =
        format!("# cart.toml — the Cartridge Exchange package manifest (schema 1)\n{body}");
    // The round-trip proof: parse enforces every semantic rule serde cannot.
    let parsed = CartManifest::parse(&rendered)?;
    if parsed != manifest {
        return Err("rendered cart.toml did not round-trip".to_owned());
    }
    Ok(rendered)
}

// --- the rendered submission (§6.2, E21) ---------------------------------------

/// The marker a pre-push release block carries on both of its comment lines.
/// `scripts/exchange/stage_registry.sh` refuses to export a registry still
/// holding it, and `scripts/exchange/finalise_release.sh` deletes it when the
/// pushed digests arrive — so the string is the contract between the two, and
/// lives here, where the block that carries it is rendered.
pub const PENDING_TRANSPORT_MARKER: &str = "# PENDING FIRST PUSH";

/// The `sha256` a pre-push block stands in with. Sixty-four zeroes: well-formed
/// enough for the registry to parse and verify offline, and unmistakably not a
/// digest of anything.
const PENDING_SHA256: &str = "0000000000000000000000000000000000000000000000000000000000000000";

/// The `archive_bytes` a pre-push block stands in with. One, not zero, because
/// the entry schema requires a positive size — an archive of no bytes is a
/// different lie from an archive not yet served.
const PENDING_ARCHIVE_BYTES: u64 = 1;

/// The `[[releases]]` block for a clean acceptance — the one fragment the full
/// entry template and `jcx prepare --entry-fragment` splicing share. `published`
/// is a parameter (see [`today_utc`]) so the output stays a pure function of its
/// inputs and the tests stay deterministic.
///
/// Callable only for a clean acceptance: a rejected archive has no manifest or
/// fingerprint worth pinning, and the `expect`s document that contract.
pub fn release_block(
    acceptance: &Acceptance,
    revision: &str,
    source_url: &str,
    published: &str,
) -> String {
    render_release_block(
        acceptance,
        revision,
        source_url,
        published,
        &acceptance.sha256,
        acceptance.archive_bytes,
    )
}

/// The same block for a seed staged *ahead of its repository's first push*
/// (§12 P1.3): the fingerprint, sizes and screenshot are real — they are
/// properties of the tree, and a forge cannot change them — while the transport
/// pair describes a gzip stream no server has generated yet (§7.5), and is
/// therefore emitted as marked placeholders rather than the digests of a
/// locally-built archive the forge will never reproduce.
///
/// The block is preceded by the [`PENDING_TRANSPORT_MARKER`] comment lines that
/// stop the registry being exported in this state.
pub fn pending_release_block(
    acceptance: &Acceptance,
    revision: &str,
    source_url: &str,
    published: &str,
) -> String {
    format!(
        "{PENDING_TRANSPORT_MARKER}: sha256/archive_bytes below are placeholders until\n\
         {PENDING_TRANSPORT_MARKER}: scripts/exchange/finalise_release.sh re-pins them.\n\
         {}",
        render_release_block(
            acceptance,
            revision,
            source_url,
            published,
            PENDING_SHA256,
            PENDING_ARCHIVE_BYTES,
        )
    )
}

/// The one rendering both blocks share; the transport pair is the only thing
/// they disagree about.
fn render_release_block(
    acceptance: &Acceptance,
    revision: &str,
    source_url: &str,
    published: &str,
    sha256: &str,
    archive_bytes: u64,
) -> String {
    let manifest = acceptance
        .validation
        .manifest
        .as_ref()
        .expect("a clean report implies a parsed manifest");
    let fingerprint = acceptance
        .validation
        .fingerprint
        .as_ref()
        .expect("a clean report implies a fingerprint");
    // The screenshot line exists only when the archive carried a valid
    // screenshot.png (§12 P6.2) — attested facts, never hand-typed.
    let screenshot = acceptance
        .validation
        .screenshot
        .as_ref()
        .map(|facts| {
            format!(
                "screenshot = {{ sha256 = \"{}\", bytes = {}, width = {}, height = {} }}\n",
                facts.sha256, facts.bytes, facts.width, facts.height
            )
        })
        .unwrap_or_default();
    format!(
        "[[releases]]\n\
         version = \"{}\"\n\
         revision = \"{revision}\"\n\
         source = \"{source_url}\"\n\
         sha256 = \"{}\"\n\
         tree_sha256 = \"{fingerprint}\"\n\
         archive_bytes = {}\n\
         expanded_bytes = {}\n\
         jet64_api = {}\n\
         network = {}\n\
         published = \"{published}\"\n\
         yanked = false\n\
         unavailable = false\n\
         {screenshot}",
        manifest.version,
        sha256,
        archive_bytes,
        acceptance.expanded_bytes,
        manifest.jet64_api,
        manifest.capabilities.network,
    )
}

/// The exact registry entry (§6.2) a submission adds as `carts/<id>.toml`.
/// Facts the archive proves (name, licence, DIZ, digests) are emitted verbatim;
/// curator-side fields an author cannot self-attest are emitted as marked
/// blanks for the reviewer — except `summary`, which a caller that knows the
/// author's own one-liner may prefill (the Workstation passes `publish.ron`'s
/// description; the CLI passes `None`). An over-long or empty summary falls
/// back to the marked blank rather than emitting a line CI would reject.
pub fn registry_entry(
    acceptance: &Acceptance,
    repository: &str,
    revision: &str,
    source_url: &str,
    published: &str,
    summary: Option<&str>,
) -> String {
    let manifest = acceptance
        .validation
        .manifest
        .as_ref()
        .expect("a clean report implies a parsed manifest");
    let author = manifest
        .id
        .as_str()
        .split('.')
        .nth(2)
        .unwrap_or("EDIT-ME")
        .to_owned();
    let summary = summary
        .map(str::trim)
        .filter(|text| !text.is_empty() && text.len() <= SUMMARY_MAX_BYTES)
        .unwrap_or("EDIT ME: one plain sentence for the listing row");

    let mut entry = String::new();
    entry.push_str("schema = 1\n");
    entry.push_str(&format!("id = \"{}\"\n", manifest.id));
    entry.push_str(&format!("name = {}\n", toml_string(&manifest.name)));
    entry.push_str(&format!("summary = {}\n", toml_string(summary)));
    entry.push_str(&format!("author = \"{author}\"\n"));
    entry.push_str(&format!("license = \"{}\"\n", manifest.license));
    entry.push_str("categories = [\"EDIT ME\"]\n");
    entry.push_str(&format!("repository = \"{repository}\"\n"));
    entry.push_str(&diz_field(acceptance.validation.diz.as_deref()));
    entry.push('\n');
    entry.push_str(&release_block(acceptance, revision, source_url, published));
    entry
}

/// The registry path a submission's entry file lands at.
pub fn entry_path(id: &CartId) -> String {
    format!("{}/{id}.toml", crate::registry::CARTS_DIR)
}

/// The pull-request body a submission opens with — the E19/§6.4/E15
/// confirmations the PR template asks for, pre-filled with the release's own
/// facts so the author edits, never composes.
pub fn pr_body(acceptance: &Acceptance) -> String {
    let manifest = acceptance
        .validation
        .manifest
        .as_ref()
        .expect("a clean report implies a parsed manifest");
    format!(
        "Adds {} {} to the board.\n\
         \n\
         - [ ] I own the `{}` namespace (E19: forge account or recorded provenance)\n\
         - [ ] The licence ({}) covers every file in the release\n\
         - [ ] Capabilities are declared honestly (network = {}, clipboard = {})\n\
         - [ ] `jcx prepare` ran clean; digests above are untouched tool output\n",
        manifest.name,
        manifest.version,
        manifest.id,
        manifest.license,
        manifest.capabilities.network,
        manifest.capabilities.clipboard,
    )
}

/// The entry's `diz` field as a TOML multi-line literal — the exact bytes the
/// board will display (§6.3). The literal delimiter is the one sequence a DIZ
/// cannot carry; `validate_diz` admits it, so emission simply declines and asks
/// for a hand-written basic string in that corner case. No DIZ, no field.
fn diz_field(diz: Option<&str>) -> String {
    let Some(diz) = diz else { return String::new() };
    const TOML_LITERAL_DELIMITER: &str = "'''";
    if diz.contains(TOML_LITERAL_DELIMITER) {
        return "# diz omitted: the card contains ''' — add it as a TOML basic string\n".to_owned();
    }
    format!(
        "diz = {TOML_LITERAL_DELIMITER}\n{}\n{TOML_LITERAL_DELIMITER}\n",
        diz.trim_end_matches(['\n', '\r'])
    )
}

/// A TOML basic string with the two escapes entry text can need. Manifest
/// names and summaries are printable prose (control characters cannot survive
/// validation), so quote and backslash are the whole escape set.
fn toml_string(text: &str) -> String {
    format!("\"{}\"", text.replace('\\', "\\\\").replace('"', "\\\""))
}

// --- the calendar ---------------------------------------------------------------

/// Today's UTC date as `YYYY-MM-DD`, from the system clock — no date dependency.
/// Callers pass the result into [`release_block`] as `published`, keeping the
/// rendering itself a pure function.
pub fn today_utc() -> String {
    let seconds = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .expect("the system clock reads before 1970")
        .as_secs();
    const SECONDS_PER_DAY: u64 = 86_400;
    let (year, month, day) = civil_from_days((seconds / SECONDS_PER_DAY) as i64);
    format!("{year:04}-{month:02}-{day:02}")
}

/// Days-since-Unix-epoch to civil `(year, month, day)`, after Howard Hinnant's
/// public-domain `civil_from_days`: the epoch shifts to 0000-03-01 so each leap
/// day lands at its 400-year cycle's end and the arithmetic stays branch-free.
fn civil_from_days(days_since_epoch: i64) -> (i64, i64, i64) {
    let shifted = days_since_epoch + 719_468;
    let era = shifted.div_euclid(146_097);
    let day_of_era = shifted.rem_euclid(146_097);
    let year_of_era =
        (day_of_era - day_of_era / 1460 + day_of_era / 36_524 - day_of_era / 146_096) / 365;
    let day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let march_month = (5 * day_of_year + 2) / 153;
    let day = day_of_year - (153 * march_month + 2) / 5 + 1;
    let month = if march_month < 10 {
        march_month + 3
    } else {
        march_month - 9
    };
    (year_of_era + era * 400 + i64::from(month <= 2), month, day)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::testkit::RawTarBuilder;

    #[test]
    fn civil_dates_render_correctly() {
        // Spot checks against known dates: the epoch, a leap day, a plain day.
        assert_eq!(civil_from_days(0), (1970, 1, 1));
        assert_eq!(civil_from_days(11_016), (2000, 2, 29));
        assert_eq!(civil_from_days(20_318), (2025, 8, 18));
    }

    fn fields<'a>(id: &'a CartId, version: &'a semver::Version) -> ManifestFields<'a> {
        ManifestFields {
            id,
            name: "Space Hauler",
            version,
            jet64_api: 1,
            license: "MIT",
            network: false,
            clipboard: false,
        }
    }

    #[test]
    fn forge_ids_are_derived_normalised_and_still_rule_checked() {
        assert_eq!(
            forge_scoped_id("https://github.com/Mira/Space_Hauler")
                .unwrap()
                .as_str(),
            "io.github.mira.space-hauler"
        );
        assert_eq!(
            forge_scoped_id("https://gitlab.com/mira/rockets.git")
                .unwrap()
                .as_str(),
            "io.gitlab.mira.rockets"
        );
        assert!(
            forge_scoped_id("https://sr.ht/~mira/rockets").is_err(),
            "unsupported forge"
        );
        assert!(forge_scoped_id("not a url").is_err());
    }

    #[test]
    fn network_reach_is_judged_over_the_whole_tree_like_ci_judges_it() {
        let root = std::env::temp_dir().join(format!("jcx-netscan-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("sub")).unwrap();
        std::fs::write(root.join("main.c"), "#include \"jet64.h\"\n").unwrap();
        std::fs::write(root.join("data.txt"), "jet_http.h is only prose here\n").unwrap();
        assert!(
            !tree_reaches_network(&root).unwrap(),
            "no networking header reached"
        );

        // Reach through a nested header counts — the compiler builds every unit.
        std::fs::write(root.join("sub/wire.h"), "#include \"jet_http.h\"\n").unwrap();
        assert!(tree_reaches_network(&root).unwrap());
        assert!(
            !tree_reaches_clipboard(&root).unwrap(),
            "networking is not the clipboard"
        );
        std::fs::write(root.join("sub/paste.c"), "#include \"jet_clipboard.h\"\n").unwrap();
        assert!(tree_reaches_clipboard(&root).unwrap());
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn a_generated_manifest_round_trips_through_the_schema() {
        let id = CartId::parse("io.github.mira.space-hauler").unwrap();
        let version = semver::Version::new(1, 2, 0);
        let rendered = render_cart_manifest(&fields(&id, &version)).expect("valid fields render");
        let parsed =
            CartManifest::parse(&rendered).expect("the generator's output is schema-valid");
        assert_eq!(parsed.id, id);
        assert_eq!(parsed.name, "Space Hauler");
        assert_eq!(parsed.version, version);
        assert!(!parsed.capabilities.network);
        assert_eq!(parsed.package.include.len(), DEFAULT_INCLUDE.len());
        assert!(
            rendered.starts_with("# cart.toml"),
            "the header comment orients the reader"
        );
    }

    #[test]
    fn invalid_fields_are_refused_before_any_file_could_exist() {
        let id = CartId::parse("io.github.mira.space-hauler").unwrap();
        let version = semver::Version::new(1, 2, 0);
        let mut bad_licence = fields(&id, &version);
        bad_licence.license = "WTFPL";
        assert!(
            render_cart_manifest(&bad_licence).is_err(),
            "off-allowlist licence"
        );
        let mut bad_name = fields(&id, &version);
        bad_name.name = "   ";
        assert!(render_cart_manifest(&bad_name).is_err(), "blank name");
    }

    /// The worked fixture: a well-formed forge-style archive accepted through
    /// the real pipeline, so the rendering tests exercise genuine tool output.
    fn accepted_fixture(label: &str) -> (Acceptance, PathBuf) {
        const MANIFEST: &str = "schema = 1\n\
            id = \"io.github.mira.space-hauler\"\n\
            name = \"Space Hauler\"\n\
            version = \"1.2.0\"\n\
            jet64_api = 1\n\
            license = \"MIT\"\n";
        let bytes = RawTarBuilder::new()
            .file("space-hauler-0123abc/cart.toml", MANIFEST.as_bytes())
            .file(
                "space-hauler-0123abc/main.c",
                b"#include \"jet64.h\"\nvoid _init(void) {}\n",
            )
            .file("space-hauler-0123abc/LICENSE", b"MIT\n")
            .file(
                "space-hauler-0123abc/file_id.diz",
                b"SPACE HAULER v1.2 -- orbital delivery.\n-- mira\n",
            )
            .finish();
        let scratch =
            std::env::temp_dir().join(format!("jcx-prepare-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&scratch);
        std::fs::create_dir_all(&scratch).unwrap();
        let archive = scratch.join("release.tar.gz");
        std::fs::write(&archive, &bytes).unwrap();
        let acceptance = accept_archive(&archive, &scratch, None).expect("fixture accepts");
        assert!(
            acceptance.validation.report.is_clean(),
            "{}",
            acceptance.validation.report
        );
        (acceptance, scratch)
    }

    const REVISION: &str = "0123456789abcdef0123456789abcdef01234567";
    const REPOSITORY: &str = "https://github.com/mira/space-hauler";
    const SOURCE: &str =
        "https://github.com/mira/space-hauler/archive/0123456789abcdef0123456789abcdef01234567.tar.gz";

    /// The E21 anchor: the rendered entry is not merely plausible TOML — it
    /// parses through the registry's own schema, digests included, so what
    /// prepare emits is what CI will accept.
    #[test]
    fn the_rendered_entry_parses_through_the_registry_schema() {
        let (acceptance, scratch) = accepted_fixture("entry");
        let entry = registry_entry(
            &acceptance,
            REPOSITORY,
            REVISION,
            SOURCE,
            "2026-08-18",
            Some("Two-player orbital delivery game."),
        );
        let parsed = crate::registry::RegistryEntry::parse(&entry)
            .expect("prepare's output is registry-schema-valid");
        assert_eq!(parsed.id.as_str(), "io.github.mira.space-hauler");
        assert_eq!(parsed.name, "Space Hauler");
        assert_eq!(parsed.summary, "Two-player orbital delivery game.");
        assert_eq!(parsed.author, "mira", "derived from the namespace");
        // The TOML literal keeps the card's own trailing newline — the §6.5.8
        // witness compares DIZ text modulo exactly that, so this is byte-honest.
        assert_eq!(
            parsed.diz.as_deref(),
            Some("SPACE HAULER v1.2 -- orbital delivery.\n-- mira\n")
        );
        let release = parsed.latest_release().expect("one release");
        assert_eq!(release.revision, REVISION);
        assert_eq!(release.sha256, acceptance.sha256);
        assert_eq!(
            release.tree_sha256,
            acceptance
                .validation
                .fingerprint
                .as_ref()
                .unwrap()
                .to_string()
        );
        assert_eq!(release.archive_bytes, acceptance.archive_bytes);
        assert_eq!(release.published, "2026-08-18");
        assert_eq!(
            entry_path(&parsed.id),
            "carts/io.github.mira.space-hauler.toml"
        );
        std::fs::remove_dir_all(&scratch).unwrap();
    }

    /// §12 P1.3: a seed staged before its repository exists renders a block
    /// that the registry parses and verifies like any other — but whose two
    /// transport facts are unmistakable placeholders, marked for the one script
    /// allowed to replace them. Everything the *tree* proves is real in both
    /// forms, which is what lets a seed entry be reviewed before the push.
    #[test]
    fn a_pending_block_pins_the_tree_and_withholds_the_transport_pair() {
        let (acceptance, scratch) = accepted_fixture("pending");
        let head = registry_entry(
            &acceptance,
            REPOSITORY,
            REVISION,
            SOURCE,
            "2026-09-22",
            Some("Two-player orbital delivery game."),
        );
        // The same splice `pin_seed_entry.sh` performs: the curator's head, then
        // the tool's block.
        let entry = format!(
            "{}{}",
            head.split_inclusive('\n')
                .take_while(|line| !line.starts_with("[[releases]]"))
                .collect::<String>(),
            pending_release_block(&acceptance, REVISION, SOURCE, "2026-09-22"),
        );
        assert!(
            entry.contains(PENDING_TRANSPORT_MARKER),
            "the marker is what stops a placeholder registry being exported"
        );

        let parsed =
            crate::registry::RegistryEntry::parse(&entry).expect("a pending entry still parses");
        let release = parsed.latest_release().expect("one release");
        assert_eq!(release.sha256, PENDING_SHA256);
        assert_eq!(release.archive_bytes, PENDING_ARCHIVE_BYTES);
        // Facts of the tree, not of the transport: identical to what a served
        // acceptance would pin, which is why the push changes nothing but two
        // numbers.
        assert_eq!(release.revision, REVISION);
        assert_eq!(
            release.tree_sha256,
            acceptance
                .validation
                .fingerprint
                .as_ref()
                .unwrap()
                .to_string()
        );
        assert_eq!(release.expanded_bytes, acceptance.expanded_bytes);
        assert_eq!(
            release.screenshot.is_some(),
            acceptance.validation.screenshot.is_some()
        );
        std::fs::remove_dir_all(&scratch).unwrap();
    }

    /// §12 P6.2 end-to-end at the author's counter: a screenshot.png beside
    /// cart.toml is attested (digest, bytes, dimensions), rendered into the
    /// release block, parses through the registry's own schema — and never
    /// becomes part of the release surface it decorates.
    #[test]
    fn a_screenshot_is_attested_rendered_and_kept_out_of_the_surface() {
        use crate::fingerprint::sha256_hex_of_bytes;
        use crate::testkit::{png_header, RawTarBuilder};

        const MANIFEST: &str = "schema = 1\n\
            id = \"io.github.mira.space-hauler\"\n\
            name = \"Space Hauler\"\n\
            version = \"1.2.0\"\n\
            jet64_api = 1\n\
            license = \"MIT\"\n\
            [package]\n\
            include = [\"*.c\", \"LICENSE\"]\n";
        let picture = png_header(640, 400);
        let bytes = RawTarBuilder::new()
            .file("space-hauler-0123abc/cart.toml", MANIFEST.as_bytes())
            .file(
                "space-hauler-0123abc/main.c",
                b"#include \"jet64.h\"\nvoid _init(void) {}\n",
            )
            .file("space-hauler-0123abc/LICENSE", b"MIT\n")
            .file("space-hauler-0123abc/screenshot.png", &picture)
            .finish();
        let scratch = std::env::temp_dir().join(format!("jcx-prepare-shot-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&scratch);
        std::fs::create_dir_all(&scratch).unwrap();
        let archive = scratch.join("release.tar.gz");
        std::fs::write(&archive, &bytes).unwrap();

        let acceptance = accept_archive(&archive, &scratch, None).expect("fixture accepts");
        assert!(
            acceptance.validation.report.is_clean(),
            "{}",
            acceptance.validation.report
        );
        let facts = acceptance.validation.screenshot.as_ref().expect("attested");
        assert_eq!(facts.sha256, sha256_hex_of_bytes(&picture));
        assert_eq!(facts.bytes, picture.len() as u64);
        assert_eq!((facts.width, facts.height), (640, 400));
        assert!(
            !acceptance.surface.join("screenshot.png").exists(),
            "the picture is presentation, never release surface"
        );

        let entry = registry_entry(
            &acceptance,
            REPOSITORY,
            REVISION,
            SOURCE,
            "2026-08-18",
            None,
        );
        let parsed = crate::registry::RegistryEntry::parse(&entry)
            .expect("the rendered entry parses with its screenshot record");
        assert_eq!(
            parsed.latest_release().unwrap().screenshot.as_ref(),
            Some(facts),
            "what prepare attests is what the schema carries"
        );
        std::fs::remove_dir_all(&scratch).unwrap();
    }

    #[test]
    fn an_unusable_summary_falls_back_to_the_marked_blank() {
        let (acceptance, scratch) = accepted_fixture("summary");
        for unusable in [None, Some("   "), Some(&*"x".repeat(SUMMARY_MAX_BYTES + 1))] {
            let entry = registry_entry(
                &acceptance,
                REPOSITORY,
                REVISION,
                SOURCE,
                "2026-08-18",
                unusable,
            );
            assert!(entry.contains("EDIT ME: one plain sentence"), "{entry}");
        }
        std::fs::remove_dir_all(&scratch).unwrap();
    }

    #[test]
    fn the_pr_body_carries_the_releases_own_facts() {
        let (acceptance, scratch) = accepted_fixture("body");
        let body = pr_body(&acceptance);
        assert!(
            body.contains("Adds Space Hauler 1.2.0 to the board."),
            "{body}"
        );
        assert!(
            body.contains("`io.github.mira.space-hauler` namespace"),
            "{body}"
        );
        assert!(body.contains("(MIT)"), "{body}");
        assert!(body.contains("network = false"), "{body}");
        std::fs::remove_dir_all(&scratch).unwrap();
    }
}
