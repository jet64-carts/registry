// validator/src/fingerprint.rs
//! The canonical tree fingerprint (docs/cart_exchange/cart_exchange.md E20, §9.1).
//!
//! One algorithm, two callers: registry CI computes a release's `tree_sha256` with
//! it, and the Workstation installer computes baseline fingerprints of installed
//! carts with it — so a fresh install's baseline *is* the release's `tree_sha256`,
//! and "unmodified" and "content-authentic" are the same number. This is what lets
//! an archive re-pin after a forge recompression event (§7.5) be provably harmless:
//! transport bytes may drift, the tree may not.
//!
//! ## Canonical form
//!
//! The fingerprint is the SHA-256 of a `sha256sum`-style manifest of the tree:
//! for every regular file, one line of
//!
//! ```text
//! <64 lowercase hex digits of the file's SHA-256>  <forward-slashed relative path>\n
//! ```
//!
//! with lines ordered by the byte order of the path. Only file paths and file
//! bytes participate — never mtimes, permissions, or directory entries — because
//! those are exactly the things filesystems refuse to agree on. The manifest form
//! is deliberately auditable: `find . -type f | sort | xargs sha256sum` gets a
//! human within arm's reach of reproducing it.

use std::fmt;
use std::fs::File;
use std::io::{self, Read};
use std::path::{Path, PathBuf};

use sha2::{Digest, Sha256};

/// Read-buffer size for hashing. 64 KiB keeps syscall count low without a large
/// resident cost per concurrent hash.
const HASH_BUFFER_BYTES: usize = 64 * 1024;

/// A computed tree fingerprint — 32 bytes, rendered as 64 lowercase hex digits.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct TreeFingerprint(pub [u8; 32]);

impl TreeFingerprint {
    /// Parses the 64-hex-digit form (as stored in registry entries and installed
    /// records). Case-insensitive on input; canonical output is lowercase.
    pub fn parse(hex: &str) -> Result<Self, String> {
        if hex.len() != 64 {
            return Err(format!(
                "fingerprint must be 64 hex digits, got {}",
                hex.len()
            ));
        }
        let mut bytes = [0u8; 32];
        for (index, chunk) in hex.as_bytes().chunks_exact(2).enumerate() {
            let pair = std::str::from_utf8(chunk).map_err(|_| "non-ASCII digit".to_owned())?;
            bytes[index] =
                u8::from_str_radix(pair, 16).map_err(|_| format!("invalid hex pair {pair:?}"))?;
        }
        Ok(Self(bytes))
    }
}

impl fmt::Display for TreeFingerprint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for byte in self.0 {
            write!(f, "{byte:02x}")?;
        }
        Ok(())
    }
}

impl fmt::Debug for TreeFingerprint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "TreeFingerprint({self})")
    }
}

/// The canonical form itself, materialised: every regular file's
/// `(relative path, sha256 hex)` pair, ordered by the byte order of the path —
/// exactly the lines [`fingerprint_tree`] hashes. The registry publishes only
/// the fingerprint; the Workstation's installer additionally stores the
/// manifest on each installed record so a later edit can be *itemised*
/// (which files changed), not merely judged (docs/cart_exchange/cart_exchange.md §9.4's
/// View-changes summary).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TreeManifest {
    entries: Vec<(String, String)>,
}

impl TreeManifest {
    /// The `(path, digest)` pairs, path-sorted.
    pub fn entries(&self) -> &[(String, String)] {
        &self.entries
    }

    /// The fingerprint of this manifest — byte-identical to what
    /// [`fingerprint_tree`] computes over the same tree, because this *is* the
    /// data that function hashes.
    pub fn fingerprint(&self) -> TreeFingerprint {
        let mut manifest_hash = Sha256::new();
        for (relative, file_hex) in &self.entries {
            manifest_hash.update(file_hex.as_bytes());
            manifest_hash.update(b"  ");
            manifest_hash.update(relative.as_bytes());
            manifest_hash.update(b"\n");
        }
        TreeFingerprint(manifest_hash.finalize().into())
    }
}

/// Computes the canonical manifest of the tree rooted at `root`.
///
/// Fails on anything that has no canonical form: a symlink (releases may not
/// contain them and an installed cart should not grow one), or a path that is not
/// UTF-8. Failing beats guessing — a manifest that silently skipped an entry
/// would vouch for a tree it did not read.
pub fn manifest_tree(root: &Path) -> io::Result<TreeManifest> {
    let mut paths = collect_relative_file_paths(root)?;
    paths.sort();

    let mut buffer = vec![0u8; HASH_BUFFER_BYTES];
    let mut entries = Vec::with_capacity(paths.len());
    for relative in paths {
        let file_hex = sha256_hex_of_file(&root.join(&relative), &mut buffer)?;
        entries.push((relative, file_hex));
    }
    Ok(TreeManifest { entries })
}

/// Computes the fingerprint of the tree rooted at `root` — the manifest's hash;
/// see [`manifest_tree`] for the failure contract.
pub fn fingerprint_tree(root: &Path) -> io::Result<TreeFingerprint> {
    Ok(manifest_tree(root)?.fingerprint())
}

