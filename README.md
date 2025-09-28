# Cortex Command Reference

This document explains how to use the `cortex` shell after you have compiled it.

## Starting the Shell

- `cortex` – launch with the default VFS root.
- `cortex --portable` – use `./data/rootfs` alongside the executable.
- `USER` is read from `/etc/username` on startup; if it is missing, the shell will prompt for one.

Prompt format: `<user>@cortex:<cwd>$` where `/` is shown as `~`.

## Navigation Commands

| Command | Description | Examples |
|---------|-------------|----------|
| `pwd` | Print current VFS path | `pwd` |
| `ls [-l] [-a] [path]` | List directory contents | `ls -l /usr`; `ls` |
| `cd [path]` | Change directory (`/` when omitted) | `cd /etc`; `cd ..` |

Paths are always interpreted inside the virtual file system; attempts to escape the root are rejected.

## File Management

| Command | Description | Examples |
|---------|-------------|----------|
| `cat [file]` | Show file contents | `cat hello.txt` |
| `touch <file>` | Create or update a file timestamp | `touch notes.txt` |
| `mkdir [-p] <dir>` | Create directories | `mkdir projects`; `mkdir -p logs/app` |
| `rm [-r] <path>` | Remove files or directories | `rm old.txt`; `rm -r tmp` |
| `cp [-r] <src> <dst>` | Copy files/directories | `cp a.txt b.txt`; `cp -r dir backup` |
| `mv <src> <dst>` | Move or rename | `mv draft.txt final.txt` |
| `stat <path>` | Show metadata (name, size, type) | `stat /etc/username` |

Use `chmod +x <file>` / `chmod -x <file>` to toggle executable permission. Execute scripts via absolute or relative paths, e.g. `/scripts/hello.sh`.

## Content Utilities

- `head [-n N] [file]` – first N lines.
- `tail [-n N] [file]` – last N lines.
- `find <path> [-name PAT] [-type f|d] [-size +N|-N|N] [-maxdepth D]` – recursive search.
- `grep [-n] [-i] [-r] PATTERN [path]` – match lines or files.

## Environment & Shell Helpers

- `echo [args...]` – print arguments.
- `env` – list current environment variables.
- `set KEY=VALUE` – set variable (no spaces around `=`).
- `unset KEY` – remove variable.
- `$?` contains the status of the last command.
- `clear` – clear the console; on Windows the command enables VT sequences when possible.
- `help [cmd]` – list commands or show command-specific help.
- `version` – print Cortex build information.

## Redirection and Pipelines

- `>` overwrite, `>>` append – last stage only.
- `<` input redirect – first stage only.
- `|` pipelines – linear pipelines supported; e.g. `cat file.txt | grep error`.

## Archives

- `pack <source...> -o <archive>`
  - Bundles files/directories into a MiniArch `.mar` file.
  - Directory sources are stored with their top-level name so they extract into a folder.
- `unpack <archive> [-C <dest>]`
  - Extracts MiniArch archives. Destination defaults to current directory.

Errors such as missing sources or invalid archives return non-zero status codes and leave partial outputs cleaned up.

## Package Manager (`pkg`)

`pkg` reads repository metadata from `packages/index.ini` (or the directory defined by `CORTEX_PKG_ROOT`). Packages currently support script payloads.

| Subcommand | Purpose |
|------------|---------|
| `pkg list` | Display packages and mark installed ones. |
| `pkg info <name>` | Show metadata, install status, install path, payload source. |
| `pkg install <name>` | Copy the package payload into the VFS and mark executable. |
| `pkg remove <name>` | Remove the installed file and registry entry. |
| `pkg installed` | List installed packages with versions and paths. |

Installed package records are stored at `/var/lib/pkg/installed.db` inside the VFS.

## Running Scripts

- Make a script executable: `chmod +x /scripts/hello.sh`
- Execute via path: `/scripts/hello.sh arg1 arg2`
- `source <path>` can load and run scripts within the current shell environment.

## Exit

- `exit` or `quit` terminates the shell.
- `Ctrl+C` interrupts the current command and returns to the prompt (without terminating Cortex).

