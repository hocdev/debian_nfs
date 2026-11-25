#![allow(dead_code)]
mod iso_reader;
mod squash;
mod xz;

use iso_reader::IsoReader;
use squash::native::{Kind, Node, Squash};
use std::collections::{BTreeMap, BTreeSet};
use std::fs::{self, File};
use std::io::Write;
use std::path::{Path, PathBuf};

type Result<T> = std::result::Result<T, String>;

const EXPORT_PACKAGES: &[&str] = &[
    "found-desktop",
    "found-desktop-common",
    "found-desktop-environment",
    "found-session",
    "found-session-common",
    "found-settings-daemon",
    "found-control-center",
    "found-control-center-data",
    "found-gsettings-schemas",
    "found-themes",
    "found-icon-theme",
    "found-backgrounds",
    "found-tools",
    "found-ui-scaling",
    "found-livecd-customization",
    "libfound-desktop-4",
    "libfound-desktop-data",
    "libfound-gtk3",
    "libfound-gtk-data",
    "nemo",
    "nemo-data",
    "libnemo-extension1",
    "gir1.2-nemo-3.0",
    "nemo-print",
    "nemo-share",
    "libxapp1",
    "xapps-common",
    "gir1.2-xapp-1.0",
    "mutter",
    "mutter-common",
    "libmutter-6-0",
    "gir1.2-mutter-6",
    "nfs-custom-jjw",
    "cdos-plymouth-theme-nfslogo",
];

const COMPAT_SOURCES: &[(&str, &[&str])] = &[
    ("libcroco3", &["usr/lib/x86_64-linux-gnu/libcroco-0.6.so.3"]),
    (
        "libgnome-desktop-3-19",
        &["usr/lib/x86_64-linux-gnu/libgnome-desktop-3.so.19"],
    ),
    (
        "libtracker-sparql-2.0-0",
        &[
            "usr/lib/x86_64-linux-gnu/libtracker-sparql-2.0.so.0",
            "usr/lib/x86_64-linux-gnu/tracker-2.0/",
        ],
    ),
    ("libwacom2", &["usr/lib/x86_64-linux-gnu/libwacom.so.2"]),
    (
        "libssl1.1",
        &[
            "usr/lib/x86_64-linux-gnu/libcrypto.so.1.1",
            "usr/lib/x86_64-linux-gnu/libssl.so.1.1",
            "usr/lib/x86_64-linux-gnu/engines-1.1/",
        ],
    ),
    (
        "libbinutils",
        &["usr/lib/x86_64-linux-gnu/libbfd-2.35.2-system.so"],
    ),
    (
        "libicu67",
        &[
            "usr/lib/x86_64-linux-gnu/libicudata.so.67",
            "usr/lib/x86_64-linux-gnu/libicui18n.so.67",
            "usr/lib/x86_64-linux-gnu/libicuuc.so.67",
        ],
    ),
    (
        "libgeocode-glib0",
        &["usr/lib/x86_64-linux-gnu/libgeocode-glib.so.0"],
    ),
    ("libsm-devctl-dev", &["usr/lib/libsm_devctl.so.1"]),
];

fn u16le(b: &[u8], p: usize) -> u16 {
    u16::from_le_bytes(b[p..p + 2].try_into().unwrap())
}

fn node_mode(node: &Node) -> u32 {
    if node.header.len() >= 4 {
        (u16le(&node.header, 2) as u32) & 0o7777
    } else {
        match node.kind {
            Kind::Dir { .. } => 0o755,
            Kind::Link(_) => 0o777,
            _ => 0o644,
        }
    }
}

enum TarItem {
    Dir { mode: u32 },
    File { mode: u32, data: Vec<u8> },
    Link { target: String },
}

struct TarBuilder {
    buf: Vec<u8>,
}

impl TarBuilder {
    fn new() -> Self {
        Self { buf: Vec::new() }
    }

    fn write_octal(dst: &mut [u8], val: u64) {
        let width = dst.len() - 1;
        let s = format!("{:0width$o}", val, width = width);
        let bytes = s.as_bytes();
        let copy_len = bytes.len().min(width);
        let start = width - copy_len;
        for b in &mut dst[..width] {
            *b = b'0';
        }
        dst[start..width].copy_from_slice(&bytes[bytes.len() - copy_len..]);
        dst[width] = 0;
    }

