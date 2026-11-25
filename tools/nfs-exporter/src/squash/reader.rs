use std::path::Path;
pub fn read_squash_file(path: &Path, offset: u64, name: &str) -> Option<Vec<u8>> {
    super::native::Squash::open(path, offset)
        .ok()?
        .read(name)
        .ok()
}
