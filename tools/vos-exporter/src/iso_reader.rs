use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

pub const SECTOR_SIZE: u64 = 2048;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Extent {
    pub offset: u64,
    pub size: u64,
}

pub struct IsoReader {
    file: File,
    image_size: u64,
    root: Option<Extent>,
}

fn read_u32_le(bytes: &[u8]) -> u32 {
    u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])
}

impl IsoReader {
    pub fn new(path: &Path) -> std::io::Result<Self> {
        let mut file = File::open(path)?;
        let image_size = file.metadata()?.len();
        let mut root = None;

        let mut block = [0u8; SECTOR_SIZE as usize];
        for sector in 16..64u64 {
            if sector * SECTOR_SIZE + SECTOR_SIZE > image_size {
                break;
            }
            file.seek(SeekFrom::Start(sector * SECTOR_SIZE))?;
            if file.read_exact(&mut block).is_err() {
                break;
            }
            if &block[1..6] != b"CD001" || block[6] != 1 || block[0] == 255 {
                break;
            }
            if block[0] != 1 {
                continue;
            }
            if block[128] != 0 || block[129] != 8 || block[156] < 34 || (block[181] & 2) == 0 {
                break;
            }
            let rec = &block[156..];
            let offset = (read_u32_le(&rec[2..6]) as u64 + rec[1] as u64) * SECTOR_SIZE;
            let size = read_u32_le(&rec[10..14]) as u64;
            root = Some(Extent { offset, size });
            break;
        }

        Ok(Self {
            file,
            image_size,
            root,
        })
    }

    pub fn image_size(&self) -> u64 {
        self.image_size
    }

    pub fn files(&mut self) -> Result<Vec<(String, Extent)>, String> {
        let root = self.root.ok_or("Missing ISO root")?;
        let mut stack = vec![(String::new(), root)];
        let mut visited = std::collections::BTreeSet::new();
        let mut result = Vec::new();
        while let Some((parent, dir)) = stack.pop() {
            if !visited.insert(dir.offset) {
                continue;
            }
            if visited.len() > 100000 {
                return Err("ISO directory limit exceeded".into());
            }
            let mut block = [0; 2048];
            for offset in (0..dir.size).step_by(2048) {
                if !self.read_at(dir.offset + offset, &mut block) {
                    return Err("Cannot read ISO directory".into());
                }
                let mut pos = 0;
                while pos < 2048 {
                    let n = block[pos] as usize;
                    if n == 0 {
                        break;
                    }
                    if n < 34 || pos + n > 2048 {
                        return Err("Invalid ISO directory record".into());
                    }
                    let rec = &block[pos..pos + n];
                    pos += n;
                    let len = rec[32] as usize;
                    if 33 + len > n {
                        return Err("Invalid ISO filename".into());
                    }
                    if len == 1 && rec[33] < 2 {
                        continue;
                    }
                    let mut name = String::from_utf8_lossy(&rec[33..33 + len])
                        .trim_end_matches(";1")
                        .to_string();
                    let mut p = 33 + len + usize::from(len % 2 == 0);
                    while p + 4 <= n {
                        let size = rec[p + 2] as usize;
                        if size < 4 || p + size > n {
                            break;
                        }
                        if &rec[p..p + 2] == b"NM" && size >= 5 && rec[p + 4] == 0 {
                            name = String::from_utf8_lossy(&rec[p + 5..p + size]).into_owned();
                            break;
                        }
                        p += size;
                    }
                    if name.is_empty()
                        || name == "."
                        || name == ".."
                        || name.contains(['/', '\\', '\0'])
                    {
                        return Err("Unsafe ISO path".into());
                    }
                    let path = format!("{parent}/{name}");
                    let extent = Extent {
                        offset: (read_u32_le(&rec[2..6]) as u64 + rec[1] as u64) * 2048,
                        size: read_u32_le(&rec[10..14]) as u64,
                    };
                    if rec[25] & 2 != 0 {
                        stack.push((path, extent));
                    } else if rec[25] & 0x80 != 0 {
                        return Err("Multi-extent ISO files require a newer reader".into());
                    } else {
                        result.push((path, extent));
                    }
                }
            }
        }
        Ok(result)
    }

    fn read_at(&mut self, offset: u64, buf: &mut [u8]) -> bool {
        if offset > self.image_size || buf.len() as u64 > self.image_size - offset {
            return false;
        }
        if self.file.seek(SeekFrom::Start(offset)).is_err() {
            return false;
        }
        self.file.read_exact(buf).is_ok()
    }

    fn matches_name(record: &[u8], name: &str) -> bool {
        let len = record[0] as usize;
        let id_size = record[32] as usize;
        if id_size == 0 || 33 + id_size > len || (id_size == 1 && record[33] < 2) {
            return false;
        }

        let mut pos = 33 + id_size + if id_size % 2 == 0 { 1 } else { 0 };
        while pos + 4 <= len {
            let field = &record[pos..];
            let field_len = field[2] as usize;
            if field_len < 4 || field_len > len - pos {
                break;
            }
            if field[0] == b'N' && field[1] == b'M' {
                if field_len >= 5 && field[4] == 0 {
                    let nm = &field[5..field_len];
                    return nm == name.as_bytes();
                }
            }
            if (field[0] == b'C' && (field[1] == b'E' || field[1] == b'L'))
                || (field[0] == b'R' && field[1] == b'E')
            {
                return false;
            }
            pos += field_len;
        }

        let mut id = &record[33..33 + id_size];
        if id.ends_with(b";1") {
            id = &id[..id.len() - 2];
        }
        id.eq_ignore_ascii_case(name.as_bytes())
    }

