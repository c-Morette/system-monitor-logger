# Third-party notices

## smartmontools / smartctl

This project embeds `smartctl` binaries from smartmontools for SMART disk health collection.

- Project: smartmontools
- Homepage: https://www.smartmontools.org/
- GitHub: https://github.com/smartmontools/smartmontools
- License: GNU GPL version 2

Embedded binaries:

- Windows: smartmontools 7.5, `smartctl.exe`
- Linux amd64: Ubuntu Noble smartmontools 7.4 package, `smartctl`

The application extracts the platform-specific binary to a temporary directory during execution and removes it when possible.
