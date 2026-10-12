// validator/src/policy.rs
//! The pre-install policy scan: which file names a release may contain, and which
//! C symbols its source may never name.
//!
//! ## One list, one home (E7)
//!
//! The forbidden-symbol list's authoritative home is the closed workstation's
//! compiler (`src/core/guest/compiler.rs`, `FORBIDDEN_C_FUNCTIONS`). This crate
//! consumes the generated [`POLICY_TOML`], and a test **in the closed repo** renders
//! that file from the compiler's table and asserts byte equality — drift between the
//! sandbox and the registry is a build failure, not a heisenbug. The scanner below
//! (comment/string stripping + whole-identifier matching) is ported from the same
//! compiler, once, and the closed repo's scan-agreement test runs both over a
//! deliberately tricky corpus.
//!
//! ## The file allowlist
//!
//! Finalised by the P0 census over the 40-odd carts in the Jet64 tree (2026-08-18):
//! C source, prose, manifests, and every Jet64 asset type in real use — including
//! the C64 flagship's ROM and disk images — plus the licence-notice naming
//! conventions `player_sealing.md` §9.5 already honours. Export artefacts the
//! census also surfaced (`.exe`, `.wasm`, `.zip`, `.html`, players, `.DS_Store`)
//! are deliberately absent: a release distributes source, never builds.

use std::sync::OnceLock;

use serde::Deserialize;

/// The generated forbidden-symbol policy, embedded verbatim. Registry CI ships the
/// same file at the repository root; the closed repo's equality test keeps both in
/// lock-step with the compiler.
pub const POLICY_TOML: &str = include_str!("../../policy.toml");

/// The policy file's schema. Grows only by a new number.
pub const POLICY_SCHEMA: u32 = 1;

/// Extensions a release file may carry (compared ASCII case-insensitively, because
/// filesystems disagree about case long before authors do). Everything here is
/// inert content to the host: the only things ever *executed* are C sources, and
/// those pass the compiler's own sandbox gates again after install.
pub const ALLOWED_EXTENSIONS: &[&str] = &[
    // Source and prose.
    "c", "h", "md", "txt", "toml", "ron", "diz", // Graphics, palettes, fonts.
    // Two extensions here are the one-hex-line sidecars of `core::digest_sidecar`, and a
    // release carrying baked art carries both. `palette` is *provenance*
    // (`core::palette_stamp`): it records which LUT a bake resolved its indices against,
    // and nothing can re-derive it once the art has left its workstation. `digest` is the
    // Font ROM's *cache key* (`core::font_rom`) — re-derivable in principle, but a release
    // that omits it bakes on first open and writes both files into the installed tree,
    // which the Exchange's own baseline then reads as the player's edits (§9.4). A cache
    // that travels with its key keeps an installed cart byte-identical to what it received.
    "gfx", "pal", "palette", "rom", "digest", "png", "ani", // Audio.
    "sfx", "pcm", "flxm", "mod", "xm", "s3m", "mml", "mid", "wav",
    // World and story data.
    "map", "jet", "fif", "fst", "cfg", // The C64 flagship's disk images.
    "d64",
    // A cart's own screen shader, `data/shader/screen.glsl` (docs/cart_shader.md). Text the host
    // never executes as it stands: every host passes it through the portable checker first, and
    // the Workstation's installer refuses a release whose shader the checker refuses.
    "glsl",
];

/// Exact names allowed with no extension at all.
pub const ALLOWED_EXACT_NAMES: &[&str] = &["LICENSE", "COPYING", "NOTICE"];

