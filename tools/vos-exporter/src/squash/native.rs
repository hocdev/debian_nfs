use std::collections::{BTreeMap, BTreeSet};
use std::fs::File;
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::Path;
type Result<T> = std::result::Result<T, String>;
fn u16le(b: &[u8], p: usize) -> u16 {
    u16::from_le_bytes(b[p..p + 2].try_into().unwrap())
}
fn u32le(b: &[u8], p: usize) -> u32 {
    u32::from_le_bytes(b[p..p + 4].try_into().unwrap())
}
fn u64le(b: &[u8], p: usize) -> u64 {
    u64::from_le_bytes(b[p..p + 8].try_into().unwrap())
}
fn put16(b: &mut Vec<u8>, n: u16) {
    b.extend_from_slice(&n.to_le_bytes());
}
fn put32(b: &mut Vec<u8>, n: u32) {
    b.extend_from_slice(&n.to_le_bytes());
}
fn put64(b: &mut Vec<u8>, n: u64) {
    b.extend_from_slice(&n.to_le_bytes());
}

#[derive(Clone, Debug)]
pub struct Node {
    pub reference: u64,
    pub header: Vec<u8>,
    pub raw: Vec<u8>,
    pub kind: Kind,
}
#[derive(Clone, Debug)]
pub enum Kind {
    Dir {
        start: u32,
        offset: u16,
        size: u32,
        parent: u32,
    },
    File {
        start: u64,
        size: u64,
        fragment: u32,
        offset: u32,
        blocks: Vec<u32>,
    },
    Link(String),
    Other,
}
#[derive(Clone, Debug)]
pub struct Entry {
    pub name: String,
    pub reference: u64,
    pub number: u32,
    pub kind: u16,
}
pub struct Squash {
    file: File,
    base: u64,
    pub superblock: Vec<u8>,
    pub block_size: usize,
    pub used: u64,
    inode_start: u64,
    dir_start: u64,
    cache: BTreeMap<u64, (Vec<u8>, u64)>,
    data_cache: BTreeMap<u64, Vec<u8>>,
}
impl Squash {
    pub fn open(path: &Path, base: u64) -> Result<Self> {
        let file = File::open(path).map_err(|e| e.to_string())?;
        let mut fs = Self {
            file,
            base,
            superblock: vec![],
            block_size: 0,
            used: 0,
            inode_start: 0,
            dir_start: 0,
            cache: BTreeMap::new(),
            data_cache: BTreeMap::new(),
        };
        let header = fs.at(0, 96)?;
        if &header[..4] != b"hsqs" || u16le(&header, 28) != 4 || u16le(&header, 30) != 0 {
            return Err("Requires SquashFS 4.0 little endian".into());
        }
        if u16le(&header, 20) != 4 {
            return Err(
                "Bản Rust chuẩn hiện hỗ trợ SquashFS nén XZ; ISO này dùng bộ nén khác.".into(),
            );
        }
        fs.block_size = u32le(&header, 12) as usize;
        fs.used = u64le(&header, 40);
        fs.inode_start = u64le(&header, 64);
        fs.dir_start = u64le(&header, 72);
        if !fs.block_size.is_power_of_two()
            || !(4096..=1048576).contains(&fs.block_size)
            || fs
                .base
                .checked_add(fs.used)
                .is_none_or(|end| end > fs.file.metadata().map(|m| m.len()).unwrap_or(0))
        {
            return Err("Invalid SquashFS bounds".into());
        }
        fs.superblock = header;
        Ok(fs)
    }
    fn at(&mut self, position: u64, length: usize) -> Result<Vec<u8>> {
        if length > 16 * 1024 * 1024 {
            return Err("SquashFS read too large".into());
        }
        if self.used != 0
            && position
                .checked_add(length as u64)
                .is_none_or(|end| end > self.used)
        {
            return Err("SquashFS extent exceeds image".into());
        }
        let mut bytes = vec![0; length];
        self.file
            .seek(SeekFrom::Start(
                self.base
                    .checked_add(position)
                    .ok_or("SquashFS offset overflow")?,
            ))
            .map_err(|e| e.to_string())?;
        self.file
            .read_exact(&mut bytes)
            .map_err(|e| e.to_string())?;
        Ok(bytes)
    }
    fn block(&mut self, position: u64) -> Result<(Vec<u8>, u64)> {
        if let Some(found) = self.cache.get(&position) {
            return Ok(found.clone());
        }
        let header = self.at(position, 2)?;
        let size = u16le(&header, 0);
        let n = (size & 0x7fff) as usize;
        if n == 0 || n > 8192 {
            return Err("Invalid SquashFS metadata size".into());
        }
        let input = self.at(position + 2, n)?;
        let bytes = if size & 0x8000 != 0 {
            input
        } else {
            crate::xz::decode(&input, 8192)?
        };
        let value = (bytes, position + 2 + n as u64);
        self.cache.insert(position, value.clone());
        Ok(value)
    }
    fn meta(&mut self, mut position: u64, mut offset: usize, length: usize) -> Result<Vec<u8>> {
        let mut result = Vec::with_capacity(length);
        while result.len() < length {
            if !self.cache.contains_key(&position) {
                self.block(position)?;
            }
            let (block, next) = self.cache.get(&position).ok_or("Missing metadata block")?;
            if offset >= block.len() {
                return Err("Invalid SquashFS metadata offset".into());
            }
            let n = (length - result.len()).min(block.len() - offset);
            result.extend_from_slice(&block[offset..offset + n]);
            position = *next;
            offset = 0;
        }
        Ok(result)
    }
    pub fn root_ref(&self) -> u64 {
        u64le(&self.superblock, 32)
    }
    pub fn fragment_info(&mut self, index: u32) -> Result<(u64, u32)> {
        if index >= u32le(&self.superblock, 16) {
            return Err("Invalid fragment index".into());
        }
        let ptr = self.at(u64le(&self.superblock, 80) + (index as u64 / 512) * 8, 8)?;
        let entry = self.meta(u64le(&ptr, 0), index as usize % 512 * 16, 16)?;
        Ok((u64le(&entry, 0), u32le(&entry, 8)))
    }
    pub fn inode(&mut self, reference: u64) -> Result<Node> {
        let position = self
            .inode_start
            .checked_add(reference >> 16)
            .ok_or("Invalid inode reference")?;
        let offset = (reference & 0xffff) as usize;
        let common = self.meta(position, offset, 16)?;
        let kind = u16le(&common, 0);
        let fixed = match kind {
            1 => 32,
            8 => 40,
            2 => 32,
            9 => 56,
            3 | 10 => 24,
            4 | 5 => 24,
            11 | 12 => 28,
            6 | 7 => 20,
            13 | 14 => 24,
            _ => return Err(format!("Invalid SquashFS inode type {kind}")),
        };
        let bytes = self.meta(position, offset, fixed)?;
        let content = match kind {
            1 => Kind::Dir {
                start: u32le(&bytes, 16),
                offset: u16le(&bytes, 26),
                size: u16le(&bytes, 24) as u32,
                parent: u32le(&bytes, 28),
            },
            8 => Kind::Dir {
                start: u32le(&bytes, 24),
                offset: u16le(&bytes, 34),
                size: u32le(&bytes, 20),
                parent: u32le(&bytes, 28),
            },
            2 | 9 => {
                let (start, size, fragment, frag_offset) = if kind == 2 {
                    (
                        u32le(&bytes, 16) as u64,
                        u32le(&bytes, 28) as u64,
                        u32le(&bytes, 20),
                        u32le(&bytes, 24),
                    )
                } else {
                    (
                        u64le(&bytes, 16),
                        u64le(&bytes, 24),
                        u32le(&bytes, 44),
                        u32le(&bytes, 48),
                    )
                };
                let count = if fragment == u32::MAX {
                    size.div_ceil(self.block_size as u64)
                } else {
                    size / self.block_size as u64
                };
                if count > 1_000_000 {
                    return Err("SquashFS block list too large".into());
                }
                let all = self.meta(position, offset, fixed + count as usize * 4)?;
                Kind::File {
                    start,
                    size,
                    fragment,
                    offset: frag_offset,
                    blocks: all[fixed..].chunks_exact(4).map(|b| u32le(b, 0)).collect(),
                }
            }
            3 | 10 => {
                let n = u32le(&bytes, 20) as usize;
                if n > 4096 {
                    return Err("Symlink too long".into());
                }
                let all = self.meta(position, offset, 24 + n)?;
                Kind::Link(String::from_utf8(all[24..].to_vec()).map_err(|e| e.to_string())?)
            }
            _ => Kind::Other,
        };
        let raw = match &content {
            Kind::Dir { .. } => bytes,
            Kind::File { blocks, .. } => self.meta(position, offset, fixed + blocks.len() * 4)?,
            Kind::Link(link) => self.meta(
                position,
                offset,
                24 + link.len() + if kind == 10 { 4 } else { 0 },
            )?,
            _ => bytes,
        };
        Ok(Node {
            reference,
            header: common,
            raw,
            kind: content,
        })
    }
    pub fn entries(&mut self, node: &Node) -> Result<Vec<Entry>> {
        let Kind::Dir {
            start,
            offset,
            size,
            ..
        } = node.kind
        else {
            return Err("Not a directory".into());
        };
        if size < 3 || size > 16 * 1024 * 1024 {
            return Err("Invalid SquashFS directory size".into());
        }
        let bytes = self.meta(
            self.dir_start + start as u64,
            offset as usize,
            size as usize - 3,
        )?;
        let mut pos = 0;
        let mut entries = Vec::new();
        while pos < bytes.len() {
            if bytes.len() - pos < 12 {
                return Err("Truncated directory header".into());
            }
            let count = u32le(&bytes, pos) + 1;
            let block = u32le(&bytes, pos + 4);
            let number = u32le(&bytes, pos + 8);
            pos += 12;
            if count > 256 {
                return Err("Invalid directory entry count".into());
            }
            for _ in 0..count {
                if bytes.len() - pos < 8 {
                    return Err("Truncated directory entry".into());
                }
                let off = u16le(&bytes, pos);
                let delta = u16le(&bytes, pos + 2) as i16;
                let kind = u16le(&bytes, pos + 4);
                let n = u16le(&bytes, pos + 6) as usize + 1;
                pos += 8;
                let name = String::from_utf8(
                    bytes
                        .get(pos..pos + n)
                        .ok_or("Truncated directory name")?
                        .to_vec(),
                )
                .map_err(|_| "Non-UTF8 SquashFS path is unsupported")?;
                pos += n;
                if name == "." || name == ".." || name.contains(['/', '\0']) {
                    return Err(format!("Unsafe SquashFS path: {name:?}"));
                }
                entries.push(Entry {
                    name,
                    reference: ((block as u64) << 16) | off as u64,
                    number: number.wrapping_add(delta as u32),
                    kind,
                });
            }
        }
        Ok(entries)
    }
    pub fn find(&mut self, path: &str) -> Result<Node> {
        let mut node = self.inode(self.root_ref())?;
        for segment in path.trim_matches('/').split('/').filter(|s| !s.is_empty()) {
            if segment == ".." || segment == "." || segment.contains('\\') {
                return Err("Unsafe SquashFS path".into());
            }
            let entry = self
                .entries(&node)?
                .into_iter()
                .find(|e| e.name == segment)
                .ok_or_else(|| format!("Không tìm thấy {path}"))?;
            node = self.inode(entry.reference)?;
        }
        Ok(node)
    }
    pub fn read(&mut self, path: &str) -> Result<Vec<u8>> {
        self.read_link(path, 0)
    }
    fn read_link(&mut self, path: &str, depth: usize) -> Result<Vec<u8>> {
        if depth > 16 {
            return Err("Symlink cycle".into());
        }
        let node = self.find(path)?;
        if let Kind::Link(link) = &node.kind {
            let combined = if link.starts_with('/') {
                link.clone()
            } else {
                format!(
                    "{}/{}",
                    path.rsplit_once('/').map(|x| x.0).unwrap_or(""),
                    link
                )
            };
            let mut parts = Vec::new();
            for p in combined.split('/') {
                if p == ".." {
                    parts.pop();
                } else if !p.is_empty() && p != "." {
                    parts.push(p);
                }
            }
            return self.read_link(&parts.join("/"), depth + 1);
        }
        self.read_node(&node)
    }
    pub fn read_node(&mut self, node: &Node) -> Result<Vec<u8>> {
        let Kind::File {
            start,
            size,
            fragment,
            offset,
            ref blocks,
        } = node.kind
        else {
            return Err("Not a regular file".into());
        };
        if size > 64 * 1024 * 1024 {
            return Err("File exceeds 64 MiB read limit".into());
        }
        let mut out = Vec::with_capacity(size as usize);
        let mut at = start;
        for &block in blocks {
            let n = (block & 0x00ffffff) as usize;
            if n == 0 {
                out.resize((out.len() + self.block_size).min(size as usize), 0);
                continue;
            }
            let input = self.at(at, n)?;
            at += n as u64;
            let data = if block & (1 << 24) != 0 {
                input
            } else {
                crate::xz::decode(&input, self.block_size)?
            };
            out.extend_from_slice(&data);
        }
        if fragment != u32::MAX && out.len() < size as usize {
            let index = u64le(&self.superblock, 80) + (fragment as u64 / 512) * 8;
            let ptr = self.at(index, 8)?;
            let bytes = self.meta(u64le(&ptr, 0), fragment as usize % 512 * 16, 16)?;
            let frag_at = u64le(&bytes, 0);
            let block = u32le(&bytes, 8);
            let data = if let Some(data) = self.data_cache.get(&frag_at) {
                data.clone()
            } else {
                let input = self.at(frag_at, (block & 0x00ffffff) as usize)?;
                let data = if block & (1 << 24) != 0 {
                    input
                } else {
                    crate::xz::decode(&input, self.block_size)?
                };
                if self.data_cache.len() > 256 {
                    self.data_cache.clear();
                }
                self.data_cache.insert(frag_at, data.clone());
                data
            };
            let n = size as usize - out.len();
            out.extend_from_slice(
                data.get(offset as usize..offset as usize + n)
                    .ok_or("Invalid fragment offset")?,
            );
        }
        if out.len() != size as usize {
            return Err("SquashFS file length mismatch".into());
        }
        Ok(out)
    }
    pub fn paths(&mut self) -> Result<BTreeMap<String, Node>> {
        let root = self.inode(self.root_ref())?;
        let mut stack = vec![(String::new(), root)];
        let mut paths = BTreeMap::new();
        while let Some((path, node)) = stack.pop() {
            if paths.len() > 1_000_000 {
                return Err("SquashFS path limit exceeded".into());
            }
            if matches!(node.kind, Kind::Dir { .. }) {
                for entry in self.entries(&node)? {
                    let p = if path.is_empty() {
                        entry.name
                    } else {
                        format!("{path}/{}", entry.name)
                    };
                    stack.push((p, self.inode(entry.reference)?));
                }
            }
            paths.insert(path, node);
        }
        Ok(paths)
    }
    /// Preserve compressed file data, Linux metadata, hard links and xattrs.
    /// Rebuild canonical inode/directory/index tables using raw metadata blocks.
    pub fn overlay(
        &mut self,
        output: &Path,
        added: &BTreeMap<String, Vec<u8>>,
        removed: &BTreeSet<String>,
    ) -> Result<()> {
        for path in added.keys().chain(removed.iter()) {
            if path.is_empty()
                || path
                    .split('/')
                    .any(|s| s.is_empty() || s == ".." || s == "." || s.contains(['\\', '\0']))
            {
                return Err("Unsafe overlay path".into());
            }
        }
        let mut paths = self.paths()?;
        paths.retain(|path, _| {
            !removed
                .iter()
                .any(|p| path == p || path.starts_with(&format!("{p}/")))
        });
        let mut out = File::create(output).map_err(|e| e.to_string())?;
        let frag_count = u32le(&self.superblock, 16) as usize;
        let mut fragment_data = Vec::with_capacity(frag_count * 16);
        for p in (0..frag_count * 16).step_by(8192) {
            let ptr = self.at(u64le(&self.superblock, 80) + (p / 8192 * 8) as u64, 8)?;
            fragment_data.extend_from_slice(&self.meta(
                u64le(&ptr, 0),
                0,
                (frag_count * 16 - p).min(8192),
            )?);
        }
        let mut first = self.inode_start;
        for node in paths.values() {
            if let Kind::File {
                start, ref blocks, ..
            } = node.kind
            {
                if !blocks.is_empty() {
                    first = first.min(start);
                }
            }
        }
        for entry in fragment_data.chunks_exact(16) {
            first = first.min(u64le(entry, 0));
        }
        self.copy_extent(&mut out, 0, first)?;
        let mut extents = BTreeMap::new();
        for node in paths.values_mut() {
            if let Kind::File {
                ref mut start,
                ref blocks,
                ..
            } = node.kind
            {
                let length: u64 = blocks.iter().map(|b| (b & 0x00ffffff) as u64).sum();
                if length > 0 {
                    let key = (*start, length);
                    let relocated = if let Some(&at) = extents.get(&key) {
                        at
                    } else {
                        let at = out.stream_position().map_err(|e| e.to_string())?;
                        self.copy_extent(&mut out, *start, length)?;
                        extents.insert(key, at);
                        at
                    };
                    *start = relocated;
                    if u16le(&node.header, 0) == 2 {
                        node.raw[16..20].copy_from_slice(
                            &u32::try_from(relocated)
                                .map_err(|_| "Basic inode offset exceeds 4 GiB")?
                                .to_le_bytes(),
                        );
                    } else {
                        node.raw[16..24].copy_from_slice(&relocated.to_le_bytes());
                    }
                }
            }
        }
        // Fragment blocks can contain tails shared by many packages. Keep their
        // IDs and compressed payloads intact rather than risk breaking shared tails.
        for entry in fragment_data.chunks_exact_mut(16) {
            let old = u64le(entry, 0);
            let length = (u32le(entry, 8) & 0x00ffffff) as u64;
            let at = out.stream_position().map_err(|e| e.to_string())?;
            self.copy_extent(&mut out, old, length)?;
            entry[..8].copy_from_slice(&at.to_le_bytes());
        }
        let mut link_counts = BTreeMap::new();
        for node in paths.values() {
            *link_counts.entry(node.reference).or_insert(0usize) += 1;
        }
        let root = paths.get("").ok_or("Missing root")?.clone();
        let mut next = u32le(&self.superblock, 4);
        let mut synthetic = u64::MAX;
        for (path, data) in added {
            let mut prefix = String::new();
            let parts: Vec<_> = path.split('/').collect();
            for part in &parts[..parts.len() - 1] {
                if !prefix.is_empty() {
                    prefix.push('/');
                }
                prefix.push_str(part);
                if let Some(node) = paths.get(&prefix) {
                    if !matches!(node.kind, Kind::Dir { .. }) {
                        return Err(format!("Overlay ancestor is not a directory: {prefix}"));
                    }
                } else {
                    next += 1;
                    synthetic -= 1;
                    let mut h = root.header.clone();
                    h[12..16].copy_from_slice(&next.to_le_bytes());
                    let mut raw = h.clone();
                    raw.resize(40, 0);
                    raw[36..40].fill(0xff);
                    paths.insert(
                        prefix.clone(),
                        Node {
                            reference: synthetic,
                            header: h,
                            raw,
                            kind: Kind::Dir {
                                start: 0,
                                offset: 0,
                                size: 3,
                                parent: 0,
                            },
                        },
                    );
                }
            }
            let old = paths.get(path);
            if old.is_some_and(|n| matches!(n.kind, Kind::Dir { .. })) {
                return Err(format!("Cannot replace directory with file: {path}"));
            }
            let mut h = old.map(|n| n.header.clone()).unwrap_or_else(|| {
                next += 1;
                let mut h = root.header.clone();
                h[2..4].copy_from_slice(&0o644u16.to_le_bytes());
                h[12..16].copy_from_slice(&next.to_le_bytes());
                h
            });
            if old.is_some_and(|n| link_counts.get(&n.reference).copied().unwrap_or(0) > 1) {
                next += 1;
                h[12..16].copy_from_slice(&next.to_le_bytes());
            }
            h[..2].copy_from_slice(&9u16.to_le_bytes());
            let at = out.stream_position().map_err(|e| e.to_string())?;
            let mut blocks = Vec::new();
            for chunk in data.chunks(self.block_size) {
                let packed =
                    crate::xz::encode_with_dict(chunk, ((self.block_size.ilog2() - 12) * 2) as u8);
                if packed.len() < chunk.len() {
                    out.write_all(&packed).map_err(|e| e.to_string())?;
                    blocks.push(packed.len() as u32);
                } else {
                    out.write_all(chunk).map_err(|e| e.to_string())?;
                    blocks.push(chunk.len() as u32 | (1 << 24));
                }
            }
            let mut raw = h.clone();
            put64(&mut raw, at);
            put64(&mut raw, data.len() as u64);
            put64(&mut raw, 0);
            put32(&mut raw, 1);
            put32(&mut raw, u32::MAX);
            put32(&mut raw, 0);
            put32(
                &mut raw,
                old.filter(|n| u16le(&n.header, 0) == 9)
                    .map(|n| u32le(&n.raw, 52))
                    .unwrap_or(u32::MAX),
            );
            for &b in &blocks {
                put32(&mut raw, b);
            }
            synthetic -= 1;
            paths.insert(
                path.clone(),
                Node {
                    reference: synthetic,
                    header: h,
                    raw,
                    kind: Kind::File {
                        start: at,
                        size: data.len() as u64,
                        fragment: u32::MAX,
                        offset: 0,
                        blocks,
                    },
                },
            );
        }
        let mut final_links = BTreeMap::new();
        for node in paths.values() {
            *final_links.entry(node.reference).or_insert(0u32) += 1;
        }
        for node in paths.values_mut() {
            let kind = u16le(&node.header, 0);
            let count = final_links[&node.reference];
            if kind == 9 {
                node.raw[40..44].copy_from_slice(&count.to_le_bytes());
            } else if matches!(kind, 3..=7 | 10..=14) {
                node.raw[16..20].copy_from_slice(&count.to_le_bytes());
            }
        }
        let mut children: BTreeMap<String, Vec<String>> = BTreeMap::new();
        for path in paths.keys().filter(|p| !p.is_empty()) {
            let parent = path.rsplit_once('/').map(|p| p.0).unwrap_or("");
            children
                .entry(parent.into())
                .or_default()
                .push(path.clone());
        }
        let mut refs = BTreeMap::new();
        let mut inodes = MetaWriter::new();
        let mut directories = MetaWriter::new();
        for node in paths
            .values()
            .filter(|n| !matches!(n.kind, Kind::Dir { .. }))
        {
            if refs.contains_key(&node.reference) {
                continue;
            }
            let reference = inodes.reference();
            inodes.push(&node.raw);
            refs.insert(node.reference, reference);
        }
        let mut directory_paths: Vec<_> = paths
            .iter()
            .filter(|(_, n)| matches!(n.kind, Kind::Dir { .. }))
            .map(|(p, _)| p.clone())
            .collect();
        directory_paths.sort_by_key(|p| {
            std::cmp::Reverse((
                p.matches('/').count() + usize::from(!p.is_empty()),
                p.clone(),
            ))
        });
        for path in directory_paths {
            let node = &paths[&path];
            let entries = children.get(&path).cloned().unwrap_or_default();
            let mut subdirs = 0;
            let mut bytes = Vec::new();
            let directory_ref = directories.reference();
            for child_path in entries {
                let child = &paths[&child_path];
                let name = child_path.rsplit('/').next().unwrap();
                let reference = refs[&child.reference];
                let kind = if matches!(child.kind, Kind::Dir { .. }) {
                    subdirs += 1;
                    1
                } else {
                    let t = u16le(&child.header, 0);
                    if t > 7 {
                        t - 7
                    } else {
                        t
                    }
                };
                put32(&mut bytes, 0);
                put32(&mut bytes, (reference >> 16) as u32);
                put32(&mut bytes, u32le(&child.header, 12));
                put16(&mut bytes, reference as u16);
                put16(&mut bytes, 0);
                put16(&mut bytes, kind);
                put16(&mut bytes, (name.len() - 1) as u16);
                bytes.extend_from_slice(name.as_bytes());
            }
            directories.push(&bytes);
            let parent_path = path.rsplit_once('/').map(|p| p.0).unwrap_or("");
            let parent = paths.get(parent_path).unwrap_or(&root);
            let mut raw = node.header.clone();
            raw[..2].copy_from_slice(&8u16.to_le_bytes());
            put32(&mut raw, 2 + subdirs);
            put32(&mut raw, bytes.len() as u32 + 3);
            put32(&mut raw, (directory_ref >> 16) as u32);
            put32(&mut raw, u32le(&parent.header, 12));
            put16(&mut raw, 0);
            put16(&mut raw, directory_ref as u16);
            put32(
                &mut raw,
                if u16le(&node.header, 0) == 8 {
                    u32le(&node.raw, 36)
                } else {
                    u32::MAX
                },
            );
            let reference = inodes.reference();
            inodes.push(&raw);
            refs.insert(node.reference, reference);
        }
        let inode_start = out.stream_position().map_err(|e| e.to_string())?;
        out.write_all(&inodes.finish()).map_err(|e| e.to_string())?;
        let dir_start = out.stream_position().map_err(|e| e.to_string())?;
        out.write_all(&directories.finish())
            .map_err(|e| e.to_string())?;
        let frag_index = if frag_count > 0 {
            write_indexed(&mut out, &fragment_data)?
        } else {
            u64::MAX
        };
        let export_index = if u16le(&self.superblock, 24) & (1 << 7) != 0 {
            let mut data = vec![255u8; next as usize * 8];
            for node in paths.values() {
                let number = u32le(&node.header, 12) as usize;
                if number > 0 && number <= next as usize {
                    data[(number - 1) * 8..number * 8]
                        .copy_from_slice(&refs[&node.reference].to_le_bytes());
                }
            }
            write_indexed(&mut out, &data)?
        } else {
            u64::MAX
        };
        let ids = u16le(&self.superblock, 26) as usize;
        let id_index = self.copy_indexed(&mut out, u64le(&self.superblock, 48), ids * 4)?;
        let old_xattr = u64le(&self.superblock, 56);
        let mut xattr_index = u64::MAX;
        if old_xattr != u64::MAX {
            let header = self.at(old_xattr, 16)?;
            let old_start = u64le(&header, 0);
            let ids = u32le(&header, 8) as usize;
            let index_len = (ids * 16).div_ceil(8192) * 8;
            let new_start = out.stream_position().map_err(|e| e.to_string())?;
            let mut blob = self.at(old_start, (self.used - old_start) as usize)?;
            let at = (old_xattr - old_start) as usize;
            blob[at..at + 8].copy_from_slice(&new_start.to_le_bytes());
            for p in (at + 16..at + 16 + index_len).step_by(8) {
                let old = u64le(&blob, p);
                blob[p..p + 8].copy_from_slice(&(new_start + (old - old_start)).to_le_bytes());
            }
            out.write_all(&blob).map_err(|e| e.to_string())?;
            xattr_index = new_start + (old_xattr - old_start);
        }
        let used = out.stream_position().map_err(|e| e.to_string())?;
        let mut sb = self.superblock.clone();
        sb[4..8].copy_from_slice(&next.to_le_bytes());
        sb[32..40].copy_from_slice(&refs[&root.reference].to_le_bytes());
        sb[40..48].copy_from_slice(&used.to_le_bytes());
        sb[48..56].copy_from_slice(&id_index.to_le_bytes());
        sb[56..64].copy_from_slice(&xattr_index.to_le_bytes());
        sb[64..72].copy_from_slice(&inode_start.to_le_bytes());
        sb[72..80].copy_from_slice(&dir_start.to_le_bytes());
        sb[80..88].copy_from_slice(&frag_index.to_le_bytes());
        sb[88..96].copy_from_slice(&export_index.to_le_bytes());
        out.seek(SeekFrom::Start(0)).map_err(|e| e.to_string())?;
        out.write_all(&sb).map_err(|e| e.to_string())?;
        out.set_len(used.div_ceil(4096) * 4096)
            .map_err(|e| e.to_string())?;
        Ok(())
    }
    fn copy_extent(&mut self, out: &mut File, position: u64, length: u64) -> Result<()> {
        if position
            .checked_add(length)
            .is_none_or(|end| end > self.used)
        {
            return Err("Invalid data extent".into());
        }
        self.file
            .seek(SeekFrom::Start(self.base + position))
            .map_err(|e| e.to_string())?;
        let n = std::io::copy(&mut std::io::Read::by_ref(&mut self.file).take(length), out)
            .map_err(|e| e.to_string())?;
        if n != length {
            return Err("Truncated source data".into());
        }
        Ok(())
    }
    fn copy_indexed(&mut self, out: &mut File, table: u64, length: usize) -> Result<u64> {
        let mut data = Vec::with_capacity(length);
        for p in (0..length).step_by(8192) {
            let ptr = self.at(table + (p / 8192 * 8) as u64, 8)?;
            data.extend_from_slice(&self.meta(u64le(&ptr, 0), 0, (length - p).min(8192))?);
        }
        write_indexed(out, &data)
    }
}
struct MetaWriter {
    output: Vec<u8>,
    buffer: Vec<u8>,
}
impl MetaWriter {
    fn new() -> Self {
        Self {
            output: Vec::new(),
            buffer: Vec::new(),
        }
    }
    fn reference(&self) -> u64 {
        ((self.output.len() as u64) << 16) | self.buffer.len() as u64
    }
    fn push(&mut self, mut bytes: &[u8]) {
        while !bytes.is_empty() {
            let n = bytes.len().min(8192 - self.buffer.len());
            self.buffer.extend_from_slice(&bytes[..n]);
            bytes = &bytes[n..];
            if self.buffer.len() == 8192 {
                self.flush();
            }
        }
    }
    fn flush(&mut self) {
        if self.buffer.is_empty() {
            return;
        }
        let packed = crate::xz::encode_with_dict(&self.buffer, 2);
        if packed.len() < self.buffer.len() {
            self.output
                .extend_from_slice(&(packed.len() as u16).to_le_bytes());
            self.output.extend_from_slice(&packed);
        } else {
            self.output
                .extend_from_slice(&(self.buffer.len() as u16 | 0x8000).to_le_bytes());
            self.output.extend_from_slice(&self.buffer);
        }
        self.buffer.clear();
    }
    fn finish(mut self) -> Vec<u8> {
        self.flush();
        self.output
    }
}
fn write_indexed(out: &mut File, data: &[u8]) -> Result<u64> {
    let mut positions = Vec::new();
    for block in data.chunks(8192) {
        positions.push(out.stream_position().map_err(|e| e.to_string())?);
        let mut meta = MetaWriter::new();
        meta.push(block);
        out.write_all(&meta.finish()).map_err(|e| e.to_string())?;
    }
    let at = out.stream_position().map_err(|e| e.to_string())?;
    for pos in positions {
        out.write_all(&pos.to_le_bytes())
            .map_err(|e| e.to_string())?;
    }
    Ok(at)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn fixture() -> Vec<u8> {
        let mut image = vec![0u8; 96];
        image[..4].copy_from_slice(b"hsqs");
        image[4..8].copy_from_slice(&2u32.to_le_bytes());
        image[12..16].copy_from_slice(&4096u32.to_le_bytes());
        image[20..22].copy_from_slice(&4u16.to_le_bytes());
        image[22..24].copy_from_slice(&12u16.to_le_bytes());
        image[26..28].copy_from_slice(&1u16.to_le_bytes());
        image[28..30].copy_from_slice(&4u16.to_le_bytes());
        image[56..64].fill(255);
        image[80..96].fill(255);
        image.extend_from_slice(b"hello");
        let inode_start = image.len() as u64;
        let mut file = Vec::new();
        put16(&mut file, 9);
        put16(&mut file, 0o640);
        put16(&mut file, 0);
        put16(&mut file, 0);
        put32(&mut file, 123);
        put32(&mut file, 1);
        put64(&mut file, 96);
        put64(&mut file, 5);
        put64(&mut file, 0);
        put32(&mut file, 2);
        put32(&mut file, u32::MAX);
        put32(&mut file, 0);
        put32(&mut file, u32::MAX);
        put32(&mut file, 5 | (1 << 24));
        let root_ref = file.len() as u64;
        let mut directory = Vec::new();
        put32(&mut directory, 1);
        put32(&mut directory, 0);
        put32(&mut directory, 1);
        for name in [b'a', b'b'] {
            put16(&mut directory, 0);
            put16(&mut directory, 0);
            put16(&mut directory, 2);
            put16(&mut directory, 0);
            directory.push(name);
        }
        put16(&mut file, 1);
        put16(&mut file, 0o755);
        put16(&mut file, 0);
        put16(&mut file, 0);
        put32(&mut file, 123);
        put32(&mut file, 2);
        put32(&mut file, 0);
        put32(&mut file, 2);
        put16(&mut file, (directory.len() + 3) as u16);
        put16(&mut file, 0);
        put32(&mut file, 2);
        put16(&mut image, file.len() as u16 | 0x8000);
        image.extend_from_slice(&file);
        let dir_start = image.len() as u64;
        put16(&mut image, directory.len() as u16 | 0x8000);
        image.extend_from_slice(&directory);
        let id_block = image.len() as u64;
        put16(&mut image, 0x8004);
        put32(&mut image, 0);
        let id_index = image.len() as u64;
        put64(&mut image, id_block);
        let used = image.len() as u64;
        image[32..40].copy_from_slice(&root_ref.to_le_bytes());
        image[40..48].copy_from_slice(&used.to_le_bytes());
        image[48..56].copy_from_slice(&id_index.to_le_bytes());
        image[64..72].copy_from_slice(&inode_start.to_le_bytes());
        image[72..80].copy_from_slice(&dir_start.to_le_bytes());
        image
    }
    #[test]
    fn roundtrip_keeps_links_permissions_and_nested_additions() {
        let folder = std::env::temp_dir().join(format!("isolite-squash-{}", std::process::id()));
        std::fs::create_dir_all(&folder).unwrap();
        let input = folder.join("in.sfs");
        let output = folder.join("out.sfs");
        std::fs::write(&input, fixture()).unwrap();
        let mut source = Squash::open(&input, 0).unwrap();
        assert_eq!(source.read("a").unwrap(), b"hello");
        let original = source.superblock.clone();
        let added = [("etc/default/locale".into(), b"LANG=vi_VN.UTF-8\n".to_vec())]
            .into_iter()
            .collect();
        source.overlay(&output, &added, &BTreeSet::new()).unwrap();
        let mut result = Squash::open(&output, 0).unwrap();
        assert_eq!(&result.superblock[12..32], &original[12..32]);
        assert_eq!(result.read("a").unwrap(), b"hello");
        assert_eq!(
            result.find("a").unwrap().reference,
            result.find("b").unwrap().reference
        );
        assert_eq!(u16le(&result.find("a").unwrap().header, 2), 0o640);
        assert_eq!(
            result.read("etc/default/locale").unwrap(),
            b"LANG=vi_VN.UTF-8\n"
        );
        drop(result);
        source
            .overlay(
                &output,
                &BTreeMap::new(),
                &["a".to_owned()].into_iter().collect(),
            )
            .unwrap();
        let mut result = Squash::open(&output, 0).unwrap();
        assert!(result.find("a").is_err());
        assert_eq!(result.read("b").unwrap(), b"hello");
        assert_eq!(u32le(&result.find("b").unwrap().raw, 40), 1);
        drop(result);
        source
            .overlay(
                &output,
                &[("a".to_owned(), b"new".to_vec())].into_iter().collect(),
                &BTreeSet::new(),
            )
            .unwrap();
        let mut result = Squash::open(&output, 0).unwrap();
        assert_eq!(result.read("a").unwrap(), b"new");
        assert_eq!(result.read("b").unwrap(), b"hello");
        assert_ne!(
            u32le(&result.find("a").unwrap().header, 12),
            u32le(&result.find("b").unwrap().header, 12)
        );
        assert_eq!(u32le(&result.find("b").unwrap().raw, 40), 1);
        drop(result);
        drop(source);
        std::fs::remove_file(input).unwrap();
        std::fs::remove_file(output).unwrap();
        std::fs::remove_dir(folder).unwrap();
    }
}
