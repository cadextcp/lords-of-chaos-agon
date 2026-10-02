# ADR 0001 – C mit agondev statt BBC BASIC

- **Status:** angenommen (2026-10-02)
- **Kontext:** Versuch 1 (BBC BASIC) scheiterte an Interpreter-Grenzen: nur GOSUB, Dateien über 14,4 KB crashen, langsames VDU über eine BASIC-Schicht. Vom Spiel entstand kaum etwas.

## Entscheidung

- Das Spiel wird in **C99** geschrieben, Hotspots später in **eZ80-Assembler**.
- Toolchain ist **agondev v0.22** (AgonPlatform, clang/LLVM für ez80, aktiv gepflegt), nicht das ältere AgDev 3.1.0 (letztes Release 2024).
- agondev gibt es nur für Linux und macOS. Unter Windows wird es über **WSL** aufgerufen (`tools/agon_env.py`), in CI nativ auf `ubuntu-latest`.
- Emulator ist **fab-agon-emulator 1.2.5** (GUI und CLI), MOS gepinnt auf **Console8 2.3.3**.
- Alle Downloads werden per SHA-256 geprüft (`tools/setup.py`). Ein Versions-Update ist eine bewusste Änderung.

## Folgen

- \+ Echter Compiler, Funktionen, Structs, Module, Optimierung; keine Größen- oder Zeilennummern-Limits.
- \+ Derselbe Core kompiliert auch mit gcc für Host-Tests (ADR 0003).
- − Unter Windows ist WSL Voraussetzung.
- − `int` ist 24 Bit. Das ist eine Disziplinfrage für Core-Code und wird durch den Selftest auf beiden Plattformen abgesichert.