/// Whether `file_name` (no directory components) is admissible in a release.
///
/// Three ways in: an allowlisted extension; an exact licence name, dotted or not
/// (`LICENSE`, `LICENSE.MIT`); or the bundled-notice conventions `*-NOTICE.*` /
/// `*-COPYING.*` that the exporter's THIRD_PARTY_LICENSES collector already honours
/// — those carry arbitrary extensions (`OPENROMS-COPYING.LESSER`) by design.
pub fn file_name_is_allowed(file_name: &str) -> bool {
    if let Some((stem, extension)) = file_name.rsplit_once('.') {
        if ALLOWED_EXTENSIONS
            .iter()
            .any(|allowed| extension.eq_ignore_ascii_case(allowed))
        {
            return true;
        }
        if ALLOWED_EXACT_NAMES.contains(&stem) {
            return true; // e.g. LICENSE.MIT, COPYING.GPL, NOTICE.old
        }
        if stem.ends_with("-NOTICE") || stem.ends_with("-COPYING") {
            return true;
        }
    }
    ALLOWED_EXACT_NAMES.contains(&file_name)
}

/// One forbidden-symbol rule, as generated from the compiler's table.
#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct PolicyRule {
    pub symbol: String,
    /// Kebab-case key into the policy's `[kinds]` table.
    pub kind: String,
    /// Whether a match must look like a call (`identifier (`) rather than merely
    /// being the identifier — see the compiler's `requires_call_syntax` for why the
    /// socket vocabulary needs this and the allocation rows must not have it.
    pub requires_call: bool,
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
struct PolicyKind {
    /// The one-line remedy shown beneath a violation, shared verbatim with the
    /// workstation's Guru Meditation so the author reads the same advice wherever
    /// the release was refused.
    remedy: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
struct PolicyFile {
    schema: u32,
    #[serde(default)]
    kinds: std::collections::BTreeMap<String, PolicyKind>,
    #[serde(default)]
    forbidden: Vec<PolicyRule>,
}

/// The parsed forbidden-symbol policy.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PolicySet {
    kinds: std::collections::BTreeMap<String, PolicyKind>,
    rules: Vec<PolicyRule>,
}

impl PolicySet {
    /// Parses a policy document, verifying schema and that every rule's kind is
    /// declared — an undeclared kind would silently lose its remedy line.
    pub fn parse(source: &str) -> Result<Self, String> {
        let file: PolicyFile = toml::from_str(source).map_err(|error| error.to_string())?;
        if file.schema != POLICY_SCHEMA {
            return Err(format!(
                "unsupported policy schema {} (this validator accepts schema {POLICY_SCHEMA})",
                file.schema
            ));
        }
        for rule in &file.forbidden {
            if !file.kinds.contains_key(&rule.kind) {
                return Err(format!(
                    "rule {:?} names undeclared kind {:?}",
                    rule.symbol, rule.kind
                ));
            }
        }
        Ok(Self {
            kinds: file.kinds,
            rules: file.forbidden,
        })
    }

    /// The embedded [`POLICY_TOML`], parsed once. The embedded file is validated by
    /// this crate's own tests (and the closed repo's equality test), so the panic
    /// path is a build-time impossibility, not a runtime hazard.
    pub fn builtin() -> &'static PolicySet {
        static BUILTIN: OnceLock<PolicySet> = OnceLock::new();
        BUILTIN.get_or_init(|| {
            PolicySet::parse(POLICY_TOML).expect("embedded policy.toml is valid by construction")
        })
    }

    pub fn rules(&self) -> &[PolicyRule] {
        &self.rules
    }

    /// The remedy line for a rule's kind (declared for every rule by construction).
    pub fn remedy(&self, kind: &str) -> Option<&str> {
        self.kinds.get(kind).map(|entry| entry.remedy.as_str())
    }

    /// Every forbidden symbol `source` names — one report per symbol, in table
    /// order. `source` is raw C; stripping happens here so no caller can forget it.
    pub fn scan_source(&self, source: &str) -> Vec<&PolicyRule> {
        let clean = strip_c_comments_and_strings(source);
        self.rules
            .iter()
            .filter(|rule| names_whole_identifier(&clean, &rule.symbol, rule.requires_call))
            .collect()
    }
}