/// Every regular file under `root`, as forward-slashed relative UTF-8 paths.
pub fn collect_relative_file_paths(root: &Path) -> io::Result<Vec<String>> {
    let mut paths = Vec::new();
    let mut stack: Vec<PathBuf> = vec![root.to_path_buf()];
    while let Some(directory) = stack.pop() {
        for entry in std::fs::read_dir(&directory)? {
            let entry = entry?;
            let path = entry.path();
            // symlink_metadata: a link to a directory must not be traversed, and a
            // link to a file must not be hashed as if it were its target.
            let metadata = std::fs::symlink_metadata(&path)?;
            if metadata.file_type().is_symlink() {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!(
                        "symlink at {} — trees with links have no canonical form",
                        path.display()
                    ),
                ));
            }
            if metadata.is_dir() {
                stack.push(path);
            } else if metadata.is_file() {
                let relative = path
                    .strip_prefix(root)
                    .expect("walk stays under root by construction");
                let utf8 = relative.to_str().ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::InvalidData,
                        format!("non-UTF-8 path {relative:?} has no canonical form"),
                    )
                })?;
                paths.push(utf8.replace('\\', "/"));
            }
        }
    }
    Ok(paths)
}

/// Streams one file through SHA-256, returning lowercase hex.
pub fn sha256_hex_of_file(path: &Path, buffer: &mut [u8]) -> io::Result<String> {
    let mut file = File::open(path)?;
    let mut hash = Sha256::new();
    loop {
        let read = file.read(buffer)?;
        if read == 0 {
            break;
        }
        hash.update(&buffer[..read]);
    }
    Ok(hex_lower(&hash.finalize()))
}

/// SHA-256 of in-memory bytes, lowercase hex — the sibling of
/// [`sha256_hex_of_file`] for content already held whole (screenshot bytes).
pub fn sha256_hex_of_bytes(bytes: &[u8]) -> String {
    let mut hash = Sha256::new();
    hash.update(bytes);
    hex_lower(&hash.finalize())
}

/// Lowercase hex of arbitrary bytes — the crate's one digest-rendering spelling.
pub fn hex_lower(bytes: &[u8]) -> String {
    let mut out = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        use fmt::Write;
        write!(out, "{byte:02x}").expect("writing hex to a String cannot fail");
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch_tree(label: &str) -> PathBuf {
        let root =
            std::env::temp_dir().join(format!("jcx-fingerprint-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        root
    }

    #[test]
    fn fingerprint_is_deterministic_and_content_sensitive() {
        let root = scratch_tree("determinism");
        std::fs::create_dir_all(root.join("data")).unwrap();
        std::fs::write(root.join("main.c"), b"void _init(void) {}\n").unwrap();
        std::fs::write(root.join("data/level.map"), b"\x01\x02").unwrap();

        let first = fingerprint_tree(&root).unwrap();
        let second = fingerprint_tree(&root).unwrap();
        assert_eq!(first, second, "same tree, same number");

        std::fs::write(root.join("data/level.map"), b"\x01\x03").unwrap();
        assert_ne!(
            first,
            fingerprint_tree(&root).unwrap(),
            "one byte changes the number"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn renaming_a_file_changes_the_fingerprint() {
        let root = scratch_tree("rename");
        std::fs::write(root.join("a.txt"), b"same bytes").unwrap();
        let before = fingerprint_tree(&root).unwrap();
        std::fs::rename(root.join("a.txt"), root.join("b.txt")).unwrap();
        assert_ne!(
            before,
            fingerprint_tree(&root).unwrap(),
            "paths are part of identity"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn empty_directories_do_not_participate() {
        let root = scratch_tree("empty-dirs");
        std::fs::write(root.join("main.c"), b"x").unwrap();
        let before = fingerprint_tree(&root).unwrap();
        std::fs::create_dir_all(root.join("assets/unused")).unwrap();
        assert_eq!(
            before,
            fingerprint_tree(&root).unwrap(),
            "only files carry content"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_manifest_is_the_fingerprints_own_data() {
        let root = scratch_tree("manifest");
        std::fs::create_dir_all(root.join("data")).unwrap();
        std::fs::write(root.join("main.c"), b"void _init(void) {}\n").unwrap();
        std::fs::write(root.join("data/level.map"), b"\x01\x02").unwrap();

        let manifest = manifest_tree(&root).unwrap();
        assert_eq!(
            manifest
                .entries()
                .iter()
                .map(|(path, _)| path.as_str())
                .collect::<Vec<_>>(),
            vec!["data/level.map", "main.c"],
            "entries are path-sorted (byte order)"
        );
        assert_eq!(
            manifest.fingerprint(),
            fingerprint_tree(&root).unwrap(),
            "one canonical form, two views"
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn hex_round_trips() {
        let root = scratch_tree("hex");
        std::fs::write(root.join("main.c"), b"x").unwrap();
        let fingerprint = fingerprint_tree(&root).unwrap();
        let rendered = fingerprint.to_string();
        assert_eq!(TreeFingerprint::parse(&rendered).unwrap(), fingerprint);
        assert!(TreeFingerprint::parse("abc").is_err());
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn a_symlink_fails_the_fingerprint() {
        let root = scratch_tree("symlink");
        std::fs::write(root.join("main.c"), b"x").unwrap();
        std::os::unix::fs::symlink(root.join("main.c"), root.join("link.c")).unwrap();
        assert!(fingerprint_tree(&root).is_err());
        std::fs::remove_dir_all(&root).unwrap();
    }
}