    fn push_header(
        &mut self,
        name: &str,
        mode: u32,
        size: u64,
        typeflag: u8,
        linkname: &str,
        prefix: &str,
    ) {
        let mut hdr = [0u8; 512];
        let nb = name.as_bytes();
        hdr[..nb.len().min(100)].copy_from_slice(&nb[..nb.len().min(100)]);
        Self::write_octal(&mut hdr[100..108], mode as u64);
        Self::write_octal(&mut hdr[108..116], 0);
        Self::write_octal(&mut hdr[116..124], 0);
        Self::write_octal(&mut hdr[124..136], size);
        Self::write_octal(&mut hdr[136..148], 1700000000);
        hdr[148..156].fill(b' ');
        hdr[156] = typeflag;
        let lb = linkname.as_bytes();
        hdr[157..157 + lb.len().min(100)].copy_from_slice(&lb[..lb.len().min(100)]);
        hdr[257..263].copy_from_slice(b"ustar\0");
        hdr[263..265].copy_from_slice(b"00");
        hdr[265..269].copy_from_slice(b"root");
        hdr[297..301].copy_from_slice(b"root");
        let pb = prefix.as_bytes();
        hdr[345..345 + pb.len().min(155)].copy_from_slice(&pb[..pb.len().min(155)]);

        let sum: u32 = hdr.iter().map(|&b| b as u32).sum();
        let sum_str = format!("{:06o}\0 ", sum);
        hdr[148..156].copy_from_slice(sum_str.as_bytes());
        self.buf.extend_from_slice(&hdr);
    }

    fn push_pax(&mut self, records: &[(String, String)]) {
        let mut payload = Vec::new();
        for (k, v) in records {
            let body = format!(" {}={}\n", k, v);
            let mut total = body.len() + 1;
            loop {
                let line = format!("{}{}", total, body);
                if line.len() == total {
                    payload.extend_from_slice(line.as_bytes());
                    break;
                }
                total = line.len();
            }
        }
        self.push_header("PaxHeader/entry", 0o644, payload.len() as u64, b'x', "", "");
        self.buf.extend_from_slice(&payload);
        let pad = (512 - (payload.len() % 512)) % 512;
        self.buf.resize(self.buf.len() + pad, 0);
    }

    fn add_entry(&mut self, path: &str, item: &TarItem) {
        let (mode, size, typeflag, linkname) = match item {
            TarItem::Dir { mode } => (*mode, 0u64, b'5', ""),
            TarItem::File { mode, data } => (*mode, data.len() as u64, b'0', ""),
            TarItem::Link { target } => (0o777, 0u64, b'2', target.as_str()),
        };

        let mut pax = Vec::new();
        let (name_field, prefix_field) = if path.len() <= 100 {
            (path.to_string(), String::new())
        } else {
            // Try split at '/'
            let mut split = None;
            if let Some(pos) = path[..path.len().min(155)].rfind('/') {
                let pfx = &path[..pos];
                let rest = &path[pos + 1..];
                if pfx.len() <= 155 && rest.len() <= 100 && !rest.is_empty() {
                    split = Some((rest.to_string(), pfx.to_string()));
                }
            }
            match split {
                Some(s) => s,
                None => {
                    pax.push(("path".to_string(), path.to_string()));
                    let truncated = &path[..100];
                    (truncated.to_string(), String::new())
                }
            }
        };

        let link_field = if linkname.len() > 100 {
            pax.push(("linkpath".to_string(), linkname.to_string()));
            &linkname[..100]
        } else {
            linkname
        };

        if !pax.is_empty() {
            self.push_pax(&pax);
        }
        self.push_header(&name_field, mode, size, typeflag, link_field, &prefix_field);

        if let TarItem::File { data, .. } = item {
            self.buf.extend_from_slice(data);
            let pad = (512 - (data.len() % 512)) % 512;
            self.buf.resize(self.buf.len() + pad, 0);
        }
    }

    fn finish(mut self) -> Vec<u8> {
        self.buf.resize(self.buf.len() + 1024, 0);
        self.buf
    }
}

