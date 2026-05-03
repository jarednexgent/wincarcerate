# WinCarcerate

> [!WARNING]
> For research, education, and authorized defensive security testing only.
> Do not use on systems you do not own or lack permission to assess.
> The author is not responsible for misuse or resulting damage.

**`WinCarcerate`** is a simulated ransomware payload for Windows. It implements a thread-safe producer-consumer design pattern, optimized for performance and accelerated using [AES-NI](https://www.intel.com/content/www/us/en/developer/articles/technical/advanced-encryption-standard-instructions-aes-ni.html) intrinsics.

## Build 

Open the **Developer PowerShell for VS 2022**, run `Builder.ps1`, and follow the interactive prompts.

```
C:\Tools\WinCarcerate>.\Builder.ps1

888       888 d8b           .d8888b.                                                   888
888   o   888 Y8P          d88P  Y88b                                                  888
888  d8b  888              888    888                                                  888
888 d888b 888 888 88888b.  888         8888b.  888d888 .d8888b .d88b.  888d888 8888b.  888888 .d88b.
888d88888b888 888 888 "88b 888            "88b 888P"  d88P"   d8P  Y8b 888P"      "88b 888   d8P  Y8b
88888P Y88888 888 888  888 888    888 .d888888 888    888     88888888 888    .d888888 888   88888888
8888P   Y8888 888 888  888 Y88b  d88P 888  888 888    Y88b.   Y8b.     888    888  888 Y88b. Y8b.
888P     Y888 888 888  888  "Y8888P"  "Y888888 888     "Y8888P "Y8888  888    "Y888888  "Y888 "Y8888



=== WinCarcerate Build (Release|x64) ===

Toolset: [M]SVC or [L]LVM/Clang-cl?: M
Build [L]ocker or [D]ecryptor?: L
Enable logging? [Y]es or [N]o?: Y

  Toolset : v143
  Target  : Locker
  Logging : True
  Defines : /DLOCKER /DLOG_TO_CONSOLE

Build started 4/26/2026 3:46:22 AM.
```

Each run of `Builder.ps1` produces a single output binary at `x64\Release\WinCarcerate.exe`. If you need both Locker and Decryptor variants for a lab, run the build twice and preserve the first output before rebuilding so it is not overwritten.


## Usage

No command-line arguments are required. On launch, `WinCarcerate.exe` automatically identifies all local drives and recursively encrypts the filesystem from each drive root. Upon completion, a mock ransom note is displayed, then the binary deletes itself from disk.

