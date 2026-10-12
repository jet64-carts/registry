// validator/src/archive.rs
//! Archive hygiene (docs/cart_exchange/cart_exchange.md §7.3, E18): streaming `.tar.gz`
//! extraction under hard caps, with every hostile shape rejected **during**
//! extraction — never discovered afterwards.
//!
//! The contract, exactly: no entry escapes the destination; no permission bit
//! survives; no link, device, or special file is ever created; and every cap is a
//! named constant enforced cumulatively while bytes stream, so a decompression
//! bomb dies at its byte budget, not at the filesystem's.
//!
//! On error the partially written destination is left in place — the caller owns
//! the staging area's lifecycle (delete, or park in quarantine when developer
//! diagnostics ask for it) and no partial tree is ever anywhere near `carts/`.
//!
//! ## Why release paths are printable ASCII
//!
//! Filesystems disagree about Unicode: case folding differs, normalisation
//! differs (HFS+ stores NFD), and confusable code points make two "identical"
//! names distinct bytes. Schema v1 sidesteps the whole class by construction:
//! entry paths are printable ASCII, and case-colliding duplicates are rejected.
//! The author's voice lives in `file_id.diz` and the DIZ is full UTF-8 — paths
//! are addresses, not prose. Relaxing this later is a registry-policy decision.

use std::collections::HashMap;
use std::fmt;
use std::fs::File;
use std::io::{self, BufReader, Read, Write};
use std::path::{Path, PathBuf};

use flate2::read::GzDecoder;

/// §7.3 caps — the community-archive policy, all named here.
pub const ARCHIVE_MAX_BYTES: u64 = 32 * 1024 * 1024;
pub const EXPANDED_MAX_BYTES: u64 = 96 * 1024 * 1024;
pub const MAX_FILE_COUNT: usize = 2048;
pub const FILE_MAX_BYTES: u64 = 16 * 1024 * 1024;
pub const PATH_MAX_BYTES: usize = 200;
pub const DIR_MAX_DEPTH: usize = 8;

/// Copy-buffer size for streaming an entry to disk.
const COPY_BUFFER_BYTES: usize = 64 * 1024;

/// The permission every extracted file lands with on Unix: read/write for the
/// owner, read for everyone else. Permissions are never preserved (§7.3).
#[cfg(unix)]
const EXTRACTED_FILE_MODE: u32 = 0o644;

/// The caps one extraction runs under. A struct rather than loose parameters so a
/// different caller (e.g. a future toolchain migration) can carry its own policy
/// without this module growing flags.
#[derive(Clone, Copy, Debug)]
pub struct ArchivePolicy {
    pub archive_max_bytes: u64,
    pub expanded_max_bytes: u64,
    pub max_file_count: usize,
    pub file_max_bytes: u64,
    pub path_max_bytes: usize,
    pub dir_max_depth: usize,
}

/// The policy every community release is judged under.
pub const COMMUNITY_ARCHIVE_POLICY: ArchivePolicy = ArchivePolicy {
    archive_max_bytes: ARCHIVE_MAX_BYTES,
    expanded_max_bytes: EXPANDED_MAX_BYTES,
    max_file_count: MAX_FILE_COUNT,
    file_max_bytes: FILE_MAX_BYTES,
    path_max_bytes: PATH_MAX_BYTES,
    dir_max_depth: DIR_MAX_DEPTH,
};

/// What a successful extraction produced.
#[derive(Debug)]
pub struct ExtractedTree {
    /// The destination directory — the release tree, top-level wrapper stripped.
    pub root: PathBuf,
    pub file_count: usize,
    pub expanded_bytes: u64,
    /// The single top-level directory name the archive wrapped its contents in
    /// (a forge's `repo-revision/`), recorded for diagnostics.
    pub top_level: String,
}

