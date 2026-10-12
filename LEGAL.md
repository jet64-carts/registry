# Legal policy — takedowns, yanks, and appeals

Short, public, and applied with as much kindness as the law allows.

## Complaints

Copyright, trademark, or other rights complaints about a listed cart: open an
issue on this repository titled `takedown: <cart-id>` (or use a private
security advisory if the material itself should not be linked publicly).
Include what is infringed, where, and your standing to complain. A maintainer
acknowledges, and if the complaint is facially valid the release is yanked
while it is resolved — yanking is cheap, reversible, and fair to both sides.

## What yanking is

A yanked release is removed from **default discovery**: it stops appearing in
browse and search, existing installs are warned, and the record itself is
preserved (the registry's history is its audit trail). Yanking is not deletion
and not a verdict — it is the board taking a file off the shelf while people
talk.

## Prohibited content

The board refuses: malware or credential theft in any form; carts that
misrepresent their capabilities (the INFO screen's ACCESS line is checked
against the source, both directions); content unlawful to distribute;
harassment dressed as software. Maintainers may also reject legally uncertain
submissions that pass every automated check — admission is curation, not a
right.

## Abandoned and compromised upstreams

If an author's repository vanishes, installed carts keep working and the entry
is marked `unavailable` — not yanked, because nothing was wrong. Where the
licence permits redistribution, a maintainer may re-point the release at a
community mirror; CI accepts the re-point only if the mirror's tree
fingerprint is byte-identical to the reviewed release, so a mirror can restore
availability but can never alter content. A compromised upstream (history
rewritten under a reviewed pin) costs nothing retroactively — the pinned
digest no longer resolves, which fails closed — and future releases from it
need fresh review.

## Ownership transfer and appeals

Cart IDs are never recycled. Transfers of an existing ID are recorded by pull
request with both parties' consent (or clear evidence the namespace's owner
consents). Appeals of any decision here: one issue, one calm statement, and a
maintainer will answer when the batched review day comes. Decisions rest with
the sysop; the registry is forkable precisely so that this authority is never
a cage.
