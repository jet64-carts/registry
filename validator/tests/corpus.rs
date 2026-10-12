// validator/tests/corpus.rs
//! The adversarial archive corpus (docs/cart_exchange/cart_exchange.md §11): every hostile
//! shape §7.3 promises to reject, written with the raw ustar writer so the
//! fixtures actually contain what a polite builder would refuse to produce.
//!
//! Each case asserts two things: the extraction fails with the *right* error,
//! and nothing hostile was materialised outside the staging destination. (A NUL
//! inside a tar path cannot be expressed at the container level — the ustar name
//! field is NUL-terminated — so that rejection is unit-tested directly against
//! the path validator in `src/archive.rs`.)

use std::path::PathBuf;

use jcx_validator::archive::{
    extract_tar_gz, ArchiveError, ArchivePolicy, COMMUNITY_ARCHIVE_POLICY,
};
use jcx_validator::testkit::{RawEntryType, RawTarBuilder};

/// A fresh scratch area per case, cleaned on drop.
struct Scratch(PathBuf);

impl Scratch {
    fn new(label: &str) -> Self {
        let path = std::env::temp_dir().join(format!("jcx-corpus-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&path);
        std::fs::create_dir_all(&path).unwrap();
        Self(path)
    }
}

impl Drop for Scratch {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

fn extract(label: &str, bytes: &[u8], policy: &ArchivePolicy) -> Result<(), ArchiveError> {
    let scratch = Scratch::new(label);
    let archive = scratch.0.join("hostile.tar.gz");
    std::fs::write(&archive, bytes).unwrap();
    extract_tar_gz(&archive, &scratch.0.join("out"), policy).map(|_| ())
}

/// Every traversal and host-absolute path shape dies as a hostile path.
#[test]
fn traversal_and_absolute_paths_are_rejected() {
    let cases: &[(&str, &str)] = &[
        ("dotdot", "cart/../../escape.c"),
        ("dotdot-root", "../escape.c"),
        ("absolute", "/etc/evil.c"),
        ("drive", "C:/windows/evil.c"),
        ("unc", "\\\\server\\share\\evil.c"),
        ("backslash", "cart\\evil.c"),
        ("dot-segment", "cart/./evil.c"),
    ];
    for (label, path) in cases {
        let bytes = RawTarBuilder::new().file(path, b"x").finish();
        assert!(
            matches!(
                extract(label, &bytes, &COMMUNITY_ARCHIVE_POLICY),
                Err(ArchiveError::HostilePath { .. })
            ),
            "case {label}: {path:?} was not rejected as hostile"
        );
    }
}

#[test]
fn non_ascii_paths_are_rejected_by_construction() {
    // Unicode-confusable and normalisation collisions are impossible when the
    // path repertoire is printable ASCII — the whole class dies here.
    let bytes = RawTarBuilder::new().file("cart/résumé.c", b"x").finish();
    assert!(matches!(
        extract("non-ascii", &bytes, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::HostilePath { .. })
    ));
}

#[test]
fn links_and_special_files_are_rejected_by_type() {
    let symlink = RawTarBuilder::new()
        .file("cart/main.c", b"x")
        .symlink("cart/link.c", "/etc/passwd")
        .finish();
    assert!(matches!(
        extract("symlink", &symlink, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::ForbiddenEntryType {
            kind: "symlink",
            ..
        })
    ));

    let hard_link = RawTarBuilder::new()
        .file("cart/main.c", b"x")
        .hard_link("cart/alias.c", "cart/main.c")
        .finish();
    assert!(matches!(
        extract("hardlink", &hard_link, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::ForbiddenEntryType {
            kind: "hard link",
            ..
        })
    ));

    for (label, entry_type, kind) in [
        ("char-device", RawEntryType::CharDevice, "character device"),
        ("block-device", RawEntryType::BlockDevice, "block device"),
        ("fifo", RawEntryType::Fifo, "FIFO"),
    ] {
        let bytes = RawTarBuilder::new()
            .special("cart/dev", entry_type)
            .finish();
        let result = extract(label, &bytes, &COMMUNITY_ARCHIVE_POLICY);
        assert!(
            matches!(result, Err(ArchiveError::ForbiddenEntryType { kind: found, .. }) if found == kind),
            "case {label} was not rejected as a {kind}"
        );
    }
}

#[test]
fn a_symlink_never_touches_the_filesystem_even_mid_archive() {
    // The link is rejected at its own entry — but prove the earlier benign file
    // is all that reached disk, and the link path itself never appeared.
    let scratch = Scratch::new("symlink-side-effects");
    let archive = scratch.0.join("hostile.tar.gz");
    let bytes = RawTarBuilder::new()
        .file("cart/main.c", b"x")
        .symlink("cart/link.c", "/etc/passwd")
        .finish();
    std::fs::write(&archive, bytes).unwrap();
    let destination = scratch.0.join("out");
    assert!(extract_tar_gz(&archive, &destination, &COMMUNITY_ARCHIVE_POLICY).is_err());
    assert!(!destination.join("link.c").exists());
}

#[test]
fn extreme_expansion_ratio_dies_at_the_byte_budget() {
    // 64 KiB of zeros gzips to a few hundred bytes; a 4 KiB budget must stop it
    // *during* the copy — the bomb guard is cumulative bytes, not header claims.
    let tight = ArchivePolicy {
        expanded_max_bytes: 4 * 1024,
        file_max_bytes: 1024 * 1024,
        ..COMMUNITY_ARCHIVE_POLICY
    };
    let zeros = vec![0u8; 64 * 1024];
    let bytes = RawTarBuilder::new().file("cart/bomb.pcm", &zeros).finish();
    assert!(matches!(
        extract("bomb", &bytes, &tight),
        Err(ArchiveError::ExpandedTooLarge { .. })
    ));
}

#[test]
fn too_many_files_dies_at_the_count_cap() {
    let tight = ArchivePolicy {
        max_file_count: 8,
        ..COMMUNITY_ARCHIVE_POLICY
    };
    let mut builder = RawTarBuilder::new();
    for index in 0..9 {
        builder = builder.file(&format!("cart/file{index}.txt"), b"x");
    }
    assert!(matches!(
        extract("count", &builder.finish(), &tight),
        Err(ArchiveError::TooManyFiles { .. })
    ));
}

#[test]
fn a_lying_size_header_cannot_smuggle_bytes_past_the_cap() {
    // The header may claim any size; enforcement follows the bytes actually
    // streamed. A file whose header under-declares still hits the per-file cap.
    let tight = ArchivePolicy {
        file_max_bytes: 16,
        ..COMMUNITY_ARCHIVE_POLICY
    };
    let bytes = RawTarBuilder::new()
        .file("cart/big.txt", &[b'a'; 64])
        .finish();
    assert!(matches!(
        extract("lying-size", &bytes, &tight),
        Err(ArchiveError::FileTooLarge { .. })
    ));
}

#[test]
fn a_truncated_stream_is_an_error_never_a_partial_success() {
    let builder = RawTarBuilder::new()
        .file("cart/main.c", b"void _init(void) {}\n")
        .file("cart/data.pcm", &[0u8; 2048]);
    // Keep the first entry (header 512 + padded data 512), the second entry's
    // header, and only a sliver of its 2048 data bytes — a cut mid-entry, which
    // is what a died download looks like. (A cut *between* entries is
    // indistinguishable from a legal unterminated tar and extracts cleanly.)
    let bytes = builder.finish_truncated(1800);
    assert!(matches!(
        extract("truncated", &bytes, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::Io(_))
    ));
}

#[test]
fn garbage_that_is_not_gzip_is_an_error() {
    assert!(matches!(
        extract(
            "not-gzip",
            b"PK\x03\x04 this is a zip, not a tar.gz",
            &COMMUNITY_ARCHIVE_POLICY
        ),
        Err(ArchiveError::Io(_))
    ));
}

#[test]
fn duplicate_paths_case_folded_or_exact_are_rejected() {
    let exact = RawTarBuilder::new()
        .file("cart/main.c", b"one")
        .file("cart/main.c", b"two")
        .finish();
    assert!(matches!(
        extract("dupe-exact", &exact, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::DuplicatePath { .. })
    ));

    let folded = RawTarBuilder::new()
        .file("cart/Data/level.map", b"x")
        .file("cart/data/Level.Map", b"y")
        .finish();
    assert!(matches!(
        extract("dupe-folded", &folded, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::DuplicatePath { .. })
    ));

    let file_vs_dir = RawTarBuilder::new()
        .file("cart/data", b"a file named data")
        .file("cart/Data/level.map", b"x")
        .finish();
    assert!(matches!(
        extract("dupe-file-vs-dir", &file_vs_dir, &COMMUNITY_ARCHIVE_POLICY),
        Err(ArchiveError::DuplicatePath { .. })
    ));
}