    pub fn find(&mut self, mut path: &str) -> Option<Extent> {
        let mut directory = self.root?;
        while directory.size > 0 && path.starts_with('/') {
            path = &path[1..];
            let slash_idx = path.find('/');
            let name = match slash_idx {
                Some(idx) => &path[..idx],
                None => path,
            };
            let descend = slash_idx.is_some();
            let mut found: Option<Extent> = None;
            let mut complete = false;
            let mut block = [0u8; SECTOR_SIZE as usize];

            let mut position = 0u64;
            while position < directory.size && !complete {
                let bytes = std::cmp::min(SECTOR_SIZE, directory.size - position) as usize;
                if !self.read_at(directory.offset + position, &mut block[..bytes]) {
                    return None;
                }
                let mut i = 0;
                while i < bytes {
                    let rec = &block[i..];
                    let length = rec[0] as usize;
                    if length == 0 {
                        break;
                    }
                    if length < 34 || length > bytes - i {
                        return None;
                    }
                    i += length;

                    if !Self::matches_name(&rec[..length], name) {
                        if found.is_some() {
                            return None;
                        }
                        continue;
                    }

                    let is_dir = (rec[25] & 2) != 0;
                    if descend != is_dir || rec[26] != 0 || rec[27] != 0 {
                        return None;
                    }

                    let sec_offset = (read_u32_le(&rec[2..6]) as u64 + rec[1] as u64) * SECTOR_SIZE;
                    let sec_size = read_u32_le(&rec[10..14]) as u64;
                    if sec_offset > self.image_size || sec_size > self.image_size - sec_offset {
                        return None;
                    }

                    let section = Extent {
                        offset: sec_offset,
                        size: sec_size,
                    };
                    if let Some(ref mut f) = found {
                        if f.size % SECTOR_SIZE != 0 || section.offset != f.offset + f.size {
                            return None;
                        }
                        f.size += section.size;
                    } else {
                        found = Some(section);
                    }

                    complete = (rec[25] & 128) == 0;
                    if complete {
                        break;
                    }
                }
                position += SECTOR_SIZE;
            }

            if !complete {
                return None;
            }
            if !descend {
                return found;
            }
            directory = found?;
            path = &path[slash_idx.unwrap()..];
        }

        None
    }

    pub fn text(&mut self, path: &str) -> Option<String> {
        let item = self.find(path)?;
        if item.size > 16_000_000 {
            return None;
        }
        let mut content = vec![0u8; item.size as usize];
        if !self.read_at(item.offset, &mut content) {
            return None;
        }
        String::from_utf8(content).ok()
    }

    pub fn squash(&mut self, path: &str) -> u64 {
        let item = match self.find(path) {
            Some(i) => i,
            None => return 0,
        };
        let mut header = [0u8; 96];
        if item.size < header.len() as u64 || !self.read_at(item.offset, &mut header) {
            return 0;
        }
        if &header[0..4] != b"hsqs" {
            return 0;
        }
        let used =
            read_u32_le(&header[40..44]) as u64 | ((read_u32_le(&header[44..48]) as u64) << 32);
        if used < header.len() as u64 || used > item.size {
            return 0;
        }
        item.offset
    }

    pub fn has_el_torito_boot(&mut self) -> bool {
        let mut block = [0u8; SECTOR_SIZE as usize];
        let mut catalog = 0u32;
        for sector in 16..64u64 {
            if !self.read_at(sector * SECTOR_SIZE, &mut block) {
                break;
            }
            if &block[1..6] != b"CD001" || block[6] != 1 || block[0] == 255 {
                break;
            }
            if block[0] == 0 && &block[7..30] == b"EL TORITO SPECIFICATION" {
                catalog = read_u32_le(&block[71..75]);
                break;
            }
        }
        if catalog == 0 {
            return false;
        }
        if !self.read_at(catalog as u64 * SECTOR_SIZE, &mut block) {
            return false;
        }
        if block[0] != 1 || block[30] != 0x55 || block[31] != 0xaa {
            return false;
        }

        let mut checksum = 0u32;
        for i in (0..32).step_by(2) {
            checksum += (block[i] as u32) | ((block[i + 1] as u32) << 8);
        }
        if (checksum & 0xffff) != 0 {
            return false;
        }

        let bootable = |entry: &[u8], platform: u8, image_size: u64| -> bool {
            if (platform != 0 && platform != 0xef) || entry[0] != 0x88 {
                return false;
            }
            let start = read_u32_le(&entry[8..12]) as u64 * SECTOR_SIZE;
            let length = ((entry[6] as u64) | ((entry[7] as u64) << 8)) * 512;
            start != 0 && start < image_size && length <= image_size - start
        };

        if bootable(&block[32..64], block[1], self.image_size) {
            return true;
        }

        let mut pos = 64;
        while pos + 32 <= block.len() {
            let entry_type = block[pos];
            if entry_type != 0x90 && entry_type != 0x91 {
                break;
            }
            let platform = block[pos + 1];
            let count = (block[pos + 2] as usize) | ((block[pos + 3] as usize) << 8);
            pos += 32;
            if count > (block.len() - pos) / 32 {
                break;
            }
            for _ in 0..count {
                if bootable(&block[pos..pos + 32], platform, self.image_size) {
                    return true;
                }
                pos += 32;
            }
            if entry_type == 0x91 {
                break;
            }
        }

        false
    }

