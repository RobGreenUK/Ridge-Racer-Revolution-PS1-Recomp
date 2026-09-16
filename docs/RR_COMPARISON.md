# Review against current Ridge Racer

This comparison uses Ridge Racer source revision
`cd0ee7ac60568a74bdbfaab9d927b5e7aea93bec` and the maintained Revolution source.
It is an implementation review, not a claim of equivalent game behaviour.
Ridge Racer targets SCUS-94300; Revolution targets SLUS-00214 and a separate
racing executable. Addresses, model classifications and IPC are not interchangeable.

## Shared improvements already present

| Area | Revolution evidence and conclusion |
|---|---|
| Texture invalidation/decoding | `src/scene/texture_data.h` matches RR; resident palette handling, shared page signatures and palette expansion are already present. `tests/test_texture_updates.py` covers output parity. |
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
| Static bounds and sorting optimizations | RRR still walks its own geometry and uses stable material sorting. These are optimization candidates, not established correctness faults. | Profile actual workload and preserve visibility/layer order in matched renders. |
| Windows transport/launcher | RRR's mapped snapshot ABI and Swift launcher differ from RR. | Implement and test the native Windows pipeline described in the Windows scope guide. |

These are review findings and next verification steps, not completed ports or an
issue-history dump. The first release preserves the working RRR paths rather than
substituting unverified RR addresses. Keep findings current as RRR-specific tests
and gameplay evidence become available.
