// validator/src/validate.rs
//! The acceptance pipeline: what registry CI runs on every submission and the
//! Workstation runs before any tree is committed under `carts/`.
//!
//! Canonical order, shared by both callers through [`accept_tree`]:
//!
//! 1. **Prune** to the release surface — `[package] include` narrows what the
//!    release *is* (§5.2); excluded files are removed from the staging tree and
//!    are never installed, fingerprinted, or listed.
//! 2. **Validate** the surface — manifest, identity, file allowlist, forbidden
//!    symbols, capability honesty, the declared API generation against the names
//!    the source uses, licence presence, DIZ bounds.
//! 3. **Fingerprint** the surface (only when clean) — the number CI publishes as
//!    `tree_sha256` and the installer stores as the baseline (E20).
//!
//! Environmental failures (unreadable disk) are `io::Error`s; policy failures are
//! findings in a deterministic [`ValidationReport`]. The distinction matters:
//! a finding is evidence about the release, an error is weather.

use std::io;
use std::path::Path;

use crate::api_generations::{ApiGenerations, ApiUse};
use crate::fingerprint::{self, TreeFingerprint, TreeManifest};
use crate::manifest::{
    validate_diz, CartManifest, IncludePattern, ReleaseIdentity, CART_MANIFEST_FILE, DIZ_FILE,
};
use crate::policy::{
    extract_include_names, file_name_is_allowed, PolicySet, CLIPBOARD_HEADERS, NETWORK_HEADERS,
};
use crate::report::{FindingCode, ValidationReport};
use crate::screenshot::{validate_screenshot, ScreenshotFacts, SCREENSHOT_FILE};

/// Everything one acceptance run produced.
#[derive(Debug)]
pub struct TreeValidation {
    /// The parsed manifest, when `cart.toml` parsed at all.
    pub manifest: Option<CartManifest>,
    /// The author's `file_id.diz` text, when present and within bounds.
    pub diz: Option<String>,
    /// The attested facts of `screenshot.png`, when the *pre-prune* tree ships
    /// a valid one (§12 P6.2). The picture is presentation, not source — it is
    /// read before the include-filter removes it, and it is never part of the
    /// release surface or its fingerprint.
    pub screenshot: Option<ScreenshotFacts>,
    pub report: ValidationReport,
    /// Files removed by the include-filter (forward-slashed, sorted).
    pub pruned: Vec<String>,
    /// The release surface's per-file manifest — computed only for a clean
    /// report, because a rejected tree has no identity worth vouching for. The
    /// installer stores it as the baseline for later edit itemisation (§9.4).
    pub tree_manifest: Option<TreeManifest>,
    /// The manifest's fingerprint — the number CI publishes as `tree_sha256`
    /// and the installer stores as the baseline (E20). Present exactly when
    /// [`Self::tree_manifest`] is.
    pub fingerprint: Option<TreeFingerprint>,
}

/// Runs the full acceptance pipeline over the extracted tree at `root`.
///
/// **Mutates the tree**: step 1 deletes files outside the release surface. Callers
/// hand this a private staging directory (never a user's project), which is
/// exactly what the §9.2 install transaction and CI's checkout both do.
pub fn accept_tree(
    root: &Path,
    policy: &PolicySet,
    expected: Option<&ReleaseIdentity>,
) -> io::Result<TreeValidation> {
    let mut report = ValidationReport::new();

    let manifest = read_manifest(root, &mut report)?;
    // The screenshot is judged before pruning removes it: it belongs to the
    // *repository* at the pinned revision, deliberately outside the release
    // surface (§12 P6.2), and this is the one moment both are in hand.
    let screenshot = read_screenshot(root, &mut report)?;
    let pruned = match &manifest {
        Some(manifest) => prune_to_release_surface(root, manifest)?,
        None => Vec::new(),
    };
    if let (Some(manifest), Some(expected)) = (&manifest, expected) {
        check_identity(manifest, expected, &mut report);
    }

    let diz = validate_surface(root, policy, manifest.as_ref(), &mut report)?;

    report.finalize();
    let tree_manifest = if report.is_clean() {
        Some(fingerprint::manifest_tree(root)?)
    } else {
        None
    };
    let fingerprint = tree_manifest.as_ref().map(TreeManifest::fingerprint);
    Ok(TreeValidation {
        manifest,
        diz,
        screenshot,
        report,
        pruned,
        tree_manifest,
        fingerprint,
    })
}