/// Reduces C source to the text the sandbox scan reads: each comment replaced by one
/// space (C translation phase 3), string and character literals removed — an unterminated
/// one ended at its physical line, which C forbids a literal from spanning — and newlines
/// from line comments kept so line structure survives. Ported verbatim from the closed
/// workstation's `strip_c_comments_and_strings` (`src/core/guest/compiler.rs`) — the
/// sandbox's own pre-scan text model, held byte-identical by the E7 test so the two
/// scanners read identical text.
pub fn strip_c_comments_and_strings(c: &str) -> String {
    let mut out = String::with_capacity(c.len());
    let mut chars = c.chars().peekable();
    let mut in_line = false;
    let mut in_block = false;
    let mut in_string = false;
    let mut in_char = false;
    let mut escape = false;

    while let Some(ch) = chars.next() {
        if ch == '\\' && !escape {
            if let Some(&next_ch) = chars.peek() {
                if next_ch == '\n' {
                    chars.next();
                    continue;
                } else if next_ch == '\r' {
                    chars.next();
                    if let Some(&'\n') = chars.peek() {
                        chars.next();
                    }
                    continue;
                }
            }
        }

        if in_line {
            if ch == '\n' {
                in_line = false;
                out.push('\n');
            }
        } else if in_block {
            if ch == '*' && chars.peek() == Some(&'/') {
                chars.next();
                in_block = false;
            }
        } else if in_string {
            if ch == '\n' {
                // A literal cannot span a physical line: C ends an unterminated string at
                // the newline (a splice would have continued it — handled above), so the
                // scan does too, or a stray `"` in a directive hides every following line
                // up to the next quote. Byte-for-byte the workstation lexer's `walk`; the
                // E7 corpus stresses the shape.
                in_string = false;
                escape = false;
                out.push('\n');
            } else if escape {
                escape = false;
            } else if ch == '\\' {
                escape = true;
            } else if ch == '"' {
                in_string = false;
            }
        } else if in_char {
            if ch == '\n' {
                // The `in_string` twin — closes the `#define NOTE don't mind me` shape.
                in_char = false;
                escape = false;
                out.push('\n');
            } else if escape {
                escape = false;
            } else if ch == '\\' {
                escape = true;
            } else if ch == '\'' {
                in_char = false;
            }
        } else if ch == '/' && chars.peek() == Some(&'/') {
            chars.next();
            in_line = true;
            // C phase 3: each comment becomes one space, so it separates its
            // neighbours rather than gluing them. Byte-identical to the closed
            // compiler's `strip_c_comments_and_strings` — the E7 test holds them so.
            out.push(' ');
        } else if ch == '/' && chars.peek() == Some(&'*') {
            chars.next();
            in_block = true;
            out.push(' ');
        } else if ch == '"' {
            in_string = true;
        } else if ch == '\'' {
            in_char = true;
        } else {
            out.push(ch);
        }
    }
    out
}

/// Whether `clean_source` (already stripped) names `symbol` as a whole C identifier
/// — the character on each side neither alphanumeric nor `_`, so `jet_pool_free`
/// and `player_send` pass while a bare `free` or `send(` trips. With
/// `requires_call`, the next non-whitespace character must additionally be `(`.
/// Ported verbatim from the compiler's `names_whole_identifier` (see
/// [`strip_c_comments_and_strings`] for the porting contract).
pub fn names_whole_identifier(clean_source: &str, symbol: &str, requires_call: bool) -> bool {
    for (idx, _) in clean_source.match_indices(symbol) {
        let before = clean_source[..idx].chars().last().unwrap_or(' ');
        let rest = &clean_source[idx + symbol.len()..];
        let after = rest.chars().next().unwrap_or(' ');

        let whole_identifier =
            !before.is_alphanumeric() && before != '_' && !after.is_alphanumeric() && after != '_';
        if !whole_identifier {
            continue;
        }
        if requires_call
            && !rest
                .trim_start_matches(char::is_whitespace)
                .starts_with('(')
        {
            continue;
        }
        return true;
    }
    false
}

