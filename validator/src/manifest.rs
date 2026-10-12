// validator/src/manifest.rs
//! The `cart.toml` package manifest — schema v1 (docs/cart_exchange/cart_exchange.md §5.2) — plus
//! the [`CartId`] identity rules (§5.1) and the `file_id.diz` bounds (§3.4).
//!
//! The manifest is the *package's* identity, reviewable in a five-line diff. By
//! schema there is no `build`, `command`, `script`, `url`, `flags`, or hook field —
//! and the absence is enforced, not hoped for: unknown keys are a parse error
//! (`deny_unknown_fields`), so the field that would smuggle behaviour cannot even
//! be written down.

use std::fmt;

use serde::{Deserialize, Serialize};

/// Canonical manifest filename at the cart root.
pub const CART_MANIFEST_FILE: &str = "cart.toml";
/// Canonical author's-card filename at the cart root (§3.4).
pub const DIZ_FILE: &str = "file_id.diz";

/// The one manifest schema this crate accepts. Grows only by a new number.
pub const MANIFEST_SCHEMA: u32 = 1;

/// `file_id.diz` bounds — the genuine 1990s artefact, resurrected exactly: a card
/// this size lays out identically everywhere the board renders it.
pub const DIZ_MAX_LINES: usize = 10;
/// Maximum characters (Unicode scalars) per DIZ line.
pub const DIZ_MAX_COLS: usize = 45;

/// [`CartId`] length bounds in bytes (§5.1).
pub const CART_ID_MIN_BYTES: usize = 3;
pub const CART_ID_MAX_BYTES: usize = 128;

/// Display-name bound. Generous for a title, small enough that a listing row can
/// never be weaponised into a wall of text.
pub const NAME_MAX_BYTES: usize = 100;

/// Licences the registry admits (§6.4, ratified 2026-08-18). A manifest's `license`
/// is a single SPDX id from this list, or ids joined by ` OR ` (each allowlisted).
/// Extending the list is a registry-policy pull request, not a code fork.
///
/// The document's shorthand `LGPL-2.1`/`LGPL-3.0` is spelled here in current SPDX
/// (`-only`/`-or-later`): the bare forms are deprecated ids, and a new registry has
/// no legacy corpus to stay compatible with.
pub const SPDX_ALLOWLIST: &[&str] = &[
    "MIT",
    "Apache-2.0",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "Zlib",
    "MPL-2.0",
    "GPL-2.0-only",
    "GPL-2.0-or-later",
    "GPL-3.0-only",
    "GPL-3.0-or-later",
    "LGPL-2.1-only",
    "LGPL-2.1-or-later",
    "LGPL-3.0-only",
    "LGPL-3.0-or-later",
    "CC0-1.0",
    // Asset licences.
    "CC-BY-4.0",
    "CC-BY-SA-4.0",
];

/// A cart's permanent reverse-DNS-style identity (§5.1), e.g.
/// `io.github.mira.space-hauler`.
///
/// Display names are mutable and non-unique; this is neither. Rules: lowercase
/// ASCII letters, digits, dots and hyphens; [`CART_ID_MIN_BYTES`]–
/// [`CART_ID_MAX_BYTES`] bytes; at least two dot-separated segments (a namespace
/// and a name); every segment non-empty, starting and ending alphanumeric. An ID is
/// never recycled to a different project — ownership transfers are registry
/// history, never ID changes.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(try_from = "String", into = "String")]
pub struct CartId(String);

impl CartId {
    pub fn parse(raw: &str) -> Result<Self, String> {
        if raw.len() < CART_ID_MIN_BYTES || raw.len() > CART_ID_MAX_BYTES {
            return Err(format!(
                "cart id must be {CART_ID_MIN_BYTES}–{CART_ID_MAX_BYTES} bytes, got {}",
                raw.len()
            ));
        }
        if let Some(bad) = raw
            .chars()
            .find(|c| !(c.is_ascii_lowercase() || c.is_ascii_digit() || matches!(c, '.' | '-')))
        {
            return Err(format!(
                "cart id may use only lowercase ASCII letters, digits, dots and hyphens \
                 (found {bad:?})"
            ));
        }
        let segments: Vec<&str> = raw.split('.').collect();
        if segments.len() < 2 {
            return Err(
                "cart id needs at least two dot-separated segments (a namespace and a name)".into(),
            );
        }
        for segment in &segments {
            let valid = segment
                .chars()
                .next()
                .is_some_and(|c| c.is_ascii_alphanumeric())
                && segment
                    .chars()
                    .last()
                    .is_some_and(|c| c.is_ascii_alphanumeric());
            if !valid {
                return Err(format!(
                    "cart id segment {segment:?} must be non-empty and start/end alphanumeric"
                ));
            }
        }
        Ok(Self(raw.to_owned()))
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }

