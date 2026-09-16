# Review against current Ridge Racer

This comparison uses Ridge Racer source revision
`6104e3314c41f87795b4f54642d6a1cfa4b32f2d` and the maintained Revolution source.
It is an implementation review, not a claim of equivalent game behaviour.
Ridge Racer targets SCUS-94300; Revolution targets SLUS-00214 and a separate
racing executable. Addresses, model classifications and IPC are not interchangeable.

## Shared improvements already present

| Area | Revolution evidence and conclusion |
|---|---|
| Resident texture invalidation/decoding | `src/scene/texture_data.h` matches RR; resident palette handling, shared page signatures and palette expansion are already present. `tests/test_texture_updates.py` covers output parity. |
| GPU geometry batching | `depth_renderer.h` already streams a vertex buffer and batches draws; copying RR's renderer would overwrite Revolution-specific layer handling. |
| Asynchronous metrics | `frame_metrics.h` already uses a bounded writer queue. `test_async_metrics.py` checks a blocked sink; Revolution also retains detailed camera/source tracing. |
| Display callback before draw | `display_pacer.h` and `native.cpp` already wait before rendering. The separate RR phase-trial/hand-off instrumentation is not evidence that Revolution needs the same phase. |
| Stable motion sampling | Revolution latches its interpolation timestamp before snapshot/VRAM work and shares it between main/mirror views. Camera-trace tests cover this. Preserve it. |
| Wheel pose continuity | `timeline.h` already matches component pose identity across mesh animation changes. RR's car-address classifier cannot replace Revolution's owner/part/pass identity. |
| Process cleanup and settings isolation | The launcher owns both processes and uses temporary companion preferences with Revolution-only saves. |
| Read-only extended visibility | Revolution's evaluator and tests already cover private-memory execution and bounded output; RR's evaluator has different verified boundaries. |

## Changes applied for this source release

**GPU readback ordering:** imported RR's framework-level patch against the same
public SDK revision. Queued drawing must be flushed before readback consumes
GPU dirty regions, including when no CPU upload happens to force that flush.
This is a shared runtime correctness fix, not a game-address port. The real-GL
regression checks subsequent reads at 1× and 4×. This does not claim that it
resolves Revolution's tunnel-motion report.

**Reproducible SDK setup:** replaced the unpublished SDK pin with its public parent
and preserved the Revolution division-guard change as a tracked patch. Registered
its synthetic test with CTest; otherwise a clean emitter configure fails the
upstream test-registration guard. Build scripts apply patches before generation
and run the emitter helper with Bash, as required by its array syntax.

**Project independence:** disc paths are relative to this checkout. No build or
launcher requires the sibling RR checkout. Source publication excludes previous
history, private notes, media and generated game data.

**Single enhanced window and boot screens:** imported RR's hidden-companion SDK
patch and adapted its startup frame-hook approach to Revolution's mapped snapshot
transport. Galaga/boot framebuffer capture and SIO input forwarding now precede
the existing racing-executable hooks. This ports the runtime mechanism without
copying RR's guest addresses. See the architecture guide for the handoff contract.

## Fixes that need Revolution-specific work

| RR feature/fix | Why it was not copied | Required evidence before a port |
|---|---|---|
| Additional enhanced replay states | RR state numbers are unrelated to Revolution's state table; RRR currently gates enhanced scenes on 17/19. | Verify each RRR mode, camera transition, snapshot readiness and replay identity. |
| Car/shadow contact depth shader | RRR uses different parts, depth layers and a road tolerance; RR tags and hooks are game-specific. | Verify original shadow submissions and road-plane selection across courses, slopes, jumps and mirror passes. |
| Fallen-sign lifetime and replay history | RR's object addresses, state transitions and recorded indices cannot identify Revolution's objects. | Find the corresponding RRR lifecycle, prove the defect and preserve original movement/collision. |
| Widescreen HUD edge anchors | RRR has different packet parsing, minimap and rear-view overlays. | Compare 4:3 identity and all widescreen groups in race, menus and replay. |
| Mirror signage/decal corrections | A rear-view mirror pass is distinct from reflecting a course; RR's selected model IDs are not portable. | Verify RRR course-reflection and rear-view transforms, UVs, culling and controls separately. |
| RR phase trial and native handoff trace | RRR has its own pacing and camera tracing; CPU cadence alone cannot establish a scanout problem. | Controlled timing/configuration comparisons plus display evidence. |
| Windows transport/launcher | RRR's mapped snapshot ABI and Swift launcher differ from RR. | Implement and test the native Windows pipeline described in the Windows scope guide. |