fn find_squash_offset(iso_path: &Path) -> Result<u64> {
    let mut reader = IsoReader::new(iso_path).map_err(|e| e.to_string())?;
    let files = reader.files().map_err(|e| e.to_string())?;
    for (p, ext) in files {
        if p.eq_ignore_ascii_case("/live/filesystem.squashfs") {
            return Ok(ext.offset);
        }
    }
    Err("Cannot find /live/filesystem.squashfs in ISO".into())
}

fn parse_dpkg_status(raw: &str) -> BTreeMap<String, Vec<(String, String)>> {
    let mut map = BTreeMap::new();
    for stanza in raw.split("\n\n") {
        let mut fields: Vec<(String, String)> = Vec::new();
        for line in stanza.lines() {
            if line.starts_with(' ') || line.starts_with('\t') {
                if let Some(last) = fields.last_mut() {
                    last.1.push('\n');
                    last.1.push_str(line);
                }
            } else if let Some((k, v)) = line.split_once(':') {
                fields.push((k.trim().to_string(), v.trim_start().to_string()));
            }
        }
        if let Some((_, pkg)) = fields.iter().find(|(k, _)| k == "Package") {
            map.insert(pkg.clone(), fields);
        }
    }
    map
}

fn rewrite_dep_list(raw: &str, pkg: &str) -> String {
    let mut out: Vec<String> = Vec::new();
    let mut seen: BTreeSet<String> = BTreeSet::new();

    for item in raw.split(',') {
        let t = item.trim();
        if t.is_empty() {
            continue;
        }
        let base_name = t
            .split([' ', '(', '|'])
            .next()
            .unwrap_or("")
            .trim()
            .to_string();

        let mapped: Option<String> = match base_name.as_str() {
            "pam-nfs-faillock" | "pam-nfs-auth" | "pam-nfs-qr" | "aptdaemon" | "bioauth" => None,
            "libcroco3"
            | "libgnome-desktop-3-19"
            | "libtracker-sparql-2.0-0"
            | "libwacom2"
            | "libssl1.1"
            | "libbinutils"
            | "libicu67"
            | "libgeocode-glib0" => Some("nfs-compat-libs".to_string()),
            "tracker-extract" => Some("tracker-miner-fs".to_string()),
            "gnome-themes-standard" => Some("gnome-themes-extra".to_string()),
            _ => Some(t.to_string()),
        };

        if let Some(m) = mapped {
            if seen.insert(m.clone()) {
                out.push(m);
            }
        }
    }

    if pkg == "found-icon-theme" && seen.insert("found-icon-theme-dark".to_string()) {
        out.push("found-icon-theme-dark".to_string());
    }
    if (pkg == "found-desktop"
        || pkg == "found-settings-daemon"
        || pkg == "found-control-center"
        || pkg == "nemo"
        || pkg == "libmutter-6-0")
        && seen.insert("nfs-compat-libs".to_string())
    {
        out.push("nfs-compat-libs".to_string());
    }

    out.join(", ")
}

fn build_control_text(pkg: &str, fields: &[(String, String)], version_override: Option<&str>, arch_override: Option<&str>, installed_kib: u64) -> String {
    let mut lines = Vec::new();
    for (k, v) in fields {
        match k.as_str() {
            "Status" | "Original-Maintainer" => continue,
            "Version" => {
                lines.push(format!("Version: {}", version_override.unwrap_or(v)));
            }
            "Architecture" => {
                lines.push(format!("Architecture: {}", arch_override.unwrap_or(v)));
            }
            "Installed-Size" => {
                lines.push(format!("Installed-Size: {}", installed_kib.max(1)));
            }
            "Depends" | "Pre-Depends" | "Recommends" => {
                let rw = rewrite_dep_list(v, pkg);
                if !rw.is_empty() {
                    lines.push(format!("{}: {}", k, rw));
                }
            }
            _ => {
                lines.push(format!("{}: {}", k, v));
            }
        }
    }
    if !fields.iter().any(|(k, _)| k == "Installed-Size") {
        lines.push(format!("Installed-Size: {}", installed_kib.max(1)));
    }
    lines.join("\n") + "\n"
}

