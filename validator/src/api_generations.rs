// validator/src/api_generations.rs
//! Which API generation brought each `jet_` name — the record that makes a cart's
//! declared `jet64_api` a checkable claim rather than a hope (E9).
//!
//! ## What a generation guarantees
//!
//! `JET64_API_GENERATION` is a single monotone integer, and a Workstation reporting
//! generation *N* may have been built from any commit between the one that moved the
//! number to *N* and the one that moved it on. A name is therefore **of generation N**
//! when the commit that set the number to *N* already declared it, and no earlier such
//! commit did: that is the first generation every build of which can compile a cart
//! naming it. A name that landed between two bumps belongs to the later one, because
//! the earlier generation's first builds lacked it.
//!
//! ## One record, one home
//!
//! [`API_GENERATIONS_TOML`] is generated in the closed repository, by a test that
//! re-derives every name's generation from the git history of those bumps and
//! the runtime headers each one shipped, and fails the build on any drift — so the
//! record can neither be hand-edited into a convenient answer nor fall behind a name
//! the runtime gains. The same test holds the record's newest generation equal to the
//! machine's `JET64_API_GENERATION`: surface added without a bump is a red build that
//! names the bump to make.
//!
//! The record covers every name the API Codex documents in a published header, enum
//! constants included. Its answer is a **floor**: a cart that names a generation-6
//! symbol cannot run on generation 5, but a cart may need more than its names show
//! (a behaviour, an asset format), which is why an author declares the generation they
//! verified against and this record only refuses a declaration that is provably low.

use std::collections::HashMap;
use std::fmt::Write as _;
use std::sync::OnceLock;

use serde::Deserialize;

use crate::policy::strip_c_comments_and_strings;

/// The generated record, embedded verbatim. Registry CI ships the same file at the
/// repository root; the closed repo's history test keeps it in lock-step with the
/// runtime and with `JET64_API_GENERATION`.
pub const API_GENERATIONS_TOML: &str = include_str!("../../api_generations.toml");

/// The record's schema. Grows only by a new number.
pub const API_GENERATIONS_SCHEMA: u32 = 1;

/// The generation the contract began at: the first `jet64_api` there ever was.
pub const FIRST_GENERATION: u32 = 1;

/// The banner every rendered record opens with — part of the canonical text, so the
/// closed repo's equality test compares the whole file, comments included.
const RECORD_BANNER: &str = "\
# api_generations.toml
# GENERATED from the Jet64 runtime's own history. Do not edit by hand: the closed
# repository's history test re-derives it from the commits that moved
# JET64_API_GENERATION (jet64_core/src/lib.rs) and the runtime headers each of them
# shipped, and fails the build on any drift. A name belongs to the first generation
# whose bump commit already declared it; a cart's jet64_api must be at least the
# highest generation among the names its source uses. Regenerate by running that
# test and pasting its expected output.
";

/// One generation and the names it brought, sorted.
#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Generation {
    pub number: u32,
    pub symbols: Vec<String>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct RecordFile {
    schema: u32,
    #[serde(default)]
    generation: Vec<Generation>,
}

/// The newest-generation name a source uses: the evidence behind a cart's floor.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ApiUse {
    pub symbol: String,
    pub generation: u32,
}

/// The parsed record: generations in order, and every name's generation by name.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ApiGenerations {
    generations: Vec<Generation>,
    since: HashMap<String, u32>,
}

impl ApiGenerations {
    /// Parses a record document, holding it to the shape [`Self::from_generations`]
    /// demands.
    pub fn parse(source: &str) -> Result<Self, String> {
        let file: RecordFile = toml::from_str(source).map_err(|error| error.to_string())?;
        if file.schema != API_GENERATIONS_SCHEMA {
            return Err(format!(
                "unsupported API-generation record schema {} (this validator accepts schema \
                 {API_GENERATIONS_SCHEMA})",
                file.schema
            ));
        }
        Self::from_generations(file.generation)
    }

    /// Builds a record from its generations, which must be numbered from
    /// [`FIRST_GENERATION`] without a gap, each holding its names sorted, and no name
    /// in two generations — the canonical form, so one set of facts has one text.
    pub fn from_generations(generations: Vec<Generation>) -> Result<Self, String> {
        if generations.is_empty() {
            return Err("the record names no generation".to_owned());
        }
        let mut since = HashMap::new();
        for (expected, generation) in (FIRST_GENERATION..).zip(&generations) {
            if generation.number != expected {
                return Err(format!(
                    "generation {} is out of place: generations count from \
                     {FIRST_GENERATION} without a gap, so this one must be {expected}",
                    generation.number
                ));
            }
            if let Some(pair) = generation
                .symbols
                .windows(2)
                .find(|pair| pair[0] >= pair[1])
            {
                return Err(format!(
                    "generation {expected} lists {:?} before {:?}: names are sorted and listed once",
                    pair[0], pair[1]
                ));
            }
            for symbol in &generation.symbols {
                if let Some(earlier) = since.insert(symbol.clone(), expected) {
                    return Err(format!(
                        "{symbol:?} is listed in generation {earlier} and again in {expected}"
                    ));
                }
            }
        }
        Ok(Self { generations, since })
    }

