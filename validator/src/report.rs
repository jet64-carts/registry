// validator/src/report.rs
//! Deterministic validation reporting.
//!
//! Registry CI posts these reports on pull requests and the Workstation prints them
//! in the Virtual Console, so the same tree must always produce the same lines in
//! the same order — a report is evidence, and evidence that shuffles is not
//! reviewable. Determinism comes from two rules: every producer walks its inputs in
//! sorted order, and [`ValidationReport::finalize`] sorts the findings themselves.

use std::fmt;

/// What a finding is about — the stable, machine-readable half of a report line.
///
/// Codes are part of the public registry contract: CI messages, PR review notes,
/// and Workstation errors all cite them, so variants are appended, never renamed.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum FindingCode {
    /// `cart.toml` is absent from the tree root.
    ManifestMissing,
    /// `cart.toml` exists but does not parse against schema v1.
    ManifestInvalid,
    /// The manifest's `id`/`version` disagree with the registry release selected.
    IdentityMismatch,
    /// A file's name is outside the release allowlist (extension or exact-name rules).
    FileNotAllowed,
    /// A `[package] include` pattern is malformed or escapes the cart root.
    IncludeInvalid,
    /// No licence file (`LICENSE`, `COPYING`, or a dotted variant) in the release.
    LicenceFileMissing,
    /// The release contains no C source at all — nothing for the console to run.
    NoSource,
    /// A source file names a symbol on the forbidden-policy list.
    ForbiddenSymbol,
    /// Declared capabilities disagree with the include tree (either direction).
    CapabilityMismatch,
    /// `file_id.diz` breaks its bounds (lines, columns, or character repertoire).
    DizInvalid,
    /// A registry file (entry, bulletin, boards, index) breaks a §6 rule.
    RegistryInvalid,
    /// An identity namespace's ownership is not proven (E19).
    NamespaceUnproven,
    /// A previously accepted `(id, version)` record changed or vanished (§6.5.11).
    ImmutabilityViolation,
    /// The committed `index-v1.json` is not what the entries regenerate (§6.5.10).
    IndexDrift,
    /// A fetched release disagrees with its registry record (§6.5.1/.5–.9).
    ReleaseMismatch,
    /// `screenshot.png` breaks its bounds — not a PNG, or over the byte or
    /// dimension caps (§12 P6.2).
    ScreenshotInvalid,
    /// The cart ticks Custom and its `data/shader/screen.glsl` is outside the portable
    /// subset. Found by the Workstation's installer, which carries the portable checker
    /// (docs/cart_shader.md §5.3, law 3); this crate names the finding so every report
    /// spells it the same way.
    ShaderRefused,
    /// `cart.toml` declares a `jet64_api` below the generation that brought a name
    /// the source uses (E9) — a minimum the record of generations proves false.
    ApiGenerationUnderstated,
}

impl FindingCode {
    /// The stable kebab-case spelling used in rendered reports.
    pub const fn as_str(self) -> &'static str {
        match self {
            FindingCode::ManifestMissing => "manifest-missing",
            FindingCode::ManifestInvalid => "manifest-invalid",
            FindingCode::IdentityMismatch => "identity-mismatch",
            FindingCode::FileNotAllowed => "file-not-allowed",
            FindingCode::IncludeInvalid => "include-invalid",
            FindingCode::LicenceFileMissing => "licence-file-missing",
            FindingCode::NoSource => "no-source",
            FindingCode::ForbiddenSymbol => "forbidden-symbol",
            FindingCode::CapabilityMismatch => "capability-mismatch",
            FindingCode::DizInvalid => "diz-invalid",
            FindingCode::RegistryInvalid => "registry-invalid",
            FindingCode::NamespaceUnproven => "namespace-unproven",
            FindingCode::ImmutabilityViolation => "immutability-violation",
            FindingCode::IndexDrift => "index-drift",
            FindingCode::ReleaseMismatch => "release-mismatch",
            FindingCode::ScreenshotInvalid => "screenshot-invalid",
            FindingCode::ShaderRefused => "shader-refused",
            FindingCode::ApiGenerationUnderstated => "api-generation-understated",
        }
    }
}

/// One validation failure: what rule broke, where, and the human sentence.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Finding {
    pub code: FindingCode,
    /// Forward-slashed path relative to the cart root, when the finding is about a
    /// particular file; `None` for tree-wide findings.
    pub path: Option<String>,
    pub message: String,
}

impl fmt::Display for Finding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.path {
            Some(path) => write!(
                f,
                "error[{}] {}: {}",
                self.code.as_str(),
                path,
                self.message
            ),
            None => write!(f, "error[{}] {}", self.code.as_str(), self.message),
        }
    }
}

/// Every finding for one validated tree, in a canonical order.
///
/// An empty report is the pass verdict; any finding is a rejection. There is no
/// warning tier in schema v1 — a rule either holds or the release is not accepted,
/// which keeps CI, the installer, and the author's own `jcx` run in exact agreement
/// about what "valid" means.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ValidationReport {
    findings: Vec<Finding>,
}

impl ValidationReport {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, code: FindingCode, path: Option<String>, message: impl Into<String>) {
        self.findings.push(Finding {
            code,
            path,
            message: message.into(),
        });
    }

    /// Sorts findings into the canonical order (code, then path, then message).
    /// Producers call this after the last finding is pushed; calling it again
    /// after an [`ValidationReport::absorb`] simply re-establishes the order.
    pub fn finalize(&mut self) {
        self.findings.sort();
    }

    /// Folds another report's findings into this one — how the offline
    /// verification and the per-release fetch checks become a single
    /// deterministic report. The caller finalizes once afterwards.
    pub fn absorb(&mut self, other: ValidationReport) {
        self.findings.extend(other.findings);
    }

    pub fn is_clean(&self) -> bool {
        self.findings.is_empty()
    }

    pub fn findings(&self) -> &[Finding] {
        &self.findings
    }
}

impl fmt::Display for ValidationReport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.findings.is_empty() {
            return writeln!(f, "ok: no findings");
        }
        for finding in &self.findings {
            writeln!(f, "{finding}")?;
        }
        writeln!(
            f,
            "{} finding{}",
            self.findings.len(),
            if self.findings.len() == 1 { "" } else { "s" }
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn finalize_orders_findings_canonically() {
        let mut report = ValidationReport::new();
        report.push(
            FindingCode::ForbiddenSymbol,
            Some("z.c".into()),
            "names 'free'",
        );
        report.push(
            FindingCode::FileNotAllowed,
            Some("tool.exe".into()),
            "extension",
        );
        report.push(
            FindingCode::ForbiddenSymbol,
            Some("a.c".into()),
            "names 'free'",
        );
        report.finalize();

        let order: Vec<_> = report
            .findings()
            .iter()
            .map(|finding| (finding.code, finding.path.clone()))
            .collect();
        assert_eq!(
            order,
            vec![
                (FindingCode::FileNotAllowed, Some("tool.exe".into())),
                (FindingCode::ForbiddenSymbol, Some("a.c".into())),
                (FindingCode::ForbiddenSymbol, Some("z.c".into())),
            ]
        );
    }

    #[test]
    fn identical_inputs_render_identically() {
        let build = || {
            let mut report = ValidationReport::new();
            report.push(
                FindingCode::DizInvalid,
                Some("file_id.diz".into()),
                "11 lines",
            );
            report.push(FindingCode::NoSource, None, "no .c files");
            report.finalize();
            report.to_string()
        };
        assert_eq!(build(), build());
    }
}