/// Everything that can refuse an archive. Every variant names the entry that
/// tripped it, because a rejection the author cannot locate is a rejection they
/// cannot fix.
#[derive(Debug)]
pub enum ArchiveError {
    Io(io::Error),
    ArchiveTooLarge {
        bytes: u64,
        limit: u64,
    },
    ExpandedTooLarge {
        limit: u64,
    },
    TooManyFiles {
        limit: usize,
    },
    FileTooLarge {
        path: String,
        limit: u64,
    },
    /// Absolute, traversing, non-ASCII, over-long, or otherwise hostile path.
    HostilePath {
        path: String,
        reason: &'static str,
    },
    /// Symlink, hard link, device, FIFO, socket, sparse — anything but a plain
    /// file or directory.
    ForbiddenEntryType {
        path: String,
        kind: &'static str,
    },
    ExecutableBits {
        path: String,
    },
    /// Two entries that collide once case is folded (or byte-identical twins).
    DuplicatePath {
        path: String,
    },
    /// More than one top-level directory, or a file beside the top-level one.
    NotSingleRooted {
        path: String,
    },
    /// The archive contains no entries at all.
    Empty,
    /// The destination already exists — staging areas are always fresh.
    DestinationExists(PathBuf),
}

impl fmt::Display for ArchiveError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ArchiveError::Io(error) => write!(f, "archive I/O error: {error}"),
            ArchiveError::ArchiveTooLarge { bytes, limit } => {
                write!(f, "archive is {bytes} bytes (limit {limit})")
            }
            ArchiveError::ExpandedTooLarge { limit } => {
                write!(f, "archive expands past the {limit}-byte budget")
            }
            ArchiveError::TooManyFiles { limit } => {
                write!(f, "archive contains more than {limit} files")
            }
            ArchiveError::FileTooLarge { path, limit } => {
                write!(f, "{path} exceeds the {limit}-byte per-file limit")
            }
            ArchiveError::HostilePath { path, reason } => {
                write!(f, "entry path {path:?} rejected: {reason}")
            }
            ArchiveError::ForbiddenEntryType { path, kind } => {
                write!(
                    f,
                    "entry {path:?} is a {kind} — only files and directories are allowed"
                )
            }
            ArchiveError::ExecutableBits { path } => {
                write!(f, "entry {path:?} carries executable permission bits")
            }
            ArchiveError::DuplicatePath { path } => {
                write!(
                    f,
                    "entry {path:?} collides with an earlier entry (case-folded)"
                )
            }
            ArchiveError::NotSingleRooted { path } => {
                write!(
                    f,
                    "entry {path:?} sits outside the archive's single top-level directory"
                )
            }
            ArchiveError::Empty => write!(f, "archive contains no entries"),
            ArchiveError::DestinationExists(path) => {
                write!(
                    f,
                    "extraction destination {} already exists",
                    path.display()
                )
            }
        }
    }
}

impl std::error::Error for ArchiveError {}

impl From<io::Error> for ArchiveError {
    fn from(error: io::Error) -> Self {
        ArchiveError::Io(error)
    }
}

/// What an already-seen path was, for collision arbitration: a directory may be
/// named twice (explicitly and as a file's implied parent); a file may not.
#[derive(Clone, Copy, PartialEq, Eq)]
enum SeenKind {
    Directory,
    File,
}

