# Authors & Credits

## Original game

Olympia was created by **Rich Skrenta** (Copyright 1993 — the original sources
are public domain). The G1 engine in this repository is the first-generation
Olympia play-by-mail strategy game engine he wrote, the ancestor of the later
G2, G3, and TAG engines.

## Modernization & maintenance

- **Michael Henderson** <mdhender@mdhender.com> — standalone G1 extraction,
  CMake build, golden-test harness, and the C11/64-bit modernization effort.

## AI assistants

The modernization work (warning-ladder phases, prototype conversion, golden
harness, documentation) was carried out with help from AI coding assistants:

- **Amp** (Sourcegraph) — <https://ampcode.com>
- **Claude** (Anthropic) — <https://www.anthropic.com>

All AI-assisted changes were reviewed and accepted by the human maintainer, and
were held to the project's golden-output contract (see
[BUILD_HISTORY.md](BUILD_HISTORY.md)).