    /// The embedded [`API_GENERATIONS_TOML`], parsed once. The embedded file is held
    /// valid by this crate's own tests and the closed repo's history test, so the panic
    /// path is a build-time impossibility, not a runtime hazard.
    pub fn builtin() -> &'static ApiGenerations {
        static BUILTIN: OnceLock<ApiGenerations> = OnceLock::new();
        BUILTIN.get_or_init(|| {
            ApiGenerations::parse(API_GENERATIONS_TOML)
                .expect("embedded api_generations.toml is valid by construction")
        })
    }

    /// Every generation, oldest first.
    pub fn generations(&self) -> &[Generation] {
        &self.generations
    }

    /// The newest generation the record knows.
    pub fn newest(&self) -> u32 {
        self.generations
            .last()
            .map_or(FIRST_GENERATION, |generation| generation.number)
    }

    /// The generation that brought `symbol`, or `None` for a name that is not the API's.
    pub fn since(&self, symbol: &str) -> Option<u32> {
        self.since.get(symbol).copied()
    }

    /// The newest-generation API name that raw C `source` uses, ties broken by the
    /// name's spelling so the answer never depends on where in the file it sits.
    /// Comments and literals are stripped first, by the sandbox's own text model, so a
    /// name that is only mentioned is never counted as used.
    pub fn newest_use(&self, source: &str) -> Option<ApiUse> {
        let clean = strip_c_comments_and_strings(source);
        c_identifiers(&clean)
            .filter_map(|word| {
                self.since(word).map(|generation| ApiUse {
                    symbol: word.to_owned(),
                    generation,
                })
            })
            .max_by(|a, b| {
                a.generation
                    .cmp(&b.generation)
                    .then_with(|| b.symbol.cmp(&a.symbol))
            })
    }

    /// The canonical text of this record — what [`API_GENERATIONS_TOML`] must be,
    /// byte for byte.
    pub fn render(&self) -> String {
        let mut text = String::from(RECORD_BANNER);
        let _ = writeln!(text, "schema = {API_GENERATIONS_SCHEMA}");
        for generation in &self.generations {
            let _ = writeln!(text, "\n[[generation]]\nnumber = {}", generation.number);
            if generation.symbols.is_empty() {
                text.push_str("symbols = []\n");
                continue;
            }
            text.push_str("symbols = [\n");
            for symbol in &generation.symbols {
                let _ = writeln!(text, "    \"{symbol}\",");
            }
            text.push_str("]\n");
        }
        text
    }
}

/// Every C identifier in already-stripped source, in order: a maximal run of ASCII
/// letters, digits and `_` that does not start with a digit (which would make it part
/// of a number such as `0x1Fu`).
pub fn c_identifiers(clean_source: &str) -> impl Iterator<Item = &str> {
    clean_source
        .split(|ch: char| !(ch.is_ascii_alphanumeric() || ch == '_'))
        .filter(|word| {
            word.chars()
                .next()
                .is_some_and(|first| !first.is_ascii_digit())
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn record(generations: &[&[&str]]) -> Result<ApiGenerations, String> {
        ApiGenerations::from_generations(
            (FIRST_GENERATION..)
                .zip(generations)
                .map(|(number, symbols)| Generation {
                    number,
                    symbols: symbols.iter().map(|symbol| symbol.to_string()).collect(),
                })
                .collect(),
        )
    }

    #[test]
    fn the_embedded_record_parses_and_renders_back_to_itself() {
        let embedded = ApiGenerations::builtin();
        assert!(embedded.generations().len() > 1);
        assert_eq!(embedded.since("jet_cls"), Some(FIRST_GENERATION));
        assert_eq!(embedded.render(), API_GENERATIONS_TOML);
    }

    #[test]
    fn a_record_out_of_canonical_form_is_refused() {
        assert!(record(&[&["jet_a", "jet_b"], &["jet_c"]]).is_ok());
        assert!(record(&[&["jet_b", "jet_a"]]).is_err(), "unsorted");
        assert!(record(&[&["jet_a"], &["jet_a"]]).is_err(), "listed twice");
        assert!(record(&[]).is_err(), "empty");
        let gap = ApiGenerations::from_generations(vec![Generation {
            number: FIRST_GENERATION + 1,
            symbols: Vec::new(),
        }]);
        assert!(gap.is_err(), "numbered with a gap");
    }

    #[test]
    fn the_newest_use_reads_code_and_never_comments_or_literals() {
        let record = record(&[&["jet_old"], &[], &["jet_new", "jet_newer"]]).unwrap();
        let used = |source: &str| record.newest_use(source);

        assert_eq!(used("int main(void) { return 0; }"), None);
        assert_eq!(
            used("jet_old(); jet_new();\n"),
            Some(ApiUse {
                symbol: "jet_new".to_owned(),
                generation: 3
            })
        );
        // A tie is settled by spelling, wherever the names sit.
        assert_eq!(
            used("jet_newer(); jet_new();").map(|found| found.symbol),
            Some("jet_new".to_owned())
        );
        // Mentioned, not used.
        assert_eq!(
            used("// jet_new()\n/* jet_new */ puts(\"jet_new\"); jet_old();")
                .map(|found| found.generation),
            Some(FIRST_GENERATION)
        );
        // Whole identifiers only.
        assert_eq!(used("my_jet_new(); jet_new_ish();"), None);
    }

    #[test]
    fn identifiers_skip_numbers_and_split_on_punctuation() {
        let words: Vec<&str> = c_identifiers("x=0x1Fu+_y2*1e5f;JET_KEY_A").collect();
        assert_eq!(words, ["x", "_y2", "JET_KEY_A"]);
    }
}