fn collect_branding_overrides(root: &Path, cur: &Path, out: &mut BTreeMap<String, Vec<u8>>) -> Result<()> {
    if !cur.is_dir() {
        return Ok(());
    }
    for entry in fs::read_dir(cur).map_err(|e| e.to_string())? {
        let entry = entry.map_err(|e| e.to_string())?;
        let p = entry.path();
        if p.is_dir() {
            collect_branding_overrides(root, &p, out)?;
        } else {
            let rel = p
                .strip_prefix(root)
                .map_err(|e| e.to_string())?
                .to_string_lossy()
                .replace('\\', "/");
            let data = fs::read(&p).map_err(|e| e.to_string())?;
            out.insert(rel, data);
        }
    }
    Ok(())
}

fn write_tar_from_items(items: &BTreeMap<String, TarItem>, out_path: &Path) -> Result<()> {
    let mut tb = TarBuilder::new();
    for (path, item) in items {
        tb.add_entry(path, item);
    }
    let bytes = tb.finish();
    let mut f = File::create(out_path).map_err(|e| e.to_string())?;
    f.write_all(&bytes).map_err(|e| e.to_string())
}

fn ensure_parent_dirs(items: &mut BTreeMap<String, TarItem>, rel: &str, paths: &BTreeMap<String, Node>) {
    items.entry("./".to_string()).or_insert(TarItem::Dir { mode: 0o755 });
    let mut acc = String::new();
    let parts: Vec<&str> = rel.split('/').collect();
    for (idx, part) in parts.iter().enumerate() {
        if part.is_empty() {
            continue;
        }
        if !acc.is_empty() {
            acc.push('/');
        }
        acc.push_str(part);
        if idx + 1 < parts.len() {
            let tar_dir = format!("./{}/", acc);
            let mode = paths
                .get(&acc)
                .map(node_mode)
                .unwrap_or(0o755);
            items.entry(tar_dir).or_insert(TarItem::Dir { mode });
        }
    }
}

fn patch_maintainer_script(pkg: &str, ext: &str, raw: &[u8]) -> Vec<u8> {
    let s = String::from_utf8_lossy(raw);
    match (pkg, ext) {
        ("found-desktop", "postinst") => s
            .replace(
                "setcap CAP_AUDIT_WRITE+ep /usr/libexec/cdos-user-verify",
                "setcap CAP_AUDIT_WRITE+ep /usr/libexec/cdos-user-verify || true",
            )
            .into_bytes(),
        ("found-desktop-environment", "postinst") => s
            .replace("found-de -l $packages_file", "found-de -l $packages_file || true")
            .into_bytes(),
        ("cdos-plymouth-theme-nfslogo", "postinst") => {
            let patched = r#"#!/bin/sh
set -e
case "$1" in
    configure|upgrade|update|reconfigure)
        mkdir -p /usr/share/initramfs-tools/hooks
        cp -f /usr/share/plymouth/themes/nfslogo/chinese /usr/share/initramfs-tools/hooks/ || true
        if command -v plymouth-set-default-theme >/dev/null 2>&1; then
            plymouth-set-default-theme nfslogo || true
        fi
        update-alternatives --install \
            /usr/share/desktop-base/active-theme \
            desktop-theme \
            /usr/share/plymouth/themes/nfslogo 60 || true
    ;;
esac
exit 0
"#;
            patched.as_bytes().to_vec()
        }
        _ => raw.to_vec(),
    }
}

fn pam_screensaver_replacement(rel: &str) -> Option<&'static str> {
    match rel {
        "etc/pam.d/cdos-screensaver" | "etc/pam.d/cdos-screensaver-mix" | "etc/pam.d/cdos-screensaver-qr" => {
            Some(
                "# Debian 12 standard PAM stack for cdos-screensaver\n\
auth    required        pam_env.so readenv=1 envfile=/etc/default/locale\n\
@include common-auth\n\
auth    optional        pam_cap.so\n\
auth    optional        pam_gnome_keyring.so\n\
@include common-account\n",
            )
        }
        _ => None,
    }
}