These are review findings and next verification steps, not completed ports or an
issue-history dump. The first release preserves the working RRR paths rather than
substituting unverified RR addresses. Keep findings current as RRR-specific tests
and gameplay evidence become available.

## Rendering workload and smoothness review

The resident-texture correction was already present in Revolution: displaced live
VRAM page words do not dirty immutable resident texels, but live palette changes
still do. Palette expansion and reused RGBA upload storage also match RR. Valid
lighting/texture changes still require full texture uploads; suppressing those
would freeze game appearance.

The later portable RR optimizations are now also applied here:

- **Palette lookup:** a persistent key-to-texture index replaces a linear scan per
  remapped triangle. Keys retain page, CLUT, texture window and resident/live identity,
  including first-match behaviour for duplicate keys.
- **Shared change detection:** HUD, background and world updates share the same
  before/after page-signature caches for each snapshot. Update consumers before
  replacing the retained VRAM image.
- **Early visibility rejection:** conservative bounds cover groups of 64 static
  quads and each model. Test transformed bounds before triangle processing, without
  introducing a distance limit. Revolution uses its actual focal scale for both
  the main view and rear-view mirror; RR's fixed main-view projection is insufficient.
- **Material ordering:** sort by texture, bias and original face order. The explicit
  final tie-breaker preserves stable ordering without allocating stable-sort scratch
  storage in the OpenGL path. Triangle clipping already uses fixed scratch arrays.
- **Measurements:** populate existing mesh/build, upload, sort, depth and rejection
  columns. CPU values include both main and mirror draws; counters reset each
  presentation frame, including texture uploads. Alternate nonblocking render-wide
  GPU samples (main/mirror/HUD/final flush) with main-view depth samples. Delayed GPU
  results retain their original submission IDs; unavailable samples are -1.

Other RR findings do not imply an additional Revolution fix:

| Area | Revolution assessment |
|---|---|
| Per-frame Windows endpoint file reads | Not applicable to Revolution's persistent mapped POSIX transport. |
| Telemetry blocking, immediate-mode draw calls | Bounded asynchronous logging and streamed/batched geometry were already present. |
| Timing gaps, lost samples, interpolation holds | Existing lifecycle/camera/transport measurements remain; missing render substages are now populated. The mapped snapshot is atomic, so RR's partial datagram assembly does not apply. |
| Early display callback | Already waits before input and drawing, with SDL fallback. Fullscreen/display FPS/VSync conditions remain unchanged. |
| Additional 2 ms presentation-phase trial | RR keeps this in a separate experimental executable. Its comparisons did not establish a general fix; no new delay is imposed on Revolution. |
| Appearance changes freezing motion | RR's night-bit and phase 1→2→3 transitions are not Revolution's state values. Revolution publishes states 17/19 independently of palette changes; no RR state exception is transplanted. |
| Wheel mesh changes freezing poses | Revolution already interpolates by owner/site/part identity across mesh variants. |
| Companion workload, evaluator, interpolation delay | Retained in both projects' accepted implementation. No GPU bypass, physics-rate change or reduced interpolation buffer is warranted by the comparison. |
| Screenshots, graph and first-use uploads | RR also retains marked synchronous captures, graph work and first-use costs. These are not completed RR fixes available to copy. |
| Event/compositor/driver stalls | RR acceptance and CPU traces do not establish a portable remedy for every operating-system or scanout symptom. Windows reports remain separate from this Mac project. |

Validation uses synthetic OpenGL checks for conservative bounds (including mirror
projection), identical culled/unculled images, material order, palette cache reuse,
resident invalidation and upload counters. Local before/after saved-scene replay
checks must preserve pixels while comparing draw costs. Neither a stable CPU frame
graph nor faster draw work alone establishes perceptually smooth display scanout;
a full-course playtest remains necessary for a reported temporal symptom.
