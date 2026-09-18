# Contributing

Two people work on this: one on the backend, one on the frontend. This file is
about how that works in practice. For how the code is arranged, read
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## Who owns what

| Area | Owner | Guide |
|---|---|---|
| `src/**`, `src/CMakeLists.txt` | Backend | [docs/BACKEND_GUIDE.md](docs/BACKEND_GUIDE.md) |
| `qml/**`, `qml/CMakeLists.txt` | Frontend | [docs/FRONTEND_GUIDE.md](docs/FRONTEND_GUIDE.md) |
| `docs/QML_API_REFERENCE.md` | **Both** | changes here need both people to agree |
| Top-level `CMakeLists.txt`, `packaging/`, `.github/` | Backend by default | small and rarely touched |
| `third_party/` | Nobody | vendored; do not edit in place |

The split is deliberate: the two halves are in different directories and are
built as separate libraries, so ordinary work never puts you both in the same
file. If you find yourself needing to edit the other person's area to get your
change done, that is worth a conversation — it usually means the contract
between the halves needs a small addition rather than a workaround.

---

## The one shared thing

`docs/QML_API_REFERENCE.md` describes every method QML may call on `Backend`.
It is the seam, and it is the only place the two of you are coupled.

**Adding to it is cheap.** The frontend needs something new; the backend adds a
method and a row in the reference. Nothing existing breaks.

**Changing it is not.** QML resolves these names at runtime, so renaming a
method compiles perfectly and fails when a user clicks a button. If a name has
to change:

1. Agree on it first.
2. Change the C++ and every `.qml` call site in the **same commit**.
3. Update the reference in that commit too.

Most record-shaped methods take a `QVariantMap`, so adding a *field* to a
record needs no API change at all — the frontend just starts sending another
key. Prefer that over a new method.

---

## Before you push

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
timeout 10 ./build/EnsteinStockManager 2>&1 | grep -iE "error|segmentation|unavailable|is not defined"
```

A clean run prints nothing from that grep and shows a `Loaded N …` line per
table. This is a low bar, but it catches the two failure modes that matter
most: a crash at startup, and a QML file that does not resolve.

Frontend work should also pass:

```bash
cmake --build build --target all_qmllint
```

Ignore `Unqualified access` — it is pre-existing noise. Anything saying **`was
not found`**, **`is not a type`** or **`unavailable`** is a real problem.

> **There are no automated tests yet.** The domain services were extracted
> specifically so they become testable: each takes an `AppContext` and knows
> nothing about QML, so a test can build one against an in-memory SQLite
> database. Adding the first one would be a genuinely valuable contribution.

---

## Working against real data

The application connects to a shared PostgreSQL server if one is configured, and
falls back to a local SQLite file otherwise. When it falls back, the UI shows a
permanent red banner — working alone when you meant to be sharing is the worst
failure mode here, so it is made loud rather than convenient.

For day-to-day development the local fallback is fine. Before releasing
anything that touches the schema, test against a database that **predates** your
change: a fresh database only exercises the `CREATE TABLE`, and never runs your
migration. See
[docs/DATABASE_SCHEMA.md](docs/DATABASE_SCHEMA.md#migrations).

---

## Commits

History is direct to `main`; there are no feature branches or pull requests.
That works because the two of you are in different directories.

Write the message for whoever reads it in six months:

- A subject line that says what changed, not which files.
- A body explaining **why**, and anything surprising you found. If you worked
  around something, say what and why — that is the part nobody can recover from
  the diff.
- Keep one commit to one coherent change. A refactor and a feature in the same
  commit cannot be reverted separately.

---

## Code conventions

Both languages, same principle: **comments explain why, the code says what.**

```cpp
// Without an explicit connect timeout the operating system alone decides how
// long a dead server is waited for, and it is patient: Linux retries the TCP
// handshake for over two minutes. That wait happens while the app is starting,
// so the window cannot appear until it ends.
```

That comment is worth keeping. `// set the timeout` would not be.

**C++** — one class per file, named after the file. `m_` on members. Include by
path from `src/` (`#include "core/dbmanager.h"`). A service's header should
explain what area of the business it owns and what depends on it, so a reader
never has to open the `.cpp` to orient themselves.

**QML** — one component per file, `PascalCase.qml`, listed in
`qml/CMakeLists.txt`. Inputs (`property`) and outputs (`signal`) at the very top
so the contract is visible without scrolling. Never a colour literal — use
`Theme`. Never hand-formatted money — use `Format`. Size panels from
`implicitHeight` rather than fixed numbers, so adding a field later cannot push
content out of sight.

---

## Releasing

Tagging `v*` builds the Windows installer, the portable exe and the Linux
AppImage in CI. There is no local Windows toolchain, so a tag is the only way to
get an `.exe`. See [docs/RELEASING.md](docs/RELEASING.md).