fn export_package(
    pkg: &str,
    control_pkg_name: &str,
    squash: &mut Squash,
    paths: &BTreeMap<String, Node>,
    dpkg_status: &BTreeMap<String, Vec<(String, String)>>,
    branding: &BTreeMap<String, Vec<u8>>,
    staging_dir: &Path,
    filter: impl Fn(&str) -> bool,
    desc_override: Option<&str>,
) -> Result<String> {
    let fields = dpkg_status
        .get(control_pkg_name)
        .ok_or_else(|| format!("Missing dpkg status for {control_pkg_name}"))?;
    let version = fields
        .iter()
        .find(|(k, _)| k == "Version")
        .map(|(_, v)| v.clone())
        .ok_or("Missing Version")?;
    let arch = fields
        .iter()
        .find(|(k, _)| k == "Architecture")
        .map(|(_, v)| v.clone())
        .unwrap_or_else(|| "amd64".to_string());

    let list_path = if paths.contains_key(&format!("var/lib/dpkg/info/{control_pkg_name}.list")) {
        format!("var/lib/dpkg/info/{control_pkg_name}.list")
    } else {
        format!("var/lib/dpkg/info/{control_pkg_name}:{arch}.list")
    };
    let list_bytes = squash.read(&list_path)?;
    let list_str = String::from_utf8_lossy(&list_bytes);

    let mut data_items: BTreeMap<String, TarItem> = BTreeMap::new();
    let mut total_bytes: u64 = 0;

    for raw_line in list_str.lines() {
        let rel = raw_line.trim().trim_start_matches('/');
        if rel.is_empty() || rel == "." || !filter(rel) {
            continue;
        }
        let Some(node) = paths.get(rel) else {
            continue;
        };
        match &node.kind {
            Kind::Dir { .. } => {
                ensure_parent_dirs(&mut data_items, rel, paths);
                let tar_dir = format!("./{rel}/");
                data_items.insert(tar_dir, TarItem::Dir { mode: node_mode(node) });
            }
            Kind::File { size, .. } => {
                ensure_parent_dirs(&mut data_items, rel, paths);
                let data = if let Some(pam) = pam_screensaver_replacement(rel) {
                    pam.as_bytes().to_vec()
                } else if let Some(ov) = branding.get(rel) {
                    ov.clone()
                } else {
                    squash.read_node(node)?
                };
                total_bytes += data.len().max(*size as usize) as u64;
                data_items.insert(
                    format!("./{rel}"),
                    TarItem::File {
                        mode: node_mode(node),
                        data,
                    },
                );
            }
            Kind::Link(target) => {
                ensure_parent_dirs(&mut data_items, rel, paths);
                data_items.insert(
                    format!("./{rel}"),
                    TarItem::Link {
                        target: target.clone(),
                    },
                );
            }
            Kind::Other => {}
        }
    }

    // Also include any branding_edit files that belong to this package path prefix
    if pkg == "found-desktop-common" {
        for (rel, data) in branding {
            if rel.starts_with("usr/share/found-desktop/data/") {
                ensure_parent_dirs(&mut data_items, rel, paths);
                data_items.insert(
                    format!("./{rel}"),
                    TarItem::File {
                        mode: 0o644,
                        data: data.clone(),
                    },
                );
            }
        }
    }

    // Build control.tar
    let mut ctrl_items: BTreeMap<String, TarItem> = BTreeMap::new();
    ctrl_items.insert("./".to_string(), TarItem::Dir { mode: 0o755 });

    let mut adjusted_fields = fields.clone();
    if pkg != control_pkg_name {
        for (k, v) in &mut adjusted_fields {
            if k == "Package" {
                *v = pkg.to_string();
            }
            if k == "Description" {
                if let Some(d) = desc_override {
                    *v = d.to_string();
                }
            }
        }
    }
    let mut control_txt = build_control_text(
        pkg,
        &adjusted_fields,
        None,
        None,
        (total_bytes / 1024).max(1),
    );
    if pkg == "found-icon-theme-dark" {
        // Dark split package should not depend on itself or found-icon-theme
        let mut new_lines = Vec::new();
        for l in control_txt.lines() {
            if !l.starts_with("Depends:") {
                new_lines.push(l.to_string());
            }
        }
        control_txt = new_lines.join("\n") + "\n";
    }

    ctrl_items.insert(
        "./control".to_string(),
        TarItem::File {
            mode: 0o644,
            data: control_txt.into_bytes(),
        },
    );

    if pkg == control_pkg_name {
        for ext in ["preinst", "postinst", "prerm", "postrm", "triggers", "conffiles", "shlibs"] {
            let p1 = format!("var/lib/dpkg/info/{control_pkg_name}.{ext}");
            let p2 = format!("var/lib/dpkg/info/{control_pkg_name}:{arch}.{ext}");
            let p = if paths.contains_key(&p1) {
                Some(p1)
            } else if paths.contains_key(&p2) {
                Some(p2)
            } else {
                None
            };
            if let Some(meta_path) = p {
                if let Ok(raw) = squash.read(&meta_path) {
                    let data = patch_maintainer_script(pkg, ext, &raw);
                    let mode = if matches!(ext, "preinst" | "postinst" | "prerm" | "postrm") {
                        0o755
                    } else {
                        0o644
                    };
                    ctrl_items.insert(
                        format!("./{ext}"),
                        TarItem::File { mode, data },
                    );
                }
            }
        }
    }

    let safe_ver = version.replace(':', "_");
    let deb_base = format!("{pkg}_{safe_ver}_{arch}");
    let pkg_stage = staging_dir.join(&deb_base);
    fs::create_dir_all(&pkg_stage).map_err(|e| e.to_string())?;
    write_tar_from_items(&ctrl_items, &pkg_stage.join("control.tar"))?;
    write_tar_from_items(&data_items, &pkg_stage.join("data.tar"))?;
    println!("Staged {deb_base} ({:.2} MB raw)", total_bytes as f64 / 1048576.0);
    Ok(deb_base)
}

