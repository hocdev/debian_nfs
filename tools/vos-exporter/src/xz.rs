// XZ/LZMA2 decoder for SquashFS blocks. Written using std only.
// Format: https://tukaani.org/xz/xz-file-format.txt
type Result<T> = std::result::Result<T, String>;
const LEN: usize = 818;
const REP_LEN: usize = 1332;
const LIT: usize = 1846;

pub struct Hasher(u32);
impl Hasher {
    pub fn new() -> Self {
        Self(!0)
    }
    pub fn update(&mut self, bytes: &[u8]) {
        for &byte in bytes {
            self.0 ^= byte as u32;
            for _ in 0..8 {
                self.0 = (self.0 >> 1) ^ (0xedb88320 & (0u32.wrapping_sub(self.0 & 1)));
            }
        }
    }
    pub fn finalize(self) -> u32 {
        !self.0
    }
}
pub fn crc32(bytes: &[u8]) -> u32 {
    let mut h = Hasher::new();
    h.update(bytes);
    h.finalize()
}
fn crc64(bytes: &[u8]) -> u64 {
    let mut crc = !0u64;
    for &b in bytes {
        crc ^= b as u64;
        for _ in 0..8 {
            crc = (crc >> 1) ^ (0xc96c5795d7870f42 & (0u64.wrapping_sub(crc & 1)));
        }
    }
    !crc
}
struct Range<'a> {
    input: &'a [u8],
    pos: usize,
    range: u32,
    code: u32,
}
impl<'a> Range<'a> {
    fn new(input: &'a [u8]) -> Result<Self> {
        if input.len() < 5 || input[0] != 0 {
            return Err("Invalid LZMA range header".into());
        }
        Ok(Self {
            input,
            pos: 5,
            range: !0,
            code: u32::from_be_bytes(input[1..5].try_into().unwrap()),
        })
    }
    fn norm(&mut self) -> Result<()> {
        if self.range < 1 << 24 {
            self.range <<= 8;
            self.code = (self.code << 8)
                | *self
                    .input
                    .get(self.pos)
                    .ok_or("Truncated LZMA range stream")? as u32;
            self.pos += 1;
        }
        Ok(())
    }
    fn bit(&mut self, probs: &mut [u16], index: usize) -> Result<u32> {
        let p = probs
            .get_mut(index)
            .ok_or("Invalid LZMA probability index")?;
        let bound = (self.range >> 11) * *p as u32;
        let bit = if self.code < bound {
            self.range = bound;
            *p += (2048 - *p) >> 5;
            0
        } else {
            self.range -= bound;
            self.code -= bound;
            *p -= *p >> 5;
            1
        };
        self.norm()?;
        Ok(bit)
    }
    fn tree(&mut self, probs: &mut [u16], start: usize, bits: u32, reverse: bool) -> Result<u32> {
        let mut symbol = 1;
        let mut result = 0;
        for i in 0..bits {
            let b = self.bit(probs, start + symbol as usize)?;
            symbol = (symbol << 1) | b;
            result |= b << i;
        }
        Ok(if reverse {
            result
        } else {
            symbol - (1 << bits)
        })
    }
    fn direct(&mut self, bits: u32) -> Result<u32> {
        let mut value = 0;
        for _ in 0..bits {
            self.range >>= 1;
            let b = u32::from(self.code >= self.range);
            if b != 0 {
                self.code -= self.range;
            }
            value = (value << 1) | b;
            self.norm()?;
        }
        Ok(value)
    }
}
struct Lzma {
    probs: Vec<u16>,
    lc: u32,
    lp: u32,
    pb: u32,
    state: usize,
    reps: [usize; 4],
}
impl Lzma {
    fn new(prop: u8) -> Result<Self> {
        let lc = prop as u32 % 9;
        let rest = prop as u32 / 9;
        let lp = rest % 5;
        let pb = rest / 5;
        if pb > 4 || lc + lp > 4 {
            return Err("Unsupported LZMA properties".into());
        }
        Ok(Self {
            probs: vec![1024; LIT + (768 << (lc + lp))],
            lc,
            lp,
            pb,
            state: 0,
            reps: [0; 4],
        })
    }
    fn reset(&mut self) {
        self.probs.fill(1024);
        self.state = 0;
        self.reps = [0; 4];
    }
    fn length(&mut self, r: &mut Range<'_>, start: usize, pos: usize) -> Result<usize> {
        Ok(2 + if r.bit(&mut self.probs, start)? == 0 {
            r.tree(&mut self.probs, start + 2 + pos * 8, 3, false)?
        } else if r.bit(&mut self.probs, start + 1)? == 0 {
            8 + r.tree(&mut self.probs, start + 130 + pos * 8, 3, false)?
        } else {
            16 + r.tree(&mut self.probs, start + 258, 8, false)?
        } as usize)
    }
    fn decode(
        &mut self,
        input: &[u8],
        output: &mut Vec<u8>,
        count: usize,
        dict_start: usize,
    ) -> Result<()> {
        let mut r = Range::new(input)?;
        let end = output
            .len()
            .checked_add(count)
            .ok_or("LZMA length overflow")?;
        while output.len() < end {
            let position = output.len() - dict_start;
            let ps = position & ((1 << self.pb) - 1);
            if r.bit(&mut self.probs, self.state * 16 + ps)? == 0 {
                let prev = if position == 0 {
                    0
                } else {
                    output[output.len() - 1] as usize
                };
                let ctx = ((position & ((1 << self.lp) - 1)) << self.lc) + (prev >> (8 - self.lc));
                let start = LIT + 768 * ctx;
                let mut symbol = 1usize;
                if self.state >= 7 {
                    let at = output
                        .len()
                        .checked_sub(self.reps[0] + 1)
                        .filter(|&p| p >= dict_start)
                        .ok_or("Invalid LZMA literal distance")?;
                    let mut matched = output[at];
                    loop {
                        let mb = (matched >> 7) as usize;
                        matched <<= 1;
                        let b = r.bit(&mut self.probs, start + ((1 + mb) << 8) + symbol)? as usize;
                        symbol = (symbol << 1) | b;
                        if mb != b || symbol >= 256 {
                            break;
                        }
                    }
                }
                while symbol < 256 {
                    symbol = (symbol << 1) | r.bit(&mut self.probs, start + symbol)? as usize;
                }
                output.push(symbol as u8);
                self.state = if self.state < 4 {
                    0
                } else if self.state < 10 {
                    self.state - 3
                } else {
                    self.state - 6
                };
                continue;
            }
            let length;
            if r.bit(&mut self.probs, 192 + self.state)? != 0 {
                if r.bit(&mut self.probs, 204 + self.state)? == 0 {
                    if r.bit(&mut self.probs, 240 + self.state * 16 + ps)? == 0 {
                        self.state = if self.state < 7 { 9 } else { 11 };
                        let at = output
                            .len()
                            .checked_sub(self.reps[0] + 1)
                            .filter(|&p| p >= dict_start)
                            .ok_or("Invalid LZMA short distance")?;
                        output.push(output[at]);
                        continue;
                    }
                } else {
                    let dist = if r.bit(&mut self.probs, 216 + self.state)? == 0 {
                        self.reps[1]
                    } else {
                        let d = if r.bit(&mut self.probs, 228 + self.state)? == 0 {
                            self.reps[2]
                        } else {
                            let d = self.reps[3];
                            self.reps[3] = self.reps[2];
                            d
                        };
                        self.reps[2] = self.reps[1];
                        d
                    };
                    self.reps[1] = self.reps[0];
                    self.reps[0] = dist;
                }
                length = self.length(&mut r, REP_LEN, ps)?;
                self.state = if self.state < 7 { 8 } else { 11 };
            } else {
                length = self.length(&mut r, LEN, ps)?;
                self.state = if self.state < 7 { 7 } else { 10 };
                let slot = r.tree(&mut self.probs, 432 + (length - 2).min(3) * 64, 6, false)?;
                let dist = if slot < 4 {
                    slot
                } else {
                    let bits = (slot >> 1) - 1;
                    let base = (2 | (slot & 1)) << bits;
                    if slot < 14 {
                        base + r.tree(
                            &mut self.probs,
                            (688 + base - slot - 1) as usize,
                            bits,
                            true,
                        )?
                    } else {
                        base + (r.direct(bits - 4)? << 4) + r.tree(&mut self.probs, 802, 4, true)?
                    }
                };
                self.reps = [dist as usize, self.reps[0], self.reps[1], self.reps[2]];
            }
            if output.len() + length > end {
                return Err("LZMA output exceeds declared size".into());
            }
            for _ in 0..length {
                let at = output
                    .len()
                    .checked_sub(self.reps[0] + 1)
                    .filter(|&p| p >= dict_start)
                    .ok_or("Invalid LZMA match distance")?;
                output.push(output[at]);
            }
        }
        Ok(())
    }
}
fn byte(input: &[u8], pos: &mut usize) -> Result<u8> {
    let b = *input.get(*pos).ok_or("Truncated XZ stream")?;
    *pos += 1;
    Ok(b)
}
fn varint(input: &[u8], pos: &mut usize) -> Result<u64> {
    let mut value = 0;
    for shift in (0..63).step_by(7) {
        let b = byte(input, pos)?;
        value |= ((b & 127) as u64) << shift;
        if b < 128 {
            return Ok(value);
        }
    }
    Err("Invalid XZ integer".into())
}
pub fn decode(input: &[u8], limit: usize) -> Result<Vec<u8>> {
    if input.len() < 24
        || &input[..6] != b"\xfd7zXZ\0"
        || input[6] != 0
        || crc32(&input[6..8]) != u32::from_le_bytes(input[8..12].try_into().unwrap())
    {
        return Err("Invalid XZ header".into());
    }
    let check = input[7];
    if !matches!(check, 0 | 1 | 4) {
        return Err("Unsupported XZ checksum".into());
    }
    let mut pos = 12;
    let mut output = Vec::new();
    while *input.get(pos).ok_or("Missing XZ index")? != 0 {
        let start = pos;
        let hlen = (byte(input, &mut pos)? as usize + 1) * 4;
        let header = input
            .get(start..start + hlen)
            .ok_or("Truncated XZ block header")?;
        if crc32(&header[..hlen - 4]) != u32::from_le_bytes(header[hlen - 4..].try_into().unwrap())
        {
            return Err("Invalid XZ block header checksum".into());
        }
        let flags = byte(input, &mut pos)?;
        if flags & 0x3f != 0 {
            return Err("XZ filter chains other than LZMA2 are unsupported".into());
        }
        if flags & 0x40 != 0 {
            varint(input, &mut pos)?;
        }
        let declared = if flags & 0x80 != 0 {
            Some(varint(input, &mut pos)?)
        } else {
            None
        };
        if varint(input, &mut pos)? != 0x21 || varint(input, &mut pos)? != 1 {
            return Err("XZ requires a single LZMA2 filter".into());
        }
        if byte(input, &mut pos)? > 40 || pos > start + hlen - 4 {
            return Err("Invalid LZMA2 dictionary header".into());
        }
        pos = start + hlen;
        let data_start = pos;
        let out_start = output.len();
        let mut dict_start = out_start;
        let mut model: Option<Lzma> = None;
        loop {
            let ctrl = byte(input, &mut pos)?;
            if ctrl == 0 {
                break;
            }
            let n = ((byte(input, &mut pos)? as usize) << 8) | byte(input, &mut pos)? as usize;
            if ctrl <= 2 {
                if ctrl == 1 {
                    dict_start = output.len();
                }
                if output.len() + n + 1 > limit {
                    return Err("XZ output limit exceeded".into());
                }
                output.extend_from_slice(
                    input
                        .get(pos..pos + n + 1)
                        .ok_or("Truncated LZMA2 raw chunk")?,
                );
                pos += n + 1;
            } else if ctrl >= 0x80 {
                let count = (((ctrl & 31) as usize) << 16) + n + 1;
                let packed = (((byte(input, &mut pos)? as usize) << 8)
                    | byte(input, &mut pos)? as usize)
                    + 1;
                if ctrl >= 0xe0 {
                    dict_start = output.len();
                }
                if ctrl >= 0xc0 {
                    model = Some(Lzma::new(byte(input, &mut pos)?)?);
                }
                let decoder = model.as_mut().ok_or("Missing LZMA2 properties")?;
                if ctrl >= 0xa0 {
                    decoder.reset();
                }
                if output.len() + count > limit {
                    return Err("XZ output limit exceeded".into());
                }
                decoder.decode(
                    input
                        .get(pos..pos + packed)
                        .ok_or("Truncated LZMA2 compressed chunk")?,
                    &mut output,
                    count,
                    dict_start,
                )?;
                pos += packed;
            } else {
                return Err("Reserved LZMA2 control byte".into());
            }
        }
        if declared.is_some_and(|n| n != (output.len() - out_start) as u64) {
            return Err("XZ uncompressed size mismatch".into());
        }
        while (pos - data_start) % 4 != 0 {
            if byte(input, &mut pos)? != 0 {
                return Err("Invalid XZ padding".into());
            }
        }
        let bytes = &output[out_start..];
        let check_len = match check {
            1 => 4,
            4 => 8,
            _ => 0,
        };
        let stored = input
            .get(pos..pos + check_len)
            .ok_or("Missing XZ checksum")?;
        if (check == 1 && stored != crc32(bytes).to_le_bytes())
            || (check == 4 && stored != crc64(bytes).to_le_bytes())
        {
            return Err("XZ data checksum mismatch".into());
        }
        pos += check_len;
    }
    let footer = &input[input.len() - 12..];
    if &footer[10..] != b"YZ"
        || crc32(&footer[4..10]) != u32::from_le_bytes(footer[..4].try_into().unwrap())
    {
        return Err("Invalid XZ footer".into());
    }
    Ok(output)
}