/// Reads and attests `screenshot.png` at the (pre-prune) tree root. Absent is
/// the common, quiet case; a malformed or over-cap picture is a finding — the
/// release is evidence, and a picture that breaks its bounds is evidence too.
fn read_screenshot(
    root: &Path,
    report: &mut ValidationReport,
) -> io::Result<Option<ScreenshotFacts>> {
    let path = root.join(SCREENSHOT_FILE);
    if !path.is_file() {
        return Ok(None);
    }
    let bytes = std::fs::read(&path)?;
    match validate_screenshot(&bytes) {
        Ok(dimensions) => Ok(Some(ScreenshotFacts {
            sha256: fingerprint::sha256_hex_of_bytes(&bytes),
            bytes: bytes.len() as u64,
            width: dimensions.width,
            height: dimensions.height,
        })),
        Err(message) => {
            report.push(
                FindingCode::ScreenshotInvalid,
                Some(SCREENSHOT_FILE.to_owned()),
                message,
            );
            Ok(None)
        }
    }
}

fn read_manifest(root: &Path, report: &mut ValidationReport) -> io::Result<Option<CartManifest>> {
    let path = root.join(CART_MANIFEST_FILE);
    if !path.is_file() {
        report.push(
            FindingCode::ManifestMissing,
            None,
            format!("{CART_MANIFEST_FILE} is required at the cart root"),
        );
        return Ok(None);
    }
    let source = std::fs::read_to_string(&path)?;
    match CartManifest::parse(&source) {
        Ok(manifest) => Ok(Some(manifest)),
        Err(message) => {
            report.push(
                FindingCode::ManifestInvalid,
                Some(CART_MANIFEST_FILE.to_owned()),
                message,
            );
            Ok(None)
        }
    }
}

fn check_identity(
    manifest: &CartManifest,
    expected: &ReleaseIdentity,
    report: &mut ValidationReport,
) {
    if manifest.id != expected.id {
        report.push(
            FindingCode::IdentityMismatch,
            Some(CART_MANIFEST_FILE.to_owned()),
            format!(
                "manifest id {} does not match the selected release {}",
                manifest.id, expected.id
            ),
        );
    }
    if manifest.version != expected.version {
        report.push(
            FindingCode::IdentityMismatch,
            Some(CART_MANIFEST_FILE.to_owned()),
            format!(
                "manifest version {} does not match the selected release {}",
                manifest.version, expected.version
            ),
        );
    }
}

/// Removes every file outside the manifest's include patterns (when any are
/// declared), then removes directories the pruning emptied. `cart.toml` is
/// always part of the release — it *is* the package's identity — so it never
/// needs listing. Returns the removed paths, sorted.
pub fn prune_to_release_surface(root: &Path, manifest: &CartManifest) -> io::Result<Vec<String>> {
    let patterns: Vec<IncludePattern> = manifest.include_patterns();
    if patterns.is_empty() {
        return Ok(Vec::new());
    }

    let mut removed = Vec::new();
    for relative in fingerprint::collect_relative_file_paths(root)? {
        let keep = relative == CART_MANIFEST_FILE
            || patterns.iter().any(|pattern| pattern.matches(&relative));
        if !keep {
            std::fs::remove_file(root.join(&relative))?;
            removed.push(relative);
        }
    }
    remove_empty_directories(root)?;
    removed.sort();
    Ok(removed)
}

/// Depth-first removal of directories the include-filter emptied.
fn remove_empty_directories(root: &Path) -> io::Result<()> {
    for entry in std::fs::read_dir(root)? {
        let path = entry?.path();
        if std::fs::symlink_metadata(&path)?.is_dir() {
            remove_empty_directories(&path)?;
            if std::fs::read_dir(&path)?.next().is_none() {
                std::fs::remove_dir(&path)?;
            }
        }
    }
    Ok(())
}

