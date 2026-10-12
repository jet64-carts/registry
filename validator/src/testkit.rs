// validator/src/testkit.rs
//! Corpus-authoring toolkit: a raw ustar writer that can express archives no
//! well-behaved builder will produce — traversal paths, absolute paths, links,
//! device nodes, executable bits, lying size fields.
//!
//! The adversarial corpus (§11) is the Exchange's real quality floor, and a
//! corpus you cannot *write* is a corpus you cannot run. High-level tar builders
//! rightly refuse hostile shapes, so this module writes 512-byte ustar headers by
//! hand. It lives in the library (not behind a test gate) because three suites
//! share it: this crate's own corpus, the closed workstation's transaction tests,
//! and — come P1 — the public registry's CI self-tests.
//!
//! It is a *writer of fixtures*, never part of any validation path.

use std::io::Write;

use flate2::write::GzEncoder;
use flate2::Compression;

/// Ustar header geometry (POSIX.1-1988). Offsets are the format, not choices.
const HEADER_BYTES: usize = 512;
const NAME_FIELD: usize = 100;
const LINKNAME_FIELD: usize = 100;
const PREFIX_FIELD: usize = 155;

/// A tar entry type byte, named for corpus readability.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RawEntryType {
    Regular,
    HardLink,
    Symlink,
    CharDevice,
    BlockDevice,
    Directory,
    Fifo,
}

impl RawEntryType {
    fn type_byte(self) -> u8 {
        match self {
            RawEntryType::Regular => b'0',
            RawEntryType::HardLink => b'1',
            RawEntryType::Symlink => b'2',
            RawEntryType::CharDevice => b'3',
            RawEntryType::BlockDevice => b'4',
            RawEntryType::Directory => b'5',
            RawEntryType::Fifo => b'6',
        }
    }
}

/// Builds a `.tar.gz` from raw entries, without judgement.
pub struct RawTarBuilder {
    tar_bytes: Vec<u8>,
}

impl Default for RawTarBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl RawTarBuilder {
    pub fn new() -> Self {
        Self {
            tar_bytes: Vec::new(),
        }
    }

    /// A plain file with mode 0644 — the shape every benign fixture uses.
    pub fn file(self, path: &str, contents: &[u8]) -> Self {
        self.entry(path, RawEntryType::Regular, 0o644, contents, "")
    }

    /// A plain file with an explicit mode (e.g. `0o755` for the exec-bit case).
    pub fn file_with_mode(self, path: &str, mode: u32, contents: &[u8]) -> Self {
        self.entry(path, RawEntryType::Regular, mode, contents, "")
    }

    pub fn directory(self, path: &str) -> Self {
        self.entry(path, RawEntryType::Directory, 0o755, &[], "")
    }

    pub fn symlink(self, path: &str, target: &str) -> Self {
        self.entry(path, RawEntryType::Symlink, 0o777, &[], target)
    }

    pub fn hard_link(self, path: &str, target: &str) -> Self {
        self.entry(path, RawEntryType::HardLink, 0o644, &[], target)
    }

    pub fn special(self, path: &str, entry_type: RawEntryType) -> Self {
        self.entry(path, entry_type, 0o644, &[], "")
    }

    /// The general form: any path, any type, any mode, any link target.
    pub fn entry(
        mut self,
        path: &str,
        entry_type: RawEntryType,
        mode: u32,
        contents: &[u8],
        link_target: &str,
    ) -> Self {
        let mut header = [0u8; HEADER_BYTES];
        let (name, prefix) = split_name(path);
        header[..name.len()].copy_from_slice(name.as_bytes());
        write_octal(&mut header[100..108], mode as u64, 8); // mode
        write_octal(&mut header[108..116], 0, 8); // uid
        write_octal(&mut header[116..124], 0, 8); // gid
        write_octal(&mut header[124..136], contents.len() as u64, 12); // size
        write_octal(&mut header[136..148], 0, 12); // mtime
        header[156] = entry_type.type_byte();
        let link = &link_target.as_bytes()[..link_target.len().min(LINKNAME_FIELD)];
        header[157..157 + link.len()].copy_from_slice(link);
        header[257..263].copy_from_slice(b"ustar\0"); // magic
        header[263..265].copy_from_slice(b"00"); // version
        header[329..337].copy_from_slice(b"0000000\0"); // devmajor
        header[337..345].copy_from_slice(b"0000000\0"); // devminor
        header[345..345 + prefix.len()].copy_from_slice(prefix.as_bytes());

        // Checksum: header bytes summed with the checksum field read as spaces.
        header[148..156].fill(b' ');
        let sum: u64 = header.iter().map(|&byte| byte as u64).sum();
        write_octal(&mut header[148..155], sum, 7);
        header[155] = b' ';

        self.tar_bytes.extend_from_slice(&header);
        self.tar_bytes.extend_from_slice(contents);
        let padding = (HEADER_BYTES - contents.len() % HEADER_BYTES) % HEADER_BYTES;
        self.tar_bytes.extend(std::iter::repeat_n(0u8, padding));
        self
    }