    /// The final segment — the project's own name, and the preferred local folder
    /// slug (§9.3; the folder is a convenience, never the identity).
    pub fn last_segment(&self) -> &str {
        self.0.rsplit('.').next().unwrap_or(&self.0)
    }
}

impl TryFrom<String> for CartId {
    type Error = String;
    fn try_from(raw: String) -> Result<Self, Self::Error> {
        Self::parse(&raw)
    }
}

impl From<CartId> for String {
    fn from(id: CartId) -> String {
        id.0
    }
}

impl fmt::Display for CartId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

/// The `(id, version)` coordinate a registry release pins. The installer and CI
/// both hand this to [`crate::validate::validate_tree`] so a mismatched upload —
/// archive says one thing, registry says another — fails as the inconsistency it
/// is (§7.3) rather than installing under a name nobody reviewed.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ReleaseIdentity {
    pub id: CartId,
    pub version: semver::Version,
}

/// `[package]` — narrows the release surface (§5.2). Paths are relative and may
/// not escape the root; files outside the include set are simply not part of the
/// release: never installed, never fingerprinted, invisible to the board.
#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct PackageSpec {
    /// Include patterns (see [`IncludePattern`]). Absent or empty means the whole
    /// tree is the release surface.
    #[serde(default)]
    pub include: Vec<String>,
}

/// `[capabilities]` — declarations that inform review and the INFO screen (§3.4).
/// They grant nothing: the compiler's forbidden scan and the runtime's per-project
/// network consent gate remain the enforcement, unchanged.
#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Capabilities {
    /// Whether the cart has network reach — dialling out (`jet_net.h` /
    /// `jet_http.h` / `jet_ws.h`), connecting straight to another player
    /// (`jet_rtc.h`), or answering (`jet_http_serve.h`).
    /// Verified against the include tree in both directions (§6.5.7).
    #[serde(default)]
    pub network: bool,
    /// Whether the cart can read and write the player's clipboard (`jet_clipboard.h`) — only
    /// ever on the player's own copy and paste keys, but a clipboard is where passwords sit, so
    /// the INFO screen says so. Verified against the include tree in both directions, as
    /// `network` is.
    #[serde(default)]
    pub clipboard: bool,
}

/// `cart.toml`, schema v1 — author-owned, at the cart root.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CartManifest {
    pub schema: u32,
    pub id: CartId,
    pub name: String,
    pub version: semver::Version,
    /// Minimum `JET64_API_GENERATION` the cart requires (E9) — a single monotone
    /// integer, because the hardware contract is frozen and grows additively.
    pub jet64_api: u32,
    /// SPDX expression from [`SPDX_ALLOWLIST`] (single id, or ids joined by ` OR `).
    pub license: String,
    #[serde(default)]
    pub package: PackageSpec,
    #[serde(default)]
    pub capabilities: Capabilities,
}

impl CartManifest {
    /// Parses and semantically validates a manifest. Serde owns shape (unknown keys,
    /// types, SemVer); this owns the rules a schema cannot express.
    pub fn parse(source: &str) -> Result<Self, String> {
        let manifest: CartManifest = toml::from_str(source).map_err(|error| error.to_string())?;
        if manifest.schema != MANIFEST_SCHEMA {
            return Err(format!(
                "unsupported manifest schema {} (this validator accepts schema {MANIFEST_SCHEMA})",
                manifest.schema
            ));
        }
        if manifest.name.trim().is_empty() {
            return Err("name must not be empty".into());
        }
        if manifest.name.len() > NAME_MAX_BYTES {
            return Err(format!("name exceeds {NAME_MAX_BYTES} bytes"));
        }
        if manifest.jet64_api == 0 {
            return Err("jet64_api must be at least 1".into());
        }
        validate_licence_expression(&manifest.license)?;
        for pattern in &manifest.package.include {
            IncludePattern::parse(pattern)?;
        }
        Ok(manifest)
    }

    /// The include patterns, parsed. Only callable on a manifest that already
    /// passed [`CartManifest::parse`], so the unwrap-free rebuild cannot fail.
    pub fn include_patterns(&self) -> Vec<IncludePattern> {
        self.package
            .include
            .iter()
            .filter_map(|pattern| IncludePattern::parse(pattern).ok())
            .collect()
    }
}

/// Validates a licence expression: one allowlisted SPDX id, or several joined by
/// ` OR ` (a dual-licence offer). `AND`/`WITH` composites are out of schema-v1
/// scope — a registry-policy question, not a parser gap. Public because the
/// registry entry's `license` field answers to the same allowlist (§6.4).
pub fn validate_licence_expression(expression: &str) -> Result<(), String> {
    if expression.trim() != expression || expression.is_empty() {
        return Err("license must be a trimmed, non-empty SPDX expression".into());
    }
    for id in expression.split(" OR ") {
        if !SPDX_ALLOWLIST.contains(&id) {
            return Err(format!(
                "licence {id:?} is not on the registry allowlist \
                 (docs/cart_exchange/cart_exchange.md §6.4; extendable by registry PR)"
            ));
        }
    }
    Ok(())
}

