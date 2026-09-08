# TPFanCtrl2 WinGet Package

This directory contains Microsoft Windows Package Manager (WinGet) manifests for **TPFanCtrl2**.

## Dependency Specification

TPFanCtrl2 requires an authorized driver backend to communicate with the ThinkPad Embedded Controller (EC).
We declare the official signed driver **`namazso.PawnIO`** as a formal package dependency in the WinGet installer manifest:

```yaml
Dependencies:
  PackageDependencies:
    - PackageIdentifier: namazso.PawnIO
      MinimumVersion: 2.2.0
```

When users install TPFanCtrl2 via WinGet (`winget install Tinnci.TPFanCtrl2`), WinGet will automatically detect and install `namazso.PawnIO` first.

---

## Directory Structure

```text
packaging/winget/
├── README.md
└── manifests/
    └── t/
        └── Tinnci/
            └── TPFanCtrl2/
                └── 2.6.0/
                    ├── Tinnci.TPFanCtrl2.yaml                 (Version manifest)
                    ├── Tinnci.TPFanCtrl2.installer.yaml       (Installer & dependencies)
                    ├── Tinnci.TPFanCtrl2.locale.en-US.yaml   (Default English metadata)
                    └── Tinnci.TPFanCtrl2.locale.zh-CN.yaml   (Simplified Chinese metadata)
```

---

## Validating Manifests Locally

You can validate the manifests locally using the official WinGet CLI:

```powershell
winget validate --manifest packaging/winget/manifests/t/Tinnci/TPFanCtrl2/2.6.0
```

---

## Testing Local Installation

To test installing the package locally from the generated manifest:

```powershell
winget install --manifest packaging/winget/manifests/t/Tinnci/TPFanCtrl2/2.6.0
```

---

## Submitting to `microsoft/winget-pkgs`

To publish TPFanCtrl2 to the official Microsoft Community Repository:

1. Fork and clone [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
2. Copy `packaging/winget/manifests/t/Tinnci/TPFanCtrl2/<version>` into `manifests/t/Tinnci/TPFanCtrl2/<version>` in your fork.
3. Verify that the GitHub release asset URL and SHA-256 match the published release.
4. Submit a Pull Request to `microsoft/winget-pkgs`.