/// Replaces each comment with one space while **keeping** string and character
/// literals — the text model for `#include` extraction, where the quoted form
/// `#include "jet_http.h"` *is* a string literal and must survive. Line
/// structure is preserved the same way as the full strip, and a comment must
/// still hide an include, which is why this cannot be a plain line scan.
pub fn strip_c_comments(c: &str) -> String {
    let mut out = String::with_capacity(c.len());
    let mut chars = c.chars().peekable();
    let mut in_string = false;
    let mut in_char = false;
    let mut escape = false;

    while let Some(ch) = chars.next() {
        if in_string || in_char {
            if escape {
                escape = false;
            } else if ch == '\\' {
                escape = true;
            } else if (in_string && ch == '"') || (in_char && ch == '\'') {
                in_string = false;
                in_char = false;
            }
            out.push(ch);
        } else if ch == '/' && chars.peek() == Some(&'/') {
            // C phase 3: a comment becomes one space, so `#in/**/clude` can never
            // read as a directive and a comment never glues two include tokens.
            out.push(' ');
            for skipped in chars.by_ref() {
                if skipped == '\n' {
                    out.push('\n');
                    break;
                }
            }
        } else if ch == '/' && chars.peek() == Some(&'*') {
            chars.next();
            out.push(' ');
            let mut previous = ' ';
            for skipped in chars.by_ref() {
                if previous == '*' && skipped == '/' {
                    break;
                }
                previous = skipped;
            }
        } else {
            if ch == '"' {
                in_string = true;
            } else if ch == '\'' {
                in_char = true;
            }
            out.push(ch);
        }
    }
    out
}

/// The networking headers whose reachability makes a cart's `network = false`
/// declaration a lie (§6.5.7) — the surfaces that dial out (`jet_net.h`,
/// `jet_http.h`, `jet_ws.h`), the one that connects straight to another player
/// (`jet_rtc.h`), *and* the one that answers (`jet_http_serve.h`): a cart that
/// opens a listening socket has network reach every bit as much as one that
/// fetches, and the INFO screen's ACCESS line must say so. The guest compiler
/// builds every `.c` under a project, so *any* include of one of these anywhere
/// in the tree is reachable. `jet_lockstep.h` is not among them: it carries no
/// packet anywhere, and a cart that sends its packets includes one of these.
pub const NETWORK_HEADERS: &[&str] = &[
    "jet_net.h",
    "jet_http.h",
    "jet_http_serve.h",
    "jet_ws.h",
    "jet_rtc.h",
];

/// The header whose inclusion gives a cart the player's clipboard
/// (docs/jet_text/design.md §2.2) — the `clipboard` capability's twin of
/// [`NETWORK_HEADERS`], judged by the same rule: any include anywhere in the tree.
pub const CLIPBOARD_HEADERS: &[&str] = &["jet_clipboard.h"];

