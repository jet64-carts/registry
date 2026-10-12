// validator/src/screenshot.rs
//! The INFO screen's one picture (docs/cart_exchange/cart_exchange.md §12 P6.2).
//!
//! Convention over configuration, exactly like `file_id.diz`: a release *may*
//! ship a single `screenshot.png` beside `cart.toml`. It is presentation, not
//! source — it lives in the author's repository at the pinned revision, is
//! **not** part of the release surface (the include patterns exclude it, so it
//! is never installed, fingerprinted, or compiled), and the board fetches it
//! lazily by a digest this module attests at review time.
//!
//! The caps are deliberately strict (E14: the board stays text-first — one
//! period-appropriate image, never a gallery), and the inspection is a pure
//! header parse with no image dependency: the PNG signature and the IHDR chunk
//! give the dimensions in the first 24 bytes, which is everything acceptance
//! needs to attest. Both sides of the trust boundary use this one inspection —
//! CI to admit the picture, the Workstation to refuse a decode that the pinned
//! facts do not license — so "malformed" means the same thing everywhere.

use serde::{Deserialize, Serialize};

/// Canonical screenshot filename at the cart root, beside `cart.toml`.
pub const SCREENSHOT_FILE: &str = "screenshot.png";

/// Hard byte ceiling. A pixel-art screenshot is tens of KiB; a quarter MiB is
/// generous, and it keeps the lazy fetch a polite download forever.
pub const SCREENSHOT_MAX_BYTES: u64 = 256 * 1024;

/// Dimension ceilings — the machine's own native display (`architecture.md §2`);
/// nothing about a Jet64 cart can honestly need a larger picture.
pub const SCREENSHOT_MAX_WIDTH: u32 = 1280;
pub const SCREENSHOT_MAX_HEIGHT: u32 = 720;

/// The PNG file signature — the eight bytes every real PNG stream opens with.
const PNG_SIGNATURE: [u8; 8] = [0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n'];

/// Byte length of the IHDR chunk's data, fixed by the PNG specification.
const IHDR_DATA_BYTES: u32 = 13;

/// Offset of the IHDR chunk header (length + type) — immediately after the
/// signature — and of the width/height words inside its data.
const IHDR_LENGTH_OFFSET: usize = 8;
const IHDR_TYPE_OFFSET: usize = 12;
const IHDR_WIDTH_OFFSET: usize = 16;
const IHDR_HEIGHT_OFFSET: usize = 20;

/// The minimum prefix an inspection needs: signature + IHDR header + dimensions.
const INSPECT_PREFIX_BYTES: usize = IHDR_HEIGHT_OFFSET + 4;

/// The attested facts of one release screenshot — what a registry entry pins
/// (§6.2) and the Workstation verifies before a single pixel is decoded. The
/// digest is transport trust (the lazy fetch is judged against it); the
/// dimensions are decode trust (a decoder is never asked to allocate for a
/// picture the review did not license).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ScreenshotFacts {
    /// SHA-256 of the file bytes, lowercase hex.
    pub sha256: String,
    pub bytes: u64,
    pub width: u32,
    pub height: u32,
}

/// A PNG's pixel dimensions, read from its IHDR chunk.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PngDimensions {
    pub width: u32,
    pub height: u32,
}

/// Reads the dimensions from a PNG stream's header, refusing anything that is
/// not shaped like a real PNG. A pure parse — no decompression, no allocation
/// proportional to the image — so it is safe to run on untrusted bytes.
pub fn inspect_png(bytes: &[u8]) -> Result<PngDimensions, String> {
    if bytes.len() < INSPECT_PREFIX_BYTES {
        return Err(format!(
            "not a PNG: {} bytes is shorter than a PNG header",
            bytes.len()
        ));
    }
    if bytes[..PNG_SIGNATURE.len()] != PNG_SIGNATURE {
        return Err("not a PNG: the file does not open with the PNG signature".to_owned());
    }
    let word = |offset: usize| {
        u32::from_be_bytes([
            bytes[offset],
            bytes[offset + 1],
            bytes[offset + 2],
            bytes[offset + 3],
        ])
    };
    if word(IHDR_LENGTH_OFFSET) != IHDR_DATA_BYTES
        || &bytes[IHDR_TYPE_OFFSET..IHDR_TYPE_OFFSET + 4] != b"IHDR"
    {
        return Err("not a PNG: the first chunk is not a well-formed IHDR".to_owned());
    }
    Ok(PngDimensions {
        width: word(IHDR_WIDTH_OFFSET),
        height: word(IHDR_HEIGHT_OFFSET),
    })
}

