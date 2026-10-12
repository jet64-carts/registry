# The Jet64 runtime headers (reference copy)

These are the `jet_` API headers of the Jet64 runtime — the complete surface a
cartridge may touch, published (per the ratified decision in
`docs/cart_exchange/cart_exchange.md` §13.2) so that:

* **registry CI can hold community carts to the house bar** — every submitted
  release is syntax-checked per unit with
  `clang -fsyntax-only -std=c11 -Wall -Wextra -Wshadow -Wpedantic` against
  exactly these headers;
* **authors can run the same check at home**, and read the real, documented
  API the sandbox exposes rather than guessing at it.

The layout mirrors the runtime's, so a cart's own include paths resolve
unchanged: `jet64.h` and the `jet_*.h` modules sit at the root, and the
developer terminal — which a cart reaches as `#include "terminal/jet_terminal.h"`
— sits in `terminal/` beside them. Anything a cart may **not** include is named
for what it is (`*_internal.h`) and is not here at all.

Two things these headers are **not**:

* They are not the runtime. The implementation (`.c`), the compiler, the
  hypervisor, and the Workstation remain part of the closed Jet64 product.
* They are not a workshop. Inside Jet64 the `jet_` API is read-only to carts —
  it *is* the sandbox boundary. Editing a copy here changes nothing about what
  the console enforces.

They are updated by the maintainers in lockstep with `JET64_API_GENERATION`;
the contract is frozen and grows additively, so a cart declaring
`jet64_api = 1` compiles against every later snapshot.

**Licence:** © 2026 73v3. Verbatim redistribution of these header files with
this notice is permitted; use for compiling, syntax-checking, and studying
Jet64 cartridges is welcome and intended. (Final licence wording rests with
the author — if you need broader rights, ask.)