/// Extracts `archive` (a `.tar.gz`) into `destination`, stripping the single
/// top-level directory, under `policy`. `destination` must not yet exist.
pub fn extract_tar_gz(
    archive: &Path,
    destination: &Path,
    policy: &ArchivePolicy,
) -> Result<ExtractedTree, ArchiveError> {
    let archive_bytes = std::fs::metadata(archive)?.len();
    if archive_bytes > policy.archive_max_bytes {
        return Err(ArchiveError::ArchiveTooLarge {
            bytes: archive_bytes,
            limit: policy.archive_max_bytes,
        });
    }
    if destination.exists() {
        return Err(ArchiveError::DestinationExists(destination.to_path_buf()));
    }
    std::fs::create_dir_all(destination)?;

    let reader = BufReader::new(File::open(archive)?);
    let mut tar = tar::Archive::new(GzDecoder::new(reader));

    let mut top_level: Option<String> = None;
    let mut seen: HashMap<String, SeenKind> = HashMap::new();
    let mut file_count: usize = 0;
    let mut expanded_bytes: u64 = 0;
    let mut copy_buffer = vec![0u8; COPY_BUFFER_BYTES];

    for entry in tar.entries()? {
        let mut entry = entry?;
        let entry_type = entry.header().entry_type();

        // PAX metadata records ride along in forge tarballs (GitHub emits a
        // `pax_global_header`). They carry no content and create nothing.
        if matches!(
            entry_type,
            tar::EntryType::XHeader | tar::EntryType::XGlobalHeader
        ) {
            continue;
        }

        let raw_path = entry.path_bytes().into_owned();
        let display_path = String::from_utf8_lossy(&raw_path).into_owned();

        let is_directory = match entry_type {
            tar::EntryType::Directory => true,
            tar::EntryType::Regular => false,
            other => {
                return Err(ArchiveError::ForbiddenEntryType {
                    path: display_path,
                    kind: entry_type_name(other),
                });
            }
        };

        let components =
            validate_entry_path(&raw_path, policy).map_err(|reason| ArchiveError::HostilePath {
                path: display_path.clone(),
                reason,
            })?;

        // Single-root discipline: every entry lives under one top-level directory.
        let (root_component, relative) = components
            .split_first()
            .expect("validate_entry_path never returns an empty path");
        match &top_level {
            None => top_level = Some(root_component.clone()),
            Some(existing) if existing != root_component => {
                return Err(ArchiveError::NotSingleRooted { path: display_path });
            }
            Some(_) => {}
        }
        if relative.is_empty() {
            if is_directory {
                continue; // The wrapper directory itself — stripped, not created.
            }
            return Err(ArchiveError::NotSingleRooted { path: display_path });
        }

        let relative_display = relative.join("/");
        let directory_depth = if is_directory {
            relative.len()
        } else {
            relative.len() - 1
        };
        if directory_depth > policy.dir_max_depth {
            return Err(ArchiveError::HostilePath {
                path: display_path,
                reason: "directory nesting exceeds the depth limit",
            });
        }

        record_seen(&mut seen, &relative_display, is_directory)
            .map_err(|path| ArchiveError::DuplicatePath { path })?;
        // A file's implied parent directories join the collision namespace too,
        // so `Data/x` and a later `data` file cannot coexist.
        for ancestor in 1..relative.len() {
            let implied = relative[..ancestor].join("/");
            record_seen(&mut seen, &implied, true)
                .map_err(|path| ArchiveError::DuplicatePath { path })?;
        }

        let target = {
            let mut target = destination.to_path_buf();
            for component in relative {
                target.push(component);
            }
            target
        };

        if is_directory {
            std::fs::create_dir_all(&target)?;
            continue;
        }

        // File caps, checked against the header before a byte moves (fast, honest
        // errors) and enforced again while copying (the header is untrusted).
        if entry.header().mode()? & 0o111 != 0 {
            return Err(ArchiveError::ExecutableBits { path: display_path });
        }
        file_count += 1;
        if file_count > policy.max_file_count {
            return Err(ArchiveError::TooManyFiles {
                limit: policy.max_file_count,
            });
        }
        let declared = entry.header().size()?;
        if declared > policy.file_max_bytes {
            return Err(ArchiveError::FileTooLarge {
                path: display_path,
                limit: policy.file_max_bytes,
            });
        }

        if let Some(parent) = target.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let written = copy_capped(
            &mut entry,
            &target,
            &mut copy_buffer,
            policy.file_max_bytes,
            policy.expanded_max_bytes - expanded_bytes,
        )
        .map_err(|error| match error {
            CopyCapError::FileCap => ArchiveError::FileTooLarge {
                path: display_path.clone(),
                limit: policy.file_max_bytes,
            },
            CopyCapError::BudgetCap => ArchiveError::ExpandedTooLarge {
                limit: policy.expanded_max_bytes,
            },
            CopyCapError::Io(error) => ArchiveError::Io(error),
        })?;
        expanded_bytes += written;

        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            std::fs::set_permissions(
                &target,
                std::fs::Permissions::from_mode(EXTRACTED_FILE_MODE),
            )?;
        }
    }

    let top_level = top_level.ok_or(ArchiveError::Empty)?;
    Ok(ExtractedTree {
        root: destination.to_path_buf(),
        file_count,
        expanded_bytes,
        top_level,
    })
}