    /// Gzips the archive (two zero blocks close the tar stream first).
    pub fn finish(mut self) -> Vec<u8> {
        self.tar_bytes
            .extend(std::iter::repeat_n(0u8, HEADER_BYTES * 2));
        let mut encoder = GzEncoder::new(Vec::new(), Compression::default());
        encoder
            .write_all(&self.tar_bytes)
            .expect("writing to a Vec cannot fail");
        encoder
            .finish()
            .expect("finishing a Vec-backed encoder cannot fail")
    }

    /// Gzips a deliberately truncated archive: the stream ends mid-entry, with no
    /// closing blocks — the "download died" corpus case.
    pub fn finish_truncated(self, keep_bytes: usize) -> Vec<u8> {
        let keep = keep_bytes.min(self.tar_bytes.len());
        let mut encoder = GzEncoder::new(Vec::new(), Compression::default());
        encoder
            .write_all(&self.tar_bytes[..keep])
            .expect("writing to a Vec cannot fail");
        encoder
            .finish()
            .expect("finishing a Vec-backed encoder cannot fail")
    }
}

/// A minimal PNG header (signature + IHDR) with the given dimensions — enough
/// for [`crate::screenshot::inspect_png`], which never reads past it. Fixture
/// authoring only, like everything else in this module.
pub fn png_header(width: u32, height: u32) -> Vec<u8> {
    const PNG_SIGNATURE: [u8; 8] = [0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n'];
    const IHDR_DATA_BYTES: u32 = 13;
    let mut bytes = PNG_SIGNATURE.to_vec();
    bytes.extend_from_slice(&IHDR_DATA_BYTES.to_be_bytes());
    bytes.extend_from_slice(b"IHDR");
    bytes.extend_from_slice(&width.to_be_bytes());
    bytes.extend_from_slice(&height.to_be_bytes());
    bytes
}

/// Splits a path across the ustar `name`/`prefix` fields when it outgrows the
/// 100-byte name (panics only if a fixture path cannot be expressed at all —
/// a corpus-authoring error, not a runtime condition).
fn split_name(path: &str) -> (&str, &str) {
    if path.len() <= NAME_FIELD {
        return (path, "");
    }
    let split = path[..=PREFIX_FIELD.min(path.len() - 1)]
        .rfind('/')
        .expect("fixture path longer than 100 bytes needs a '/' to split at");
    let (prefix, name_with_slash) = path.split_at(split);
    let name = &name_with_slash[1..];
    assert!(
        name.len() <= NAME_FIELD && prefix.len() <= PREFIX_FIELD,
        "fixture path {path:?} does not fit ustar name+prefix"
    );
    (name, prefix)
}

/// Writes `value` as a NUL-terminated octal ASCII field of `width` bytes.
fn write_octal(field: &mut [u8], value: u64, width: usize) {
    let rendered = format!("{value:0>width$o}", width = width - 1);
    field[..width - 1].copy_from_slice(&rendered.as_bytes()[rendered.len() - (width - 1)..]);
    field[width - 1] = 0;
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::archive::{extract_tar_gz, COMMUNITY_ARCHIVE_POLICY};

    #[test]
    fn the_raw_writer_produces_archives_the_extractor_accepts() {
        // Round-trip sanity: if the writer's headers were malformed, every corpus
        // rejection would be vacuous — the fixtures must be *valid tar* first.
        let bytes = RawTarBuilder::new()
            .directory("cart/")
            .file("cart/main.c", b"void _init(void) {}\n")
            .file("cart/data/level.map", &[1, 2, 3])
            .finish();
        let root = std::env::temp_dir().join(format!("jcx-testkit-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let archive = root.join("fixture.tar.gz");
        std::fs::write(&archive, bytes).unwrap();

        let tree = extract_tar_gz(&archive, &root.join("out"), &COMMUNITY_ARCHIVE_POLICY).unwrap();
        assert_eq!(tree.file_count, 2);
        assert_eq!(
            std::fs::read(tree.root.join("data/level.map")).unwrap(),
            vec![1, 2, 3]
        );
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn long_paths_split_across_name_and_prefix() {
        let long_dir = "d".repeat(90);
        let path = format!("cart/{long_dir}/{}.txt", "f".repeat(60));
        let (name, prefix) = split_name(&path);
        assert!(name.len() <= NAME_FIELD);
        assert!(prefix.len() <= PREFIX_FIELD);
        assert_eq!(format!("{prefix}/{name}"), path);
    }
}
