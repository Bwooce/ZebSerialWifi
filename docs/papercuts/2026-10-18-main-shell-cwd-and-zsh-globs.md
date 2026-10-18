# Relative cd and unquoted grep globs fail in the tool shell

- Date: 2026-10-18
- Agent: main
- Seen before: no

What happened: `cd firmware/main` failed because the shell was already in firmware/main (the working directory moves
between calls), and `grep --include=*.c` failed under zsh with "no matches found".
Workaround: absolute paths for cd; quote the glob (`--include='*.c'`) or list the files.
Suggested fix: always cd to /Users/bruce/dev/ZebSerialWifi/... with the full path.
