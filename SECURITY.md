# Security policy

## Reporting a vulnerability

If you believe a listed cart is malicious, that a release's integrity is
compromised, or that the validator itself has an exploitable flaw, please open
a **private security advisory** on this repository (GitHub → Security →
"Report a vulnerability"). If the matter is urgent and the advisory route is
unavailable to you, open an ordinary issue saying only *"security — please
contact me"* and a maintainer will reach out; never post exploit details in a
public issue.

## What happens next

* A confirmed malicious or compromised release is **yanked immediately** —
  removed from default discovery, warned about on every screen that still
  shows it — and the incident is recorded publicly in the entry's history.
* Registry history is rewritten only for secrets or content GitHub itself
  cannot retain; everything else is yanks and amendments, in the open.
* Reports are handled on a hobbyist's clock, but yanks jump every queue.

## The honest scope

The Exchange distributes **source only**, and Jet64's own protections do not
trust this registry: every install re-validates digests and hygiene, every
compile re-runs the sandbox's forbidden-symbol scan, and every cart executes
inside a crash-contained child process. A registry compromise is a serious
integrity problem — it is not a code-execution path on anyone's machine, and
the design keeps it that way (docs: `cart_exchange.md` §7).
