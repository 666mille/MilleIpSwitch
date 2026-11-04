"""Generates the Scoop and winget manifests for a release.

Usage: python tools/make_manifests.py --version 1.1.0 [--sha256 HASH] [--out dist]

Without --sha256 the checksum is downloaded from the release asset milleipswitch.exe.sha256.
"""
import argparse
import json
import os
import re
import urllib.request

OWNER = "666mille"
REPO = "MilleIpSwitch"
EXE = "milleipswitch.exe"
ID = f"{OWNER}.{REPO}"
HOME = f"https://github.com/{OWNER}/{REPO}"
DESC = "Fast network profile switcher for Windows - lightweight NetSetMan alternative"
TAGS = ["network", "ip-address", "dhcp", "dns", "profile", "tray", "netsetman-alternative"]


def fetch_hash(version):
    url = f"{HOME}/releases/download/v{version}/{EXE}.sha256"
    with urllib.request.urlopen(url, timeout=30) as r:
        m = re.search(r"\b[0-9a-fA-F]{64}\b", r.read().decode("ascii", "ignore"))
    if not m:
        raise SystemExit("no SHA256 found in " + url)
    return m.group(0).lower()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def scoop(version, sha, out):
    url = f"{HOME}/releases/download/v{version}/{EXE}"
    manifest = {
        "version": version,
        "description": DESC,
        "homepage": HOME,
        "license": "MIT",
        "url": url,
        "hash": sha,
        "bin": EXE,
        "shortcuts": [[EXE, "MilleIpSwitch"]],
        "checkver": "github",
        "autoupdate": {"url": f"{HOME}/releases/download/v$version/{EXE}"},
    }
    write(os.path.join(out, "scoop", "milleipswitch.json"), json.dumps(manifest, indent=4) + "\n")


def winget(version, sha, out):
    d = os.path.join(out, "winget", version)
    mv = "1.6.0"
    write(os.path.join(d, f"{ID}.yaml"),
          f"PackageIdentifier: {ID}\nPackageVersion: {version}\nDefaultLocale: en-US\n"
          f"ManifestType: version\nManifestVersion: {mv}\n")
    write(os.path.join(d, f"{ID}.installer.yaml"),
          f"PackageIdentifier: {ID}\nPackageVersion: {version}\nInstallerType: portable\n"
          f"Commands:\n- milleipswitch\nInstallers:\n- Architecture: x64\n"
          f"  InstallerUrl: {HOME}/releases/download/v{version}/{EXE}\n  InstallerSha256: {sha.upper()}\n"
          f"ManifestType: installer\nManifestVersion: {mv}\n")
    tags = "\n".join(f"- {t}" for t in TAGS)
    write(os.path.join(d, f"{ID}.locale.en-US.yaml"),
          f"PackageIdentifier: {ID}\nPackageVersion: {version}\nPackageLocale: en-US\n"
          f"Publisher: {OWNER}\nPublisherUrl: https://github.com/{OWNER}\nPackageName: MilleIpSwitch\n"
          f"PackageUrl: {HOME}\nLicense: MIT\nLicenseUrl: {HOME}/blob/main/LICENSE\n"
          f"ShortDescription: {DESC}\nMoniker: milleipswitch\nTags:\n{tags}\n"
          f"ManifestType: defaultLocale\nManifestVersion: {mv}\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--sha256")
    ap.add_argument("--out", default="dist")
    a = ap.parse_args()
    version = a.version.lstrip("v")
    sha = (a.sha256 or fetch_hash(version)).lower()
    if not re.fullmatch(r"[0-9a-f]{64}", sha):
        raise SystemExit("invalid SHA256")
    scoop(version, sha, a.out)
    winget(version, sha, a.out)
    print(f"Manifests for {version} written to {a.out}")


if __name__ == "__main__":
    main()