    pub fn has_bootable_structures(&mut self) -> bool {
        if self.has_el_torito_boot() {
            return true;
        }
        let mut sector = [0u8; 512];
        if self.read_at(0, &mut sector) {
            if sector[510] == 0x55 && sector[511] == 0xaa {
                for i in 0..4 {
                    let part = &sector[446 + i * 16..];
                    if part[4] != 0 || part[0] == 0x80 {
                        return true;
                    }
                }
            }
        }
        if self.read_at(512, &mut sector) {
            if &sector[0..8] == b"EFI PART" {
                return true;
            }
        }
        false
    }

    pub fn read_bytes(&mut self, path: &str) -> Option<Vec<u8>> {
        let item = self.find(path)?;
        if item.size > 512 * 1024 * 1024 {
            return None;
        }
        let mut buf = vec![0u8; item.size as usize];
        if self.read_at(item.offset, &mut buf) {
            Some(buf)
        } else {
            None
        }
    }

    pub fn read_text(&mut self, path: &str) -> Option<String> {
        let bytes = self.read_bytes(path)?;
        String::from_utf8(bytes).ok()
    }

    pub fn extract_to(&mut self, dest_dir: &Path) -> Result<(), String> {
        let root = self.root.ok_or("Cannot find ISO root directory.")?;
        fs::create_dir_all(dest_dir).map_err(|e| e.to_string())?;

        let mut stack: Vec<(u64, u64, PathBuf)> =
            vec![(root.offset, root.size, dest_dir.to_path_buf())];

        while let Some((offset, length, cur_dir)) = stack.pop() {
            fs::create_dir_all(&cur_dir).map_err(|e| e.to_string())?;
            let mut block = [0u8; SECTOR_SIZE as usize];

            let mut pos = 0u64;
            while pos < length {
                let bytes = std::cmp::min(SECTOR_SIZE, length - pos) as usize;
                if !self.read_at(offset + pos, &mut block[..bytes]) {
                    break;
                }
                let mut i = 0;
                while i < bytes {
                    let rec = &block[i..];
                    let len = rec[0] as usize;
                    if len == 0 || len < 34 || len > bytes - i {
                        break;
                    }
                    i += len;

                    let fi_len = rec[32] as usize;
                    if fi_len == 0 || (fi_len == 1 && (rec[33] == 0 || rec[33] == 1)) {
                        continue;
                    }

                    let mut name = String::new();
                    let mut p = 33 + fi_len + if fi_len % 2 == 0 { 1 } else { 0 };
                    while p + 4 <= len {
                        let f = &rec[p..];
                        let flen = f[2] as usize;
                        if flen < 4 || flen > len - p {
                            break;
                        }
                        if f[0] == b'N' && f[1] == b'M' && flen >= 5 && f[4] == 0 {
                            name = String::from_utf8_lossy(&f[5..flen]).to_string();
                            break;
                        }
                        p += flen;
                    }
                    if name.is_empty() {
                        let mut raw = String::from_utf8_lossy(&rec[33..33 + fi_len]).to_string();
                        if raw.ends_with(";1") {
                            raw.truncate(raw.len() - 2);
                        }
                        name = raw;
                    }

                    let child_offset = read_u32_le(&rec[2..6]) as u64 * SECTOR_SIZE;
                    let child_len = read_u32_le(&rec[10..14]) as u64;
                    let is_dir = (rec[25] & 2) != 0;
                    let child_path = cur_dir.join(&name);

                    if is_dir {
                        stack.push((child_offset, child_len, child_path));
                    } else {
                        let mut out = File::create(&child_path)
                            .map_err(|e| format!("Cannot create file {}: {}", name, e))?;
                        let mut buf = vec![0u8; 65536];
                        let mut fpos = 0u64;
                        while fpos < child_len {
                            let count = std::cmp::min(buf.len() as u64, child_len - fpos) as usize;
                            if !self.read_at(child_offset + fpos, &mut buf[..count]) {
                                return Err(format!("Cannot read ISO data for file: {}", name));
                            }
                            out.write_all(&buf[..count])
                                .map_err(|e| format!("Cannot write file {}: {}", name, e))?;
                            fpos += count as u64;
                        }
                    }
                }
                pos += SECTOR_SIZE;
            }
        }

        Ok(())
    }
}