fn export_compat_libs(
    squash: &mut Squash,
    paths: &BTreeMap<String, Node>,
    staging_dir: &Path,
) -> Result<String> {
    let mut data_items: BTreeMap<String, TarItem> = BTreeMap::new();
    let mut total_bytes: u64 = 0;

    for (src_pkg, prefixes) in COMPAT_SOURCES {
        let lp1 = format!("var/lib/dpkg/info/{src_pkg}.list");
        let lp2 = format!("var/lib/dpkg/info/{src_pkg}:amd64.list");
        let lp = if paths.contains_key(&lp1) { lp1 } else { lp2 };
        let list_bytes = squash.read(&lp)?;
        for raw_line in String::from_utf8_lossy(&list_bytes).lines() {
            let rel = raw_line.trim().trim_start_matches('/');
            if !prefixes.iter().any(|pfx| rel.starts_with(pfx)) {
                continue;
            }
            let Some(node) = paths.get(rel) else { continue; };
            match &node.kind {
                Kind::Dir { .. } => {
                    ensure_parent_dirs(&mut data_items, rel, paths);
                    data_items.insert(format!("./{rel}/"), TarItem::Dir { mode: node_mode(node) });
                }
                Kind::File { size, .. } => {
                    ensure_parent_dirs(&mut data_items, rel, paths);
                    let data = squash.read_node(node)?;
                    total_bytes += *size;
                    data_items.insert(
                        format!("./{rel}"),
                        TarItem::File {
                            mode: node_mode(node),
                            data,
                        },
                    );
                }
                Kind::Link(target) => {
                    ensure_parent_dirs(&mut data_items, rel, paths);
                    data_items.insert(
                        format!("./{rel}"),
                        TarItem::Link {
                            target: target.clone(),
                        },
                    );
                }
                Kind::Other => {}
            }
        }
    }

    // Add ld.so.conf.d entry for tracker-2.0 private libdir
    let ldconf_rel = "etc/ld.so.conf.d/nfs-compat-libs.conf";
    ensure_parent_dirs(&mut data_items, ldconf_rel, paths);
    data_items.insert(
        format!("./{ldconf_rel}"),
        TarItem::File {
            mode: 0o644,
            data: b"/usr/lib/x86_64-linux-gnu/tracker-2.0\n".to_vec(),
        },
    );

    let mut ctrl_items: BTreeMap<String, TarItem> = BTreeMap::new();
    ctrl_items.insert("./".to_string(), TarItem::Dir { mode: 0o755 });
    let control_txt = format!(
        "Package: nfs-compat-libs\n\
Version: 1.0+nfs5\n\
Architecture: amd64\n\
Maintainer: Debian NFS Builder <builder@localhost>\n\
Section: libs\n\
Priority: optional\n\
Installed-Size: {}\n\
Depends: libc6 (>= 2.30), libglib2.0-0, libgtk-3-0, libgudev-1.0-0, libjson-glib-1.0-0, libsoup2.4-1, libxml2, libstdc++6, libsystemd0, libudev1, libseccomp2, libxkbregistry0, libwacom-common, gnome-desktop3-data\n\
Description: Debian 11 runtime compatibility libraries for NFS Found Desktop on Debian 12\n \
Provides legacy SONAME shared libraries (libcroco-0.6, libgnome-desktop-3-19,\n \
libtracker-sparql-2.0, libwacom2, libssl1.1, libbfd-2.35.2, libicu67,\n \
libgeocode-glib0, libsm_devctl) without conflicting with Debian 12 packages.\n",
        (total_bytes / 1024).max(1)
    );
    ctrl_items.insert(
        "./control".to_string(),
        TarItem::File {
            mode: 0o644,
            data: control_txt.into_bytes(),
        },
    );
    ctrl_items.insert(
        "./postinst".to_string(),
        TarItem::File {
            mode: 0o755,
            data: b"#!/bin/sh\nset -e\nldconfig\n".to_vec(),
        },
    );
    ctrl_items.insert(
        "./postrm".to_string(),
        TarItem::File {
            mode: 0o755,
            data: b"#!/bin/sh\nset -e\nldconfig\n".to_vec(),
        },
    );

    let deb_base = "nfs-compat-libs_1.0+nfs5_amd64".to_string();
    let pkg_stage = staging_dir.join(&deb_base);
    fs::create_dir_all(&pkg_stage).map_err(|e| e.to_string())?;
    write_tar_from_items(&ctrl_items, &pkg_stage.join("control.tar"))?;
    write_tar_from_items(&data_items, &pkg_stage.join("data.tar"))?;
    println!("Staged {deb_base} ({:.2} MB raw)", total_bytes as f64 / 1048576.0);
    Ok(deb_base)
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 4 {
        eprintln!("Usage: nfs-exporter <nfs.iso> <branding_edit_dir> <staging_dir>");
        std::process::exit(1);
    }
    let iso_path = PathBuf::from(&args[1]);
    let branding_dir = PathBuf::from(&args[2]);
    let staging_dir = PathBuf::from(&args[3]);
    fs::create_dir_all(&staging_dir).map_err(|e| e.to_string())?;

    let mut branding = BTreeMap::new();
    collect_branding_overrides(&branding_dir.join("squashfs"), &branding_dir.join("squashfs"), &mut branding)?;
    println!("Loaded {} branding_edit squashfs overrides", branding.len());

    let offset = find_squash_offset(&iso_path)?;
    println!("Found SquashFS at offset {}", offset);
    let mut squash = Squash::open(&iso_path, offset)?;
    let paths = squash.paths()?;
    let status_raw = String::from_utf8_lossy(&squash.read("var/lib/dpkg/status")?).to_string();
    let dpkg_status = parse_dpkg_status(&status_raw);

    for &pkg in EXPORT_PACKAGES {
        if pkg == "found-icon-theme" {
            export_package(
                "found-icon-theme",
                "found-icon-theme",
                &mut squash,
                &paths,
                &dpkg_status,
                &branding,
                &staging_dir,
                |rel| !rel.starts_with("usr/share/icons/FoundDark"),
                None,
            )?;
            export_package(
                "found-icon-theme-dark",
                "found-icon-theme",
                &mut squash,
                &paths,
                &dpkg_status,
                &branding,
                &staging_dir,
                |rel| rel.starts_with("usr/share/icons/FoundDark"),
                Some("FoundDark icon theme split from found-icon-theme"),
            )?;
        } else {
            export_package(
                pkg,
                pkg,
                &mut squash,
                &paths,
                &dpkg_status,
                &branding,
                &staging_dir,
                |_| true,
                None,
            )?;
        }
    }

    export_compat_libs(&mut squash, &paths, &staging_dir)?;
    println!("Staging complete in {}", staging_dir.display());
    Ok(())
}
