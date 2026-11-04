# Changelog

## 1.1.0
- AutoSwitch: activate profiles automatically (adapter connected, gateway MAC, ping, ARP)
- IPv6 per profile: automatic or static addresses, gateway and DNS
- Interface metric in the hover card and network status, network status can be sorted by metric
- Enable/disable adapters, renew DHCP, flush DNS cache in the network status
- Import and export of profiles, automatic backups of the profile file
- Optional daily update check against GitHub releases
- Profiles are stored in `%APPDATA%\MilleIpSwitch` unless `profiles.ini` or a file named `portable` is next to the exe
- Release assets include a SHA256 checksum, a Scoop manifest and winget manifests

## 1.0.1
- Fix tray icon not appearing after login

## 1.0.0
- First public release