/// Every `#include` target named by `source` (raw C; quoted and angled forms),
/// robust against indentation and `#  include` spacing. Used for the capability-
/// versus-include-tree verification; the closed compiler keeps its own richer
/// resolver — this only needs the *names*.
pub fn extract_include_names(source: &str) -> Vec<String> {
    let clean = strip_c_comments(source);
    let mut names = Vec::new();
    for line in clean.lines() {
        let line = line.trim_start();
        let Some(directive) = line.strip_prefix('#') else {
            continue;
        };
        let directive = directive.trim_start();
        let Some(target) = directive.strip_prefix("include") else {
            continue;
        };
        let target = target.trim_start();
        let (open, close) = match target.chars().next() {
            Some('"') => ('"', '"'),
            Some('<') => ('<', '>'),
            _ => continue,
        };
        if let Some(name) = target
            .strip_prefix(open)
            .and_then(|rest| rest.split(close).next())
        {
            names.push(name.to_owned());
        }
    }
    names
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_embedded_policy_parses_and_is_populated() {
        let policy = PolicySet::builtin();
        assert!(policy.rules().iter().any(|rule| rule.symbol == "malloc"));
        assert!(policy.rules().iter().any(|rule| rule.symbol == "socket"));
        for rule in policy.rules() {
            assert!(policy.remedy(&rule.kind).is_some(), "kind {:?}", rule.kind);
        }
    }

    #[test]
    fn the_scan_respects_identifier_boundaries_and_call_syntax() {
        let policy = PolicySet::builtin();
        let source = r#"
            // free the slot — a comment may say anything
            static const char *label = "malloc is forbidden"; /* so may a string */
            void jet_pool_free(void);      // prefixed identifiers are fine
            int player_send(int x);        // a cart's own vocabulary survives
            struct menu { int accept; };   // bare socket word, no call: fine
        "#;
        assert!(policy.scan_source(source).is_empty());

        let hostile = "void *p = malloc(64);";
        assert_eq!(policy.scan_source(hostile).len(), 1);

        let declaration = "int socket (int, int, int);";
        assert_eq!(
            policy.scan_source(declaration).len(),
            1,
            "declaration is a call shape"
        );

        let address_taken = "void (*f)(void*) = free;";
        assert_eq!(
            policy.scan_source(address_taken).len(),
            1,
            "bare rows match without a call"
        );

        // A comment is one space (C phase 3), so it can neither smuggle a forbidden
        // word past the matcher by gluing it to a neighbour nor glue two harmless
        // fragments into one. Held in lock-step with the closed compiler by E7.
        let glued_call = "void* f(void){ return/**/malloc(64); }";
        assert_eq!(
            policy.scan_source(glued_call).len(),
            1,
            "a comment must not hide malloc between tokens"
        );
        let glued_fragments = "int fr/**/ee = 0;";
        assert!(
            policy.scan_source(glued_fragments).is_empty(),
            "a comment must not glue fragments into a reserved word"
        );
    }

    #[test]
    fn an_unterminated_literal_does_not_hide_a_forbidden_symbol() {
        // The unterminated-literal bypass: a stray `'` or `"` in a directive once opened a
        // literal that ran to the next quote and swallowed the `malloc` below it, while clang
        // ends such a literal at its physical line and links the call. The port ends it there
        // too (held byte-identical to the workstation lexer by the E7 corpus).
        let policy = PolicySet::builtin();
        for source in [
            "#define NOTE don't mind me\nvoid *g(void){ return malloc(64); }\nchar t = 'x';\n",
            "#define MSG \"oops\nvoid *g(void){ return malloc(64); }\n",
            "#if 0\n  can't happen\n#endif\nvoid *g(void){ return malloc(64); }\n",
        ] {
            let found = policy.scan_source(source);
            assert_eq!(found.len(), 1, "{source}");
            assert_eq!(found[0].symbol, "malloc", "{source}");
        }

        // A splice continues a literal across a line (phase 2 precedes phase 3), so a forbidden
        // word inside a spliced literal stays hidden — only a bare newline ends a literal.
        assert!(policy
            .scan_source("const char *s = \"call \\\nmalloc now\";\n")
            .is_empty());
    }

    #[test]
    fn the_file_allowlist_admits_content_and_refuses_artefacts() {
        for allowed in [
            "main.c",
            "game.h",
            "README.md",
            "cart.toml",
            "file_id.diz",
            "sprites.gfx",
            "imports.ron.palette",
            "font.rom.digest",
            "song.flxm",
            "kernal.rom",
            "disk.d64",
            "Icon.PNG",
            "LICENSE",
            "COPYING",
            "LICENSE.MIT",
            "OPENROMS-COPYING.LESSER",
            "GEISS-NOTICE.md",
            "HACK103-NOTICE.txt",
        ] {
            assert!(file_name_is_allowed(allowed), "refused {allowed}");
        }
        for refused in [
            "player.exe",
            "cart.wasm",
            "bundle.zip",
            "index.html",
            "boot.js",
            ".DS_Store",
            ".gitignore",
            "build.sh",
            "Info.plist",
            "flux64_player",
        ] {
            assert!(!file_name_is_allowed(refused), "admitted {refused}");
        }
    }

    #[test]
    fn include_names_are_extracted_through_formatting_noise() {
        let source = r#"
            #include "jet64.h"
              #  include   "jet_http.h"
            // #include "jet_ws.h" — commented out, must not count
            #include <stdint.h>
        "#;
        assert_eq!(
            extract_include_names(source),
            vec!["jet64.h", "jet_http.h", "stdint.h"]
        );
    }
}
