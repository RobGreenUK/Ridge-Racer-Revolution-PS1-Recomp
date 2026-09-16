#!/usr/bin/env python3
"""Summarize local native-renderer CSV timings; default to the latest launch."""
import argparse
import csv
import json
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[1]
STAGES = ('scene_ms', 'screen_ms', 'vram_ms', 'draw_ms', 'present_ms')
DETAILS = ('post_present_gap_ms','event_ms','input_ms','sleep_ms','previous_record_ms','source_age_start_ms','mesh_build_ms','texture_upload_ms','sort_ms','depth_ms','gpu_depth_ms','render_flush_ms','swap_ms','gpu_render_ms','display_wait_ms')


def summarize(path, fps):
    rows = []
    with Path(path).open() as stream:
        for row in csv.DictReader(stream):
            try:
                values = {key: float(value) for key, value in row.items()}
                if all(key in values for key in (*STAGES, 'interval_ms', 'marker', 'marker_ms', 'ready', 'wall_s', 'camera_x', 'camera_y', 'camera_z', 'sequence', 'frame', 'source_age_ms')):
                    rows.append(values)
            except (TypeError, ValueError):
                continue  # A running process may have an incomplete final row.
    presentation_path = Path(str(path)+'.presentation.json')
    presentation = json.loads(presentation_path.read_text()) if presentation_path.exists() else {}
    effective_fps = presentation.get('effective_fps', fps)
    budget = 1000 / effective_fps
    # Marker screenshots intentionally stall the GPU. Exclude their frame and
    # the following interval from pacing statistics, but preserve marker locations.
    excluded = {r['frame'] + n for r in rows if r['marker'] for n in (0, 1)}
    race = [r for r in rows if r['ready'] and r['frame'] not in excluded and r['frame'] > 0]

    def distribution(key):
        values = sorted(r[key] for r in race if key in r and r[key]>=0)
        if not values:
            return None
        return dict(mean=statistics.mean(values), p50=values[int((len(values)-1)*.5)],
                    p95=values[int((len(values)-1)*.95)], p99=values[int((len(values)-1)*.99)], maximum=values[-1])

    by_frame = {r['frame']: r for r in rows}
    worst = sorted(race, key=lambda r: r['interval_ms'], reverse=True)[:20]
    return dict(path=str(path), target_fps=fps, effective_fps=effective_fps, presentation=presentation, budget_ms=budget, recorded_frames=len(rows),
                race_frames=len(race), over_1_5_budget=sum(r['interval_ms'] > budget*1.5 for r in race),
                stale_source_frames=sum(r['source_age_ms'] > 100 for r in race),
                timings={key: distribution(key) for key in ('interval_ms', *STAGES, *DETAILS) if any(key in r for r in race)},
                telemetry_dropped=max((r.get('telemetry_dropped',0) for r in rows),default=0),
                markers=[{key: r[key] for key in ('marker', 'wall_s', 'frame', 'camera_x', 'camera_y', 'camera_z')} for r in rows if r['marker']],
                worst_frames=[{**{key: r[key] for key in ('frame', 'wall_s', 'interval_ms', 'sequence', 'source_age_ms', 'camera_x', 'camera_y', 'camera_z')},
                               # Work in the preceding frame can cause this interval.
                               'stages_ms': {key: r[key] for key in STAGES},
                               'detail_ms': {key:r[key] for key in DETAILS if key in r},
                               'preceding_stages_ms': {key: by_frame.get(r['frame']-1, {}).get(key) for key in STAGES}} for r in worst])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', nargs='?', type=Path)
    parser.add_argument('--fps', type=float)
    args = parser.parse_args()
    if args.csv is None:
        files = sorted((ROOT/'diagnostics/frame-times').glob('*/frames.csv'))
        if not files:
            parser.error('No frame-time recordings yet; launch native scene mode first')
        args.csv = files[-1]
    settings = args.csv.parent/'settings.json'
    fps = args.fps or (json.loads(settings.read_text())['nativeFps'] if settings.exists() else 60)
    if fps == 0:
        presentation_path=Path(str(args.csv)+'.presentation.json')
        fps=json.loads(presentation_path.read_text()).get('effective_fps',60) if presentation_path.exists() else 60
    if not 1 <= fps <= 360:
        parser.error('fps must be 1..360')
    result = summarize(args.csv, fps)
    output = args.csv.with_suffix('.summary.json')
    output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