struct Encoder {
    low: u64,
    range: u32,
    cache: u8,
    cache_size: usize,
    output: Vec<u8>,
}
impl Encoder {
    fn new() -> Self {
        Self {
            low: 0,
            range: !0,
            cache: 0,
            cache_size: 1,
            output: Vec::new(),
        }
    }
    fn shift(&mut self) {
        let low = self.low as u32;
        if low < 0xff000000 || self.low >> 32 != 0 {
            let carry = (self.low >> 32) as u8;
            let mut byte = self.cache;
            loop {
                self.output.push(byte.wrapping_add(carry));
                byte = 255;
                self.cache_size -= 1;
                if self.cache_size == 0 {
                    break;
                }
            }
            self.cache = (low >> 24) as u8;
        }
        self.cache_size += 1;
        self.low = (low << 8) as u64;
    }
    fn bit(&mut self, p: &mut [u16], i: usize, b: u32) {
        let bound = (self.range >> 11) * p[i] as u32;
        if b == 0 {
            self.range = bound;
            p[i] += (2048 - p[i]) >> 5;
        } else {
            self.low += bound as u64;
            self.range -= bound;
            p[i] -= p[i] >> 5;
        }
        if self.range < 1 << 24 {
            self.range <<= 8;
            self.shift();
        }
    }
    fn tree(&mut self, p: &mut [u16], start: usize, bits: u32, value: u32, reverse: bool) {
        let mut symbol = 1usize;
        for n in 0..bits {
            let b = (value >> if reverse { n } else { bits - 1 - n }) & 1;
            self.bit(p, start + symbol, b);
            symbol = (symbol << 1) | b as usize;
        }
    }
    fn finish(mut self) -> Vec<u8> {
        for _ in 0..5 {
            self.shift();
        }
        self.output
    }
}
fn encode_length(r: &mut Encoder, p: &mut [u16], length: usize, pos: usize) {
    let value = length - 2;
    if value < 8 {
        r.bit(p, LEN, 0);
        r.tree(p, LEN + 2 + pos * 8, 3, value as u32, false);
    } else {
        r.bit(p, LEN, 1);
        if value < 16 {
            r.bit(p, LEN + 1, 0);
            r.tree(p, LEN + 130 + pos * 8, 3, (value - 8) as u32, false);
        } else {
            r.bit(p, LEN + 1, 1);
            r.tree(p, LEN + 258, 8, (value - 16) as u32, false);
        }
    }
}
fn encode_lzma(input: &[u8]) -> Vec<u8> {
    let mut r = Encoder::new();
    let mut probs = vec![1024u16; LIT + 768 * 8];
    let mut state = 0usize;
    let mut rep = 0usize;
    let mut at = 0;
    let mut hashes: std::collections::BTreeMap<[u8; 3], Vec<usize>> =
        std::collections::BTreeMap::new();
    while at < input.len() {
        let mut length = 0usize;
        let mut distance = 0usize;
        if at + 3 <= input.len() {
            let key: [u8; 3] = input[at..at + 3].try_into().unwrap();
            if let Some(candidates) = hashes.get(&key) {
                for &prev in candidates.iter().rev().take(24) {
                    let mut n = 3;
                    while n < 273 && at + n < input.len() && input[prev + n] == input[at + n] {
                        n += 1;
                    }
                    if n > length {
                        length = n;
                        distance = at - prev - 1;
                    }
                }
            }
        }
        let ps = at & 3;
        let count;
        if length >= 3 {
            r.bit(&mut probs, state * 16 + ps, 1);
            r.bit(&mut probs, 192 + state, 0);
            encode_length(&mut r, &mut probs, length, ps);
            let slot = if distance < 4 {
                distance as u32
            } else {
                let bits = usize::BITS - 1 - distance.leading_zeros();
                bits * 2 + ((distance >> (bits - 1)) & 1) as u32
            };
            r.tree(&mut probs, 432 + (length - 2).min(3) * 64, 6, slot, false);
            if slot >= 4 {
                let bits = (slot >> 1) - 1;
                let base = (2 | (slot & 1)) << bits;
                let remainder = distance as u32 - base;
                if slot < 14 {
                    r.tree(
                        &mut probs,
                        (688 + base - slot - 1) as usize,
                        bits,
                        remainder,
                        true,
                    );
                } else {
                    for bit in (4..bits).rev() {
                        r.range >>= 1;
                        if remainder >> bit & 1 != 0 {
                            r.low += r.range as u64;
                        }
                        if r.range < 1 << 24 {
                            r.range <<= 8;
                            r.shift();
                        }
                    }
                    r.tree(&mut probs, 802, 4, remainder & 15, true);
                }
            }
            state = if state < 7 { 7 } else { 10 };
            rep = distance;
            count = length;
        } else {
            r.bit(&mut probs, state * 16 + ps, 0);
            let prev = if at == 0 { 0 } else { input[at - 1] as usize };
            let start = LIT + 768 * (prev >> 5);
            let value = input[at];
            let mut symbol = 1usize;
            let mut matched = if state >= 7 { input[at - rep - 1] } else { 0 };
            let mut matching = state >= 7;
            for bit in (0..8).rev() {
                let b = (value >> bit) as u32 & 1;
                if matching {
                    let mb = matched >> 7;
                    matched <<= 1;
                    r.bit(&mut probs, start + ((1 + mb as usize) << 8) + symbol, b);
                    matching = mb as u32 == b;
                } else {
                    r.bit(&mut probs, start + symbol, b);
                }
                symbol = (symbol << 1) | b as usize;
            }
            state = if state < 4 {
                0
            } else if state < 10 {
                state - 3
            } else {
                state - 6
            };
            count = 1;
        }
        for p in at..at + count {
            if p + 3 <= input.len() {
                let key = input[p..p + 3].try_into().unwrap();
                let candidates = hashes.entry(key).or_default();
                if candidates.len() >= 48 {
                    candidates.drain(..24);
                }
                candidates.push(p);
            }
        }
        at += count;
    }
    r.finish()
}
fn put_varint(out: &mut Vec<u8>, mut n: u64) {
    loop {
        let byte = (n & 127) as u8;
        n >>= 7;
        out.push(byte | if n == 0 { 0 } else { 128 });
        if n == 0 {
            break;
        }
    }
}
/// XZ stream with LZMA2, a bounded dictionary and CRC32, using no native libraries.
pub fn encode(input: &[u8]) -> Vec<u8> {
    encode_with_dict(input, 10)
}
pub fn encode_with_dict(input: &[u8], dictionary_property: u8) -> Vec<u8> {
    let mut out = b"\xfd7zXZ\0\0\x01".to_vec();
    out.extend_from_slice(&crc32(&out[6..8]).to_le_bytes());
    let mut header = vec![2, 0, 0x21, 1, dictionary_property];
    while header.len() < 8 {
        header.push(0);
    }
    header.extend_from_slice(&crc32(&header).to_le_bytes());
    out.extend_from_slice(&header);
    let start = out.len();
    for chunk in input.chunks(32768) {
        let packed = encode_lzma(chunk);
        let n = chunk.len() - 1;
        if packed.len() + 6 < chunk.len() + 3 {
            out.push(0xe0 | ((n >> 16) as u8));
            out.extend_from_slice(&(n as u16).to_be_bytes());
            out.extend_from_slice(&((packed.len() - 1) as u16).to_be_bytes());
            out.push(0x5d);
            out.extend_from_slice(&packed);
        } else {
            out.push(1);
            out.extend_from_slice(&(n as u16).to_be_bytes());
            out.extend_from_slice(chunk);
        }
    }
    out.push(0);
    let packed = out.len() - start;
    while out.len() % 4 != 0 {
        out.push(0);
    }
    out.extend_from_slice(&crc32(input).to_le_bytes());
    let mut index = vec![0, 1];
    put_varint(&mut index, (header.len() + packed + 4) as u64);
    put_varint(&mut index, input.len() as u64);
    while index.len() % 4 != 0 {
        index.push(0);
    }
    index.extend_from_slice(&crc32(&index).to_le_bytes());
    let backward = (index.len() / 4 - 1) as u32;
    out.extend_from_slice(&index);
    let mut footer = backward.to_le_bytes().to_vec();
    footer.extend_from_slice(&[0, 1]);
    out.extend_from_slice(&crc32(&footer).to_le_bytes());
    out.extend_from_slice(&footer);
    out.extend_from_slice(b"YZ");
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn independent_xz_fixture() {
        assert_eq!(
            decode(include_bytes!("fixtures/hello.xz"), 200000).unwrap(),
            b"hello world".repeat(10000)
        );
    }
    #[test]
    fn encoder_match_literal_and_raw_chunks() {
        let mut random = 17u32;
        let input: Vec<u8> = (0..140000)
            .map(|i| {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                if i < 70000 {
                    (i % 19) as u8
                } else {
                    random as u8
                }
            })
            .collect();
        let encoded = encode(&input);
        assert_eq!(decode(&encoded, input.len()).unwrap(), input);
        assert_eq!(decode(&encode(&[]), 0).unwrap(), Vec::<u8>::new());
    }
    #[test]
    fn rejects_corruption_and_bombs() {
        let mut bytes = include_bytes!("fixtures/hello.xz").to_vec();
        assert!(decode(&bytes, 100).is_err());
        bytes[8] ^= 1;
        assert!(decode(&bytes, 200000).is_err());
        assert!(decode(&[0; 10], 8192).is_err());
    }
    #[test]
    fn checksums() {
        assert_eq!(crc32(b"123456789"), 0xcbf43926);
        assert_eq!(crc64(b"123456789"), 0x995dc9bbdf1939fa);
    }
}