/// One `[package] include` pattern: `/`-separated segments where `**` matches any
/// number of directories, `*` matches within one segment, and `?` matches one
/// character. Deliberately the smallest glob dialect that expresses the §5.2
/// example — a bounded matcher CI and the installer can agree on byte-for-byte.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct IncludePattern {
    segments: Vec<String>,
}

impl IncludePattern {
    pub fn parse(pattern: &str) -> Result<Self, String> {
        if pattern.is_empty() || pattern.len() > crate::archive::PATH_MAX_BYTES {
            return Err(format!(
                "include pattern must be 1–{} bytes",
                crate::archive::PATH_MAX_BYTES
            ));
        }
        if pattern.starts_with('/') || pattern.contains('\\') || pattern.contains('\0') {
            return Err(format!(
                "include pattern {pattern:?} must be relative, forward-slashed, and NUL-free"
            ));
        }
        let segments: Vec<String> = pattern.split('/').map(str::to_owned).collect();
        for segment in &segments {
            if segment.is_empty() || segment == "." || segment == ".." {
                return Err(format!(
                    "include pattern {pattern:?} may not contain empty, '.' or '..' segments"
                ));
            }
        }
        Ok(Self { segments })
    }

    /// Whether a forward-slashed relative path matches this pattern.
    pub fn matches(&self, relative_path: &str) -> bool {
        let path: Vec<&str> = relative_path.split('/').collect();
        Self::match_segments(&self.segments, &path)
    }

    fn match_segments(pattern: &[String], path: &[&str]) -> bool {
        match pattern.first() {
            None => path.is_empty(),
            Some(segment) if segment == "**" => {
                // `**` absorbs zero or more leading path segments.
                (0..=path.len()).any(|skip| Self::match_segments(&pattern[1..], &path[skip..]))
            }
            Some(segment) => match path.first() {
                Some(component) if segment_matches(segment, component) => {
                    Self::match_segments(&pattern[1..], &path[1..])
                }
                _ => false,
            },
        }
    }
}

/// Single-segment wildcard match: `*` any run, `?` one character, else literal.
fn segment_matches(pattern: &str, text: &str) -> bool {
    let pattern: Vec<char> = pattern.chars().collect();
    let text: Vec<char> = text.chars().collect();
    match_chars(&pattern, &text)
}

fn match_chars(pattern: &[char], text: &[char]) -> bool {
    match pattern.first() {
        None => text.is_empty(),
        Some('*') => (0..=text.len()).any(|skip| match_chars(&pattern[1..], &text[skip..])),
        Some('?') => !text.is_empty() && match_chars(&pattern[1..], &text[1..]),
        Some(literal) => text.first() == Some(literal) && match_chars(&pattern[1..], &text[1..]),
    }
}