/// The full admission judgement: a real PNG, within every cap. Returns the
/// dimensions so the caller can attest them without a second parse.
pub fn validate_screenshot(bytes: &[u8]) -> Result<PngDimensions, String> {
    if bytes.len() as u64 > SCREENSHOT_MAX_BYTES {
        return Err(format!(
            "screenshot is {} bytes (maximum {SCREENSHOT_MAX_BYTES})",
            bytes.len()
        ));
    }
    let dimensions = inspect_png(bytes)?;
    let in_bounds = (1..=SCREENSHOT_MAX_WIDTH).contains(&dimensions.width)
        && (1..=SCREENSHOT_MAX_HEIGHT).contains(&dimensions.height);
    if !in_bounds {
        return Err(format!(
            "screenshot is {}×{} (maximum {SCREENSHOT_MAX_WIDTH}×{SCREENSHOT_MAX_HEIGHT})",
            dimensions.width, dimensions.height
        ));
    }
    Ok(dimensions)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::testkit::png_header as png_prefix;

    #[test]
    fn a_well_formed_header_yields_its_dimensions() {
        let dims = inspect_png(&png_prefix(640, 400)).expect("a real PNG header");
        assert_eq!((dims.width, dims.height), (640, 400));
    }

    #[test]
    fn everything_not_shaped_like_a_png_is_refused_by_name() {
        assert!(inspect_png(b"").unwrap_err().contains("shorter"));
        assert!(inspect_png(&[0u8; 64]).unwrap_err().contains("signature"));
        let mut wrong_chunk = png_prefix(640, 400);
        wrong_chunk[IHDR_TYPE_OFFSET..IHDR_TYPE_OFFSET + 4].copy_from_slice(b"JUNK");
        assert!(inspect_png(&wrong_chunk).unwrap_err().contains("IHDR"));
    }

    #[test]
    fn the_caps_hold_in_every_direction() {
        assert!(
            validate_screenshot(&png_prefix(SCREENSHOT_MAX_WIDTH, SCREENSHOT_MAX_HEIGHT)).is_ok()
        );
        assert!(
            validate_screenshot(&png_prefix(SCREENSHOT_MAX_WIDTH + 1, 400))
                .unwrap_err()
                .contains("maximum")
        );
        assert!(validate_screenshot(&png_prefix(640, SCREENSHOT_MAX_HEIGHT + 1)).is_err());
        assert!(
            validate_screenshot(&png_prefix(0, 400)).is_err(),
            "zero width is not a picture"
        );

        let oversized = vec![0u8; SCREENSHOT_MAX_BYTES as usize + 1];
        assert!(validate_screenshot(&oversized)
            .unwrap_err()
            .contains("bytes"));
    }

    #[test]
    fn facts_round_trip_and_refuse_unknown_keys() {
        let facts = ScreenshotFacts {
            sha256: "ab".repeat(32),
            bytes: 48_213,
            width: 640,
            height: 400,
        };
        let toml = toml::to_string(&facts).unwrap();
        assert_eq!(toml::from_str::<ScreenshotFacts>(&toml).unwrap(), facts);
        assert!(
            toml::from_str::<ScreenshotFacts>(
                "sha256 = \"aa\"\nbytes = 1\nwidth = 1\nheight = 1\nurl = \"x\""
            )
            .is_err(),
            "no smuggled fields (E1's discipline)"
        );
    }
}
