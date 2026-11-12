# FIXME

## evdev: motion/wheel consumption is a no-op under grab+reinject

### Problem

In the evdev backend's grab+reinject model, `hook_device_proc()`
(`src/evdev/input_hook.c`) reinjects each raw event through the uinput clone
individually, gated on the consume flag returned by `hook_event_proc()`. This
works for `EV_KEY` (buttons/keys), whose consume decision is made immediately.

It does **not** work for pointer motion or scroll:

- `hook_rel_proc()` only accumulates `REL_*` deltas and always returns `false`,
  so every `REL_X`/`REL_Y`/wheel event is reinjected to the clone as soon as it
  is read.
- The actual dispatch + consume decision for motion/wheel happens later, at
  `EV_SYN`, in `hook_sync_proc()`.

By the time a consumer returns "consumed" for a mouse-move or wheel event, the
underlying `REL_*` events have already been written to the clone. Consuming
motion/scroll therefore has no suppression effect.

Worse: if motion consumption *did* take effect as currently structured, it would
suppress only the `EV_SYN` while the `REL_X`/`REL_Y` were already emitted,
leaving the clone with unflushed relative deltas (a `SYN`-less frame). So the
current model cannot cleanly suppress motion regardless.

### Why it can't just dispatch earlier

A move needs `REL_X` + `REL_Y` aggregated, and hi-res wheel needs accumulation —
inherently a per-frame (`SYN`-boundary) operation. The fix must go the other
way: defer `REL` reinjection to the `SYN` boundary.

### Proposed fix — per-frame REL buffering

1. Add a small fixed pending-`REL` buffer to `struct input_hook` (raw
   `code`/`value` pairs; a frame realistically holds <= 8).
2. In `hook_device_proc()`:
   - `EV_KEY` and everything else: dispatch + reinject inline, as today.
   - `EV_REL`: push the raw event to the pending buffer; do **not** write it.
   - `EV_SYN`: after `hook_sync_proc()` returns, if **not** consumed, flush the
     buffered `REL_*` (applying the existing DPI scaling to `REL_X`/`REL_Y`)
     followed by the `SYN`; if consumed, drop the buffer.

This keeps the DPI/carry logic intact (still operating on individual
`REL_X`/`REL_Y` events), preserves event fidelity, and makes motion/wheel
consumption actually work — while never desyncing the clone, because the
`REL`s and their `SYN` are now committed or dropped together.

### Simplification

Treat consume at frame granularity: if a `SYN` frame carries both a move and a
wheel and only one is consumed, the whole frame's `REL`s drop together. Motion
and wheel almost never share a frame (a mouse reports one or the other per
packet), so tracking them separately isn't worth the complexity. If precision is
needed later, `hook_sync_proc()` can return separate move/wheel consume flags and
each `REL` code can be gated by type.