/// Records a path in the case-folded collision namespace.
fn record_seen(
    seen: &mut HashMap<String, SeenKind>,
    relative_display: &str,
    is_directory: bool,
) -> Result<(), String> {
    let folded = relative_display.to_ascii_lowercase();
    let kind = if is_directory {
        SeenKind::Directory
    } else {
        SeenKind::File
    };
    match seen.get(&folded) {
        None => {
            seen.insert(folded, kind);
            Ok(())
        }
        // A directory may be re-stated (explicit entry + implied parents)…
        Some(SeenKind::Directory) if kind == SeenKind::Directory => Ok(()),
        // …every other repeat is a collision.
        Some(_) => Err(relative_display.to_owned()),
    }
}

/// Validates one raw tar entry path against the policy and returns its
/// components (a trailing `/` on directory entries is tolerated and dropped).
fn validate_entry_path(raw: &[u8], policy: &ArchivePolicy) -> Result<Vec<String>, &'static str> {
    if raw.is_empty() {
        return Err("empty path");
    }
    if raw.len() > policy.path_max_bytes {
        return Err("path exceeds the byte limit");
    }
    if raw.contains(&0) {
        return Err("path contains a NUL byte");
    }
    if raw.iter().any(|&byte| !(0x20..=0x7e).contains(&byte)) {
        return Err(
            "path contains non-printable or non-ASCII bytes (schema v1 paths are printable ASCII)",
        );
    }
    let path = std::str::from_utf8(raw).expect("printable ASCII is UTF-8");
    if path.starts_with('/') {
        return Err("absolute path");
    }
    if path.contains('\\') {
        return Err("backslash separators (or a UNC prefix)");
    }
    if path.len() >= 2 && path.as_bytes()[1] == b':' && path.as_bytes()[0].is_ascii_alphabetic() {
        return Err("drive-letter prefix");
    }
    let trimmed = path.strip_suffix('/').unwrap_or(path);
    let mut components = Vec::new();
    for component in trimmed.split('/') {
        match component {
            "" => return Err("empty path component"),
            "." | ".." => return Err("dot or dot-dot component"),
            normal => components.push(normal.to_owned()),
        }
    }
    Ok(components)
}

/// Streams one tar entry to `target`, failing if it exceeds `file_cap` or the
/// remaining archive-wide `budget_cap`. Returns the bytes written.
fn copy_capped(
    entry: &mut impl Read,
    target: &Path,
    buffer: &mut [u8],
    file_cap: u64,
    budget_cap: u64,
) -> Result<u64, CopyCapError> {
    let mut file = File::create(target).map_err(CopyCapError::Io)?;
    let mut written: u64 = 0;
    loop {
        let read = entry.read(buffer).map_err(CopyCapError::Io)?;
        if read == 0 {
            break;
        }
        written += read as u64;
        if written > file_cap {
            return Err(CopyCapError::FileCap);
        }
        if written > budget_cap {
            return Err(CopyCapError::BudgetCap);
        }
        file.write_all(&buffer[..read]).map_err(CopyCapError::Io)?;
    }
    file.flush().map_err(CopyCapError::Io)?;
    Ok(written)
}