/// Validates the (already pruned) release surface. Returns the DIZ text when
/// present and within bounds.
fn validate_surface(
    root: &Path,
    policy: &PolicySet,
    manifest: Option<&CartManifest>,
    report: &mut ValidationReport,
) -> io::Result<Option<String>> {
    let mut files = fingerprint::collect_relative_file_paths(root)?;
    files.sort();

    let mut has_c_source = false;
    let mut has_licence_file = false;
    let mut reaches_network = false;
    let mut reaches_clipboard = false;
    // The newest-generation API name any source uses, with the file that uses it;
    // files are walked sorted and only a strictly newer name replaces it, so the
    // evidence named is the same on every machine.
    let mut newest_use: Option<(ApiUse, String)> = None;

    for relative in &files {
        let file_name = relative.rsplit('/').next().unwrap_or(relative);
        if !file_name_is_allowed(file_name) {
            report.push(
                FindingCode::FileNotAllowed,
                Some(relative.clone()),
                "file name is outside the release allowlist (docs/cart_exchange/cart_exchange.md §7.3)",
            );
        }
        if is_root_licence_file(relative) {
            has_licence_file = true;
        }

        let is_c = file_name.ends_with(".c");
        let is_header = file_name.ends_with(".h");
        if !(is_c || is_header) {
            continue;
        }
        if is_c {
            has_c_source = true;
        }
        let source = match std::fs::read_to_string(root.join(relative)) {
            Ok(source) => source,
            Err(error) if error.kind() == io::ErrorKind::InvalidData => {
                report.push(
                    FindingCode::FileNotAllowed,
                    Some(relative.clone()),
                    "C source must be valid UTF-8",
                );
                continue;
            }
            Err(error) => return Err(error),
        };
        for rule in policy.scan_source(&source) {
            let remedy = policy.remedy(&rule.kind).unwrap_or_default();
            report.push(
                FindingCode::ForbiddenSymbol,
                Some(relative.clone()),
                format!("names forbidden symbol '{}'. {remedy}", rule.symbol),
            );
        }
        if let Some(found) = ApiGenerations::builtin().newest_use(&source) {
            if newest_use
                .as_ref()
                .is_none_or(|(held, _)| found.generation > held.generation)
            {
                newest_use = Some((found, relative.clone()));
            }
        }
        let includes = extract_include_names(&source);
        if includes
            .iter()
            .any(|name| NETWORK_HEADERS.contains(&name.as_str()))
        {
            reaches_network = true;
        }
        if includes
            .iter()
            .any(|name| CLIPBOARD_HEADERS.contains(&name.as_str()))
        {
            reaches_clipboard = true;
        }
    }

    if !has_c_source {
        report.push(
            FindingCode::NoSource,
            None,
            "the release contains no C source files",
        );
    }
    if !has_licence_file {
        report.push(
            FindingCode::LicenceFileMissing,
            None,
            "no LICENSE or COPYING file at the cart root (§6.4: unlicensed public code is not reusable)",
        );
    }

    if let Some(manifest) = manifest {
        check_capability(
            "network",
            manifest.capabilities.network,
            reaches_network,
            NETWORK_HEADERS,
            report,
        );
        check_capability(
            "clipboard",
            manifest.capabilities.clipboard,
            reaches_clipboard,
            CLIPBOARD_HEADERS,
            report,
        );
        if let Some((found, file)) = &newest_use {
            check_api_generation(manifest.jet64_api, found, file, report);
        }
    }

    read_diz(root, report)
}

/// E9's honesty check: a declared `jet64_api` is the minimum generation the cart
/// needs, so it can never be below the generation that brought a name the source
/// uses. (Above it is the author's verified claim, which no record can refute.)
fn check_api_generation(declared: u32, found: &ApiUse, file: &str, report: &mut ValidationReport) {
    if declared >= found.generation {
        return;
    }
    report.push(
        FindingCode::ApiGenerationUnderstated,
        Some(CART_MANIFEST_FILE.to_owned()),
        format!(
            "declares jet64_api = {declared}, but {file} uses {}, which API generation {} \
             brought: declare jet64_api = {} or higher",
            found.symbol, found.generation, found.generation
        ),
    );
}

/// Whether `relative` is a root-level licence file: `LICENSE`, `COPYING`, or a
/// dotted variant of either (`LICENSE.MIT`, `COPYING.txt`).
fn is_root_licence_file(relative: &str) -> bool {
    !relative.contains('/')
        && ["LICENSE", "COPYING"]
            .iter()
            .any(|name| relative == *name || relative.starts_with(&format!("{name}.")))
}