/// Validates `file_id.diz` text against its bounds: at most [`DIZ_MAX_LINES`]
/// lines of at most [`DIZ_MAX_COLS`] characters, printable text only. CRLF is
/// accepted (authors write on every OS); all other control characters are not —
/// the card is displayed verbatim inside a period-correct frame, and a terminal
/// escape has no business in it.
pub fn validate_diz(text: &str) -> Result<(), String> {
    let lines: Vec<&str> = text.trim_end_matches(['\n', '\r']).split('\n').collect();
    if lines.len() > DIZ_MAX_LINES {
        return Err(format!(
            "file_id.diz has {} lines (maximum {DIZ_MAX_LINES})",
            lines.len()
        ));
    }
    for (index, raw_line) in lines.iter().enumerate() {
        let line = raw_line.strip_suffix('\r').unwrap_or(raw_line);
        let columns = line.chars().count();
        if columns > DIZ_MAX_COLS {
            return Err(format!(
                "file_id.diz line {} is {columns} columns (maximum {DIZ_MAX_COLS})",
                index + 1
            ));
        }
        if let Some(control) = line.chars().find(|c| c.is_control()) {
            return Err(format!(
                "file_id.diz line {} contains control character {control:?}",
                index + 1
            ));
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    const VALID_MANIFEST: &str = r#"
        schema = 1
        id = "io.github.mira.space-hauler"
        name = "Space Hauler"
        version = "1.2.0"
        jet64_api = 1
        license = "MIT"

        [package]
        include = ["*.c", "*.h", "data/**", "LICENSE", "README.md", "file_id.diz"]

        [capabilities]
        network = false
    "#;

    #[test]
    fn the_worked_example_parses() {
        let manifest = CartManifest::parse(VALID_MANIFEST).expect("spec example is valid");
        assert_eq!(manifest.id.as_str(), "io.github.mira.space-hauler");
        assert_eq!(manifest.id.last_segment(), "space-hauler");
        assert_eq!(manifest.version, semver::Version::new(1, 2, 0));
        assert!(!manifest.capabilities.network);
        assert!(
            !manifest.capabilities.clipboard,
            "absent means no clipboard"
        );
    }

    #[test]
    fn the_clipboard_is_a_capability_beside_the_network() {
        let declared =
            VALID_MANIFEST.replace("network = false", "network = false\nclipboard = true");
        let manifest = CartManifest::parse(&declared).expect("clipboard is a known capability");
        assert!(manifest.capabilities.clipboard);
        assert!(!manifest.capabilities.network);
    }

    #[test]
    fn unknown_keys_are_rejected_wherever_they_appear() {
        // The E1 guarantee: the field that would smuggle behaviour cannot be written.
        for hostile in [
            VALID_MANIFEST.replace("license = \"MIT\"", "license = \"MIT\"\nbuild = \"make\""),
            VALID_MANIFEST.replace("[package]", "[hooks]\npost_install = \"x\"\n[package]"),
            VALID_MANIFEST.replace("network = false", "network = false\nscript = \"run.sh\""),
        ] {
            assert!(
                CartManifest::parse(&hostile).is_err(),
                "accepted: {hostile}"
            );
        }
    }

    #[test]
    fn cart_id_rules_hold() {
        for valid in [
            "io.github.mira.space-hauler",
            "au.org.jet64.luminvasion",
            "a.b",
        ] {
            assert!(CartId::parse(valid).is_ok(), "rejected {valid}");
        }
        for invalid in [
            "ab",                   // below the byte floor
            "single-segment",       // no namespace
            "Io.Github.Mira.Cart",  // uppercase
            "io..mira",             // empty segment
            "io.github.-mira.cart", // segment starts with hyphen
            "io.github.mira.cart-", // segment ends with hyphen
            "io.github.mira.cart!", // charset
        ] {
            assert!(CartId::parse(invalid).is_err(), "accepted {invalid}");
        }
        let ceiling = format!("io.{}", "a".repeat(CART_ID_MAX_BYTES));
        assert!(CartId::parse(&ceiling).is_err());
    }

    #[test]
    fn licence_allowlist_is_enforced() {
        assert!(validate_licence_expression("MIT").is_ok());
        assert!(validate_licence_expression("MIT OR Apache-2.0").is_ok());
        assert!(validate_licence_expression("WTFPL").is_err());
        assert!(validate_licence_expression("MIT OR WTFPL").is_err());
        assert!(
            validate_licence_expression("LGPL-2.1").is_err(),
            "deprecated bare id"
        );
        assert!(validate_licence_expression(" MIT").is_err(), "untrimmed");
    }

    #[test]
    fn include_patterns_reject_escapes_and_match_the_dialect() {
        for invalid in ["", "/abs", "a\\b", "a/../b", "./a", "a//b"] {
            assert!(
                IncludePattern::parse(invalid).is_err(),
                "accepted {invalid:?}"
            );
        }
        let cases = [
            ("*.c", "main.c", true),
            ("*.c", "actors/dog.c", false),
            ("**/*.c", "actors/dog.c", true),
            ("**/*.c", "main.c", true),
            ("data/**", "data/palettes/default.pal", true),
            ("data/**", "docs/readme.md", false),
            ("LICENSE", "LICENSE", true),
            ("sprite?.gfx", "sprite1.gfx", true),
            ("sprite?.gfx", "sprite10.gfx", false),
        ];
        for (pattern, path, expected) in cases {
            let parsed = IncludePattern::parse(pattern).unwrap();
            assert_eq!(parsed.matches(path), expected, "{pattern} vs {path}");
        }
    }

    #[test]
    fn diz_bounds_are_exact() {
        let widest = "x".repeat(DIZ_MAX_COLS);
        let tallest = [widest.as_str(); DIZ_MAX_LINES].join("\n");
        assert!(validate_diz(&tallest).is_ok());
        assert!(
            validate_diz(&format!("{tallest}\n")).is_ok(),
            "trailing newline is not a line"
        );
        assert!(
            validate_diz("made in a kitchen\r\nin Ostrava").is_ok(),
            "CRLF welcome"
        );

        assert!(
            validate_diz(&format!("{widest}y")).is_err(),
            "one column over"
        );
        assert!(
            validate_diz(&format!("{tallest}\nover")).is_err(),
            "one line over"
        );
        assert!(
            validate_diz("bell \u{7} ring").is_err(),
            "control characters"
        );
        assert!(validate_diz("tab\there").is_err(), "tabs are layout lies");
    }
}