enum CopyCapError {
    FileCap,
    BudgetCap,
    Io(io::Error),
}

/// Human name for a rejected tar entry type.
fn entry_type_name(entry_type: tar::EntryType) -> &'static str {
    use tar::EntryType;
    match entry_type {
        EntryType::Symlink => "symlink",
        EntryType::Link => "hard link",
        EntryType::Char => "character device",
        EntryType::Block => "block device",
        EntryType::Fifo => "FIFO",
        EntryType::GNUSparse => "sparse file",
        EntryType::GNULongName | EntryType::GNULongLink => "GNU long-name record",
        _ => "unsupported entry type",
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use flate2::write::GzEncoder;
    use flate2::Compression;

    /// Builds a benign `.tar.gz` from `(path, contents)` pairs via the ordinary
    /// tar builder (the hostile corpus, which needs raw header control, lives in
    /// `tests/corpus.rs` on the `testkit` writer).
    fn benign_archive(entries: &[(&str, &[u8])]) -> Vec<u8> {
        let mut builder = tar::Builder::new(GzEncoder::new(Vec::new(), Compression::default()));
        for (path, contents) in entries {
            let mut header = tar::Header::new_ustar();
            header.set_size(contents.len() as u64);
            header.set_mode(0o644);
            header.set_cksum();
            builder.append_data(&mut header, path, *contents).unwrap();
        }
        builder.into_inner().unwrap().finish().unwrap()
    }

    fn scratch(label: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("jcx-archive-{label}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        root
    }

    fn extract(
        label: &str,
        bytes: &[u8],
        policy: &ArchivePolicy,
    ) -> Result<ExtractedTree, ArchiveError> {
        let root = scratch(label);
        let archive = root.join("release.tar.gz");
        std::fs::write(&archive, bytes).unwrap();
        extract_tar_gz(&archive, &root.join("out"), policy)
    }

    #[test]
    fn a_forge_shaped_archive_extracts_with_its_wrapper_stripped() {
        let bytes = benign_archive(&[
            ("cart-0123abc/main.c", b"void _init(void) {}\n"),
            ("cart-0123abc/data/level.map", b"\x01"),
        ]);
        let tree = extract("happy", &bytes, &COMMUNITY_ARCHIVE_POLICY).unwrap();
        assert_eq!(tree.top_level, "cart-0123abc");
        assert_eq!(tree.file_count, 2);
        assert!(tree.root.join("main.c").is_file());
        assert!(tree.root.join("data/level.map").is_file());
        assert!(!tree.root.join("cart-0123abc").exists(), "wrapper stripped");
    }

    #[cfg(unix)]
    #[test]
    fn extracted_files_land_read_write_only() {
        use std::os::unix::fs::PermissionsExt;
        let bytes = benign_archive(&[("c/main.c", b"x")]);
        let tree = extract("perms", &bytes, &COMMUNITY_ARCHIVE_POLICY).unwrap();
        let mode = std::fs::metadata(tree.root.join("main.c"))
            .unwrap()
            .permissions()
            .mode();
        assert_eq!(mode & 0o777, 0o644);
    }

    #[test]
    fn two_top_level_directories_are_rejected() {
        let bytes = benign_archive(&[("one/a.c", b"x"), ("two/b.c", b"x")]);
        assert!(matches!(
            extract("two-roots", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::NotSingleRooted { .. })
        ));
    }

    #[test]
    fn a_bare_file_at_the_root_is_rejected() {
        let bytes = benign_archive(&[("loose.c", b"x")]);
        assert!(matches!(
            extract("bare-root", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::NotSingleRooted { .. })
        ));
    }

    #[test]
    fn case_folded_duplicates_are_rejected() {
        let bytes = benign_archive(&[("c/Main.c", b"x"), ("c/main.C", b"x")]);
        assert!(matches!(
            extract("case-dupe", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::DuplicatePath { .. })
        ));
    }

    #[test]
    fn the_cumulative_expansion_budget_is_the_bomb_guard() {
        let tight = ArchivePolicy {
            expanded_max_bytes: 8,
            ..COMMUNITY_ARCHIVE_POLICY
        };
        let bytes = benign_archive(&[("c/a.txt", b"12345"), ("c/b.txt", b"67890")]);
        assert!(matches!(
            extract("budget", &bytes, &tight),
            Err(ArchiveError::ExpandedTooLarge { .. })
        ));
    }

    #[test]
    fn per_file_and_count_caps_hold() {
        let tight = ArchivePolicy {
            file_max_bytes: 4,
            ..COMMUNITY_ARCHIVE_POLICY
        };
        let bytes = benign_archive(&[("c/big.txt", b"12345")]);
        assert!(matches!(
            extract("file-cap", &bytes, &tight),
            Err(ArchiveError::FileTooLarge { .. })
        ));

        let tiny = ArchivePolicy {
            max_file_count: 1,
            ..COMMUNITY_ARCHIVE_POLICY
        };
        let bytes = benign_archive(&[("c/a.txt", b"x"), ("c/b.txt", b"x")]);
        assert!(matches!(
            extract("count-cap", &bytes, &tiny),
            Err(ArchiveError::TooManyFiles { .. })
        ));
    }

    #[test]
    fn depth_and_path_length_caps_hold() {
        let deep = format!("c/{}/file.txt", ["d"; DIR_MAX_DEPTH + 1].join("/"));
        let bytes = benign_archive(&[(deep.as_str(), b"x")]);
        assert!(matches!(
            extract("depth", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::HostilePath { .. })
        ));

        let long_name = format!("c/{}.txt", "n".repeat(PATH_MAX_BYTES));
        let bytes = benign_archive(&[(long_name.as_str(), b"x")]);
        assert!(matches!(
            extract("long-path", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::HostilePath { .. })
        ));
    }

    #[test]
    fn executable_bits_are_rejected_outright() {
        let mut builder = tar::Builder::new(GzEncoder::new(Vec::new(), Compression::default()));
        let mut header = tar::Header::new_ustar();
        header.set_size(2);
        header.set_mode(0o755);
        header.set_cksum();
        builder
            .append_data(&mut header, "c/tool.c", &b"x\n"[..])
            .unwrap();
        let bytes = builder.into_inner().unwrap().finish().unwrap();
        assert!(matches!(
            extract("exec-bits", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::ExecutableBits { .. })
        ));
    }

    #[test]
    fn an_oversized_archive_never_opens() {
        let tight = ArchivePolicy {
            archive_max_bytes: 8,
            ..COMMUNITY_ARCHIVE_POLICY
        };
        let bytes = benign_archive(&[("c/a.txt", b"contents beyond eight bytes")]);
        assert!(matches!(
            extract("archive-cap", &bytes, &tight),
            Err(ArchiveError::ArchiveTooLarge { .. })
        ));
    }

    #[test]
    fn nul_bytes_in_paths_are_rejected_at_the_validator() {
        // A NUL cannot ride the ustar name field (it terminates it), so this
        // guard is exercised directly — defence in depth for any future
        // container or PAX pathway that could carry one.
        assert!(validate_entry_path(b"cart/evil\0name.c", &COMMUNITY_ARCHIVE_POLICY).is_err());
        assert!(validate_entry_path(b"", &COMMUNITY_ARCHIVE_POLICY).is_err());
    }

    #[test]
    fn an_empty_archive_is_refused() {
        let bytes = benign_archive(&[]);
        assert!(matches!(
            extract("empty", &bytes, &COMMUNITY_ARCHIVE_POLICY),
            Err(ArchiveError::Empty)
        ));
    }
}