/// The E15/§6.5.7 honesty check, both directions, for a capability judged by one set of
/// headers: the INFO screen's ACCESS line is derived from the declaration, and a declaration
/// that disagrees with the include tree in either direction misinforms the person deciding
/// whether to GET.
fn check_capability(
    name: &str,
    declared: bool,
    reaches: bool,
    headers: &[&str],
    report: &mut ValidationReport,
) {
    if reaches && !declared {
        report.push(
            FindingCode::CapabilityMismatch,
            Some(CART_MANIFEST_FILE.to_owned()),
            format!(
                "declares {name} = false, but the source includes {}",
                headers.join(" / ")
            ),
        );
    }
    if !reaches && declared {
        report.push(
            FindingCode::CapabilityMismatch,
            Some(CART_MANIFEST_FILE.to_owned()),
            format!(
                "declares {name} = true, but no source includes {}",
                headers.join(" / ")
            ),
        );
    }
}

fn read_diz(root: &Path, report: &mut ValidationReport) -> io::Result<Option<String>> {
    let path = root.join(DIZ_FILE);
    if !path.is_file() {
        return Ok(None);
    }
    let bytes = std::fs::read(&path)?;
    let text = match String::from_utf8(bytes) {
        Ok(text) => text,
        Err(_) => {
            report.push(
                FindingCode::DizInvalid,
                Some(DIZ_FILE.to_owned()),
                "file_id.diz must be valid UTF-8",
            );
            return Ok(None);
        }
    };
    match validate_diz(&text) {
        Ok(()) => Ok(Some(text)),
        Err(message) => {
            report.push(FindingCode::DizInvalid, Some(DIZ_FILE.to_owned()), message);
            Ok(None)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api_generations::FIRST_GENERATION;
    use crate::manifest::CartId;
    use std::path::PathBuf;

    const FIXTURE_MANIFEST: &str = r#"schema = 1
id = "io.github.mira.space-hauler"
name = "Space Hauler"
version = "1.2.0"
jet64_api = 1
license = "MIT"

[package]
include = ["**/*.c", "**/*.h", "data/**", "LICENSE", "README.md", "file_id.diz"]
"#;

    fn scratch_cart(label: &str) -> PathBuf {
        let root =
            std::env::temp_dir().join(format!("jcx-validate-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("data")).unwrap();
        std::fs::write(root.join(CART_MANIFEST_FILE), FIXTURE_MANIFEST).unwrap();
        std::fs::write(
            root.join("main.c"),
            "#include \"jet64.h\"\nvoid _init(void) {}\n",
        )
        .unwrap();
        std::fs::write(root.join("LICENSE"), "MIT\n").unwrap();
        std::fs::write(root.join(DIZ_FILE), "SPACE HAULER -- orbital delivery.\n").unwrap();
        std::fs::write(root.join("data/level.map"), b"\x01").unwrap();
        root
    }

    fn expected() -> ReleaseIdentity {
        ReleaseIdentity {
            id: CartId::parse("io.github.mira.space-hauler").unwrap(),
            version: semver::Version::new(1, 2, 0),
        }
    }

    #[test]
    fn a_well_formed_cart_is_accepted_and_fingerprinted() {
        let root = scratch_cart("accepts");
        let outcome = accept_tree(&root, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome.report.is_clean(), "{}", outcome.report);
        assert!(outcome.fingerprint.is_some());
        assert_eq!(
            outcome.diz.as_deref(),
            Some("SPACE HAULER -- orbital delivery.\n")
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn pruning_narrows_the_surface_and_the_fingerprint_follows() {
        let root = scratch_cart("prunes");
        std::fs::create_dir_all(root.join(".github/workflows")).unwrap();
        std::fs::write(root.join(".github/workflows/ci.yml"), "on: push\n").unwrap();
        std::fs::write(root.join(".gitignore"), "target\n").unwrap();

        let outcome = accept_tree(&root, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome.report.is_clean(), "{}", outcome.report);
        assert_eq!(
            outcome.pruned,
            vec![".github/workflows/ci.yml", ".gitignore"]
        );
        assert!(
            !root.join(".github").exists(),
            "emptied directories are removed"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn identity_mismatch_is_a_registry_inconsistency() {
        let root = scratch_cart("identity");
        let wrong = ReleaseIdentity {
            id: CartId::parse("io.github.other.cart").unwrap(),
            version: semver::Version::new(9, 9, 9),
        };
        let outcome = accept_tree(&root, PolicySet::builtin(), Some(&wrong)).unwrap();
        let identity_findings = outcome
            .report
            .findings()
            .iter()
            .filter(|finding| finding.code == FindingCode::IdentityMismatch)
            .count();
        assert_eq!(identity_findings, 2, "id and version each reported");
        assert!(
            outcome.fingerprint.is_none(),
            "no identity for a rejected tree"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn forbidden_symbols_and_disallowed_files_are_findings() {
        let root = scratch_cart("forbidden");
        std::fs::write(
            root.join("evil.c"),
            "void *p; void f(void) { p = malloc(4); }\n",
        )
        .unwrap();
        std::fs::write(root.join("data/tool.exe"), b"MZ").unwrap();
        let outcome = accept_tree(&root, PolicySet::builtin(), Some(&expected())).unwrap();
        let codes: Vec<_> = outcome
            .report
            .findings()
            .iter()
            .map(|finding| finding.code)
            .collect();
        assert!(codes.contains(&FindingCode::ForbiddenSymbol));
        assert!(codes.contains(&FindingCode::FileNotAllowed));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn network_declarations_are_checked_both_ways() {
        let dialling = scratch_cart("net-undeclared");
        std::fs::write(
            dialling.join("net.c"),
            "#include \"jet_http.h\"\nvoid poll_scores(void) {}\n",
        )
        .unwrap();
        let outcome = accept_tree(&dialling, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::CapabilityMismatch));
        std::fs::remove_dir_all(&dialling).unwrap();

        // Serving is network reach too — the gap the scoreboard cart exposed:
        // a listening cart must not be able to present itself as offline.
        let serving = scratch_cart("net-serving");
        std::fs::write(
            serving.join("api.c"),
            "#include \"jet_http_serve.h\"\nvoid route(void) {}\n",
        )
        .unwrap();
        let outcome = accept_tree(&serving, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::CapabilityMismatch));
        std::fs::remove_dir_all(&serving).unwrap();

        let overdeclared = scratch_cart("net-overdeclared");
        let manifest = FIXTURE_MANIFEST.to_owned() + "\n[capabilities]\nnetwork = true\n";
        std::fs::write(overdeclared.join(CART_MANIFEST_FILE), manifest).unwrap();
        let outcome = accept_tree(&overdeclared, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::CapabilityMismatch));
        std::fs::remove_dir_all(&overdeclared).unwrap();
    }

    #[test]
    fn a_declared_api_generation_below_a_used_name_is_understated() {
        let record = ApiGenerations::builtin();
        let newest = record.newest();
        let newest_name = record.generations().last().unwrap().symbols[0].clone();
        let understated = |label: &str, declared: u32, source: &str| {
            let cart = scratch_cart(label);
            let manifest =
                FIXTURE_MANIFEST.replace("jet64_api = 1", &format!("jet64_api = {declared}"));
            std::fs::write(cart.join(CART_MANIFEST_FILE), manifest).unwrap();
            std::fs::write(cart.join("feature.c"), source).unwrap();
            let outcome = accept_tree(&cart, PolicySet::builtin(), Some(&expected())).unwrap();
            std::fs::remove_dir_all(&cart).unwrap();
            outcome
                .report
                .findings()
                .iter()
                .find(|finding| finding.code == FindingCode::ApiGenerationUnderstated)
                .map(|finding| finding.message.clone())
        };

        let uses_newest = format!("void f(void) {{ (void){newest_name}; jet_cls(0); }}\n");
        let message = understated("api-low", newest - 1, &uses_newest)
            .expect("a name newer than the declaration is a finding");
        assert!(message.contains(&newest_name) && message.contains("feature.c"));
        assert!(message.contains(&format!("jet64_api = {newest} or higher")));

        assert_eq!(understated("api-exact", newest, &uses_newest), None);
        assert_eq!(understated("api-above", newest + 1, &uses_newest), None);
        // Mentioning the name is not using it.
        let mentions = format!("// {newest_name}\nvoid f(void) {{ jet_cls(0); }}\n");
        assert_eq!(
            understated("api-mention", FIRST_GENERATION, &mentions),
            None
        );
    }

    #[test]
    fn clipboard_declarations_are_checked_both_ways() {
        let mismatched = |outcome: &TreeValidation| {
            outcome
                .report
                .findings()
                .iter()
                .any(|finding| finding.code == FindingCode::CapabilityMismatch)
        };
        let copying = scratch_cart("clip-undeclared");
        std::fs::write(
            copying.join("paste.c"),
            "#include \"jet_clipboard.h\"\nvoid paste(void) {}\n",
        )
        .unwrap();
        let outcome = accept_tree(&copying, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(mismatched(&outcome), "an undeclared clipboard");
        let manifest = FIXTURE_MANIFEST.to_owned() + "\n[capabilities]\nclipboard = true\n";
        std::fs::write(copying.join(CART_MANIFEST_FILE), manifest).unwrap();
        let outcome = accept_tree(&copying, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(!mismatched(&outcome), "a declared one");
        std::fs::remove_dir_all(&copying).unwrap();

        let overdeclared = scratch_cart("clip-overdeclared");
        let manifest = FIXTURE_MANIFEST.to_owned() + "\n[capabilities]\nclipboard = true\n";
        std::fs::write(overdeclared.join(CART_MANIFEST_FILE), manifest).unwrap();
        let outcome = accept_tree(&overdeclared, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(
            mismatched(&outcome),
            "a clipboard claimed and never reached"
        );
        std::fs::remove_dir_all(&overdeclared).unwrap();
    }

    #[test]
    fn missing_manifest_licence_and_source_are_each_named() {
        let root = std::env::temp_dir().join(format!("jcx-validate-bare-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        std::fs::write(root.join("notes.txt"), "not a cart\n").unwrap();

        let outcome = accept_tree(&root, PolicySet::builtin(), None).unwrap();
        let codes: Vec<_> = outcome
            .report
            .findings()
            .iter()
            .map(|finding| finding.code)
            .collect();
        assert!(codes.contains(&FindingCode::ManifestMissing));
        assert!(codes.contains(&FindingCode::LicenceFileMissing));
        assert!(codes.contains(&FindingCode::NoSource));
        std::fs::remove_dir_all(&root).unwrap();
    }

    /// §12 P6.2 at the acceptance seam: a valid screenshot is attested and then
    /// pruned out of the surface; a malformed one is a named finding, never a
    /// silent shrug — and either way the picture can affect nothing but itself.
    #[test]
    fn screenshots_are_attested_pre_prune_or_refused_by_name() {
        use crate::screenshot::SCREENSHOT_FILE;
        use crate::testkit::png_header;

        let root = scratch_cart("shot-ok");
        std::fs::write(root.join(SCREENSHOT_FILE), png_header(640, 400)).unwrap();
        let outcome = accept_tree(&root, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome.report.is_clean(), "{}", outcome.report);
        let facts = outcome.screenshot.expect("attested");
        assert_eq!((facts.width, facts.height), (640, 400));
        assert!(
            outcome.pruned.iter().any(|path| path == SCREENSHOT_FILE),
            "the picture never joins the release surface"
        );
        std::fs::remove_dir_all(&root).unwrap();

        let root = scratch_cart("shot-bad");
        std::fs::write(root.join(SCREENSHOT_FILE), b"GIF89a not a png").unwrap();
        let outcome = accept_tree(&root, PolicySet::builtin(), Some(&expected())).unwrap();
        assert!(outcome.screenshot.is_none());
        assert!(outcome
            .report
            .findings()
            .iter()
            .any(|finding| finding.code == FindingCode::ScreenshotInvalid
                && finding.path.as_deref() == Some(SCREENSHOT_FILE)));
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn reports_are_deterministic_across_runs() {
        let build = |label: &str| {
            let root = scratch_cart(label);
            std::fs::write(root.join("evil.c"), "int f(void) { return fseek; }\n").unwrap();
            std::fs::write(root.join("worse.c"), "void g(void) { system(\"rm\"); }\n").unwrap();
            let outcome = accept_tree(&root, PolicySet::builtin(), Some(&expected())).unwrap();
            std::fs::remove_dir_all(&root).unwrap();
            outcome.report.to_string()
        };
        assert_eq!(build("det-a"), build("det-b"));
    }
}
