# Cortex v0.1

A minimal, portable C++ shell + VFS. Cortex provides a simple REPL with a folder-backed virtual file system sandbox and a set of core Unix-like commands, including pipelines and redirections.

## Build

Requires CMake (>=3.15) and a C++17 compiler.

- Windows (Visual Studio Developer Prompt):
  - `cmake -S . -B build`
  - `cmake --build build --config Release --target cortex`
- Windows (MinGW/Clang + Ninja):
  - `cmake -S . -B build -G Ninja`
  - `cmake --build build --target cortex`
- Linux/macOS:
  - `cmake -S . -B build`
  - `cmake --build build --config Release --target cortex`

## Run

- Default VFS root:
  - Windows: `%LOCALAPPDATA%/Cortex/rootfs`
  - Linux/macOS: `~/.local/share/Cortex/rootfs`
- Portable mode (alongside the binary):
  - `cortex --portable` uses `./data/rootfs`

On first launch, Cortex asks you to create a username. The value is stored in `/etc/username` (inside the VFS). Subsequent launches will use it for the prompt.

## Commands (v0.1)
- Navigation: `pwd`, `ls [-l] [-a] [path]`, `cd [path]`
- Files: `cat [file]`, `touch <file>`, `mkdir [-p] <dir>`, `rm [-r] <path>`, `cp [-r] <src> <dst>`, `mv <src> <dst>`, `stat <path>`
- Content: `head [-n N] [file]`, `tail [-n N] [file]`, `find <path> [-name PAT] [-type f|d] [-size +N|-N|N] [-maxdepth D]`, `grep [-n] [-i] [-r] PATTERN [path]`
- Archive: `pack <source_path...> -o <output_archive>`, `unpack <archive> -C <vfs_path>`
- Shell/env: `echo ...`, `env`, `set KEY=VALUE`, `unset KEY`, `help [cmd]`, `version`, `clear`
- Exit: `exit` or `quit`

## Notes
- Path access is sandboxed; any path escaping the VFS root is rejected.
- Quoting supports simple single/double quotes and backslash escapes.
- Linear pipelines and redirections are supported with constraints below.
- Sidecar permissions/ACL, transactions, image VFS, and advanced tools are out of scope for v0.1.
- Ctrl+C behavior: interrupts the current input/command and returns to the prompt without exiting the program.

## Redirection and Pipes
- Output redirection:
  - `>` overwrite: `echo hello > a.txt`
  - `>>` append: `echo more >> a.txt`
  - Tight form also supported: `echo hi >a.txt`, `echo hi >>a.txt`
- Input redirection (first stage only):
  - `<` read file as stdin: `cat < a.txt`
- Pipelines (`|`):
  - Linear pipelines are supported: `cat < a.txt | cat`
  - Limitations: `<` only on the first command; `>`/`>>` only on the last.
  - Pipes work with or without spaces (`a|b` or `a | b`).

## Archive
- `pack <source_path...> -o <output_archive>`

- `unpack <archive> [-C <destination_path>]`

## Screen Clear
- `clear` clears the console screen.
  - Windows: uses the console API; requires running in a real console window.
  - POSIX terminals: emits ANSI clear (`ESC[2J` + `ESC[H]`).
  - When run non-interactively (piped), behavior may be a no-op to avoid control characters in logs.

## Prompt and Banner
- Banner (first launch only): `Cortex v0.1 -- The Core Shell Environment` then `(c) 2025 [Your Name]. Type 'help' for a list of commands.`
- Prompt format: `<user>@cortex:<cwd>$` where `/` is shown as `~`.
