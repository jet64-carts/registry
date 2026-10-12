// validator/src/remote.rs
//! The network half of registry CI (feature `cli` only): fetch a pinned release
//! archive and prove it against its registry record — §6.5.1 (digests), §6.5.5–.8
//! (hygiene, policy, capabilities, DIZ), and the E20 tree fingerprint.
//!
//! The library proper stays network-free by construction; this module exists so
//! the `jcx` binary and any future registry tooling share one fetch-and-witness
//! implementation instead of two drifting ones. Every function takes explicit
//! paths and returns findings — no globals, no ambient state.

use std::io::Read;
use std::path::Path;

use crate::archive::COMMUNITY_ARCHIVE_POLICY;
use crate::manifest::ReleaseIdentity;
use crate::prepare::{accept_archive, Acceptance};
use crate::registry::ReleaseCheck;
use crate::report::{FindingCode, ValidationReport};

/// Downloads `url` to `destination`, refusing anything past the community
/// archive cap. One HTTPS GET, no redirect surprises beyond ureq's defaults —
/// the URL is always a canonical forge archive or an audited mirror.
pub fn download(url: &str, destination: &Path) -> Result<(), String> {
    let response = ureq::get(url)
        .call()
        .map_err(|error| format!("download failed: {error}"))?;
    let mut reader = response
        .into_reader()
        // One byte past the cap turns "at the limit" into a detectable overrun.
        .take(COMMUNITY_ARCHIVE_POLICY.archive_max_bytes + 1);
    let mut file = std::fs::File::create(destination)
        .map_err(|error| format!("cannot create {}: {error}", destination.display()))?;
    let copied = std::io::copy(&mut reader, &mut file)
        .map_err(|error| format!("download interrupted: {error}"))?;
    if copied > COMMUNITY_ARCHIVE_POLICY.archive_max_bytes {
        return Err(format!(
            "archive exceeds the {}-byte community limit",
            COMMUNITY_ARCHIVE_POLICY.archive_max_bytes
        ));
    }
    Ok(())
}

/// Fetches one release from the [`ReleaseCheck`] work-list and witnesses every
/// record it carries. Findings land in the returned report; `keep_trees`, when
/// given, receives the accepted release surface as `<id>-<version>/` so the CI
/// workflow can run the §6.5.9 per-unit syntax check over it.
pub fn witness_release(
    check: &ReleaseCheck,
    scratch: &Path,
    keep_trees: Option<&Path>,
) -> Result<ValidationReport, String> {
    let mut report = ValidationReport::new();
    let coordinate = format!("{} {}", check.id, check.release.version);
    let location = || Some(format!("carts/{}.toml", check.id));

    let archive = scratch.join(format!("{coordinate}.tar.gz").replace(' ', "-"));
    download(&check.release.source, &archive)?;

    let expected = ReleaseIdentity {
        id: check.id.clone(),
        version: check.release.version.clone(),
    };
    let acceptance = accept_archive(&archive, scratch, Some(&expected))?;

    if acceptance.sha256 != check.release.sha256 {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: archive sha256 is {} but the entry pins {} — either the \
                 submission is wrong, or the forge recompressed its archives (§7.5); a \
                 re-pin PR is acceptable only because the tree fingerprint below cannot move",
                acceptance.sha256, check.release.sha256
            ),
        );
    }
    if acceptance.archive_bytes != check.release.archive_bytes {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: archive is {} bytes, entry says {}",
                acceptance.archive_bytes, check.release.archive_bytes
            ),
        );
    }
    if acceptance.expanded_bytes != check.release.expanded_bytes {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: archive expands to {} bytes, entry says {}",
                acceptance.expanded_bytes, check.release.expanded_bytes
            ),
        );
    }

    report.absorb(acceptance.validation.report.clone());

    if let Some(fingerprint) = &acceptance.validation.fingerprint {
        let rendered = fingerprint.to_string();
        if rendered != check.release.tree_sha256 {
            report.push(
                FindingCode::ReleaseMismatch,
                location(),
                format!(
                    "{coordinate}: tree fingerprint is {rendered} but the entry pins {} — \
                     the content is not the reviewed content (E20)",
                    check.release.tree_sha256
                ),
            );
        }
    }

    // §12 P6.2: the entry's screenshot record and the archive's screenshot.png
    // must agree exactly — both absent, or byte-identical attested facts. The
    // board fetches the picture lazily against this digest, so a wrong record
    // is a wrong trust anchor, not a cosmetic slip.
    if acceptance.validation.screenshot != check.release.screenshot {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: the entry's screenshot record does not match the archive's \
                 screenshot.png (entry: {}, archive: {})",
                describe_screenshot(check.release.screenshot.as_ref()),
                describe_screenshot(acceptance.validation.screenshot.as_ref()),
            ),
        );
    }

    witness_manifest_facts(check, &acceptance, &coordinate, &mut report);

    if let Some(keep_root) = keep_trees {
        let destination = keep_root.join(format!("{}-{}", check.id, check.release.version));
        std::fs::create_dir_all(keep_root)
            .and_then(|()| {
                let _ = std::fs::remove_dir_all(&destination);
                std::fs::rename(&acceptance.surface, &destination)
            })
            .map_err(|error| format!("cannot keep release surface: {error}"))?;
    }

    report.finalize();
    Ok(report)
}

/// One screenshot record as a mismatch message names it.
fn describe_screenshot(facts: Option<&crate::screenshot::ScreenshotFacts>) -> String {
    match facts {
        None => "none".to_owned(),
        Some(facts) => format!(
            "{}×{}, {} bytes, sha256 {}…",
            facts.width,
            facts.height,
            facts.bytes,
            &facts.sha256[..facts.sha256.len().min(12)]
        ),
    }
}

/// The record-vs-manifest witnesses: per-release facts always, entry-level facts
/// (`name`/`license`/`diz`) only for the latest release, whose manifest is the
/// one the entry quotes (§6.3).
fn witness_manifest_facts(
    check: &ReleaseCheck,
    acceptance: &Acceptance,
    coordinate: &str,
    report: &mut ValidationReport,
) {
    let location = || Some(format!("carts/{}.toml", check.id));
    let Some(manifest) = &acceptance.validation.manifest else {
        return; // Its absence is already a finding from the acceptance run.
    };
    if manifest.jet64_api != check.release.jet64_api {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: manifest requires jet64_api {} but the entry says {}",
                manifest.jet64_api, check.release.jet64_api
            ),
        );
    }
    if manifest.capabilities.network != check.release.network {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: manifest declares network = {} but the entry says {}",
                manifest.capabilities.network, check.release.network
            ),
        );
    }
    if !check.is_latest {
        return;
    }
    if manifest.name != check.name {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: manifest names the cart {:?} but the entry says {:?}",
                manifest.name, check.name
            ),
        );
    }
    if manifest.license != check.license {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!(
                "{coordinate}: manifest licence is {:?} but the entry says {:?}",
                manifest.license, check.license
            ),
        );
    }
    // Trailing newlines are presentation, not authorship — everything else must
    // match byte-for-byte: the board displays the entry's copy verbatim (§3.4).
    let manifest_diz = acceptance.validation.diz.as_deref().map(str::trim_end);
    let entry_diz = check.diz.as_deref().map(str::trim_end);
    if manifest_diz != entry_diz {
        report.push(
            FindingCode::ReleaseMismatch,
            location(),
            format!("{coordinate}: the entry's diz does not match the release's file_id.diz"),
        );
    }
}
