#!/usr/bin/env python3
"""Compare source/render motion around markers; timings do not measure scanout."""
import argparse,csv,json,math,statistics
from pathlib import Path

def angle(a,b,prefix):
    qa=[float(a[prefix+k]) for k in ('x','y','z','w')]
    qb=[float(b[prefix+k]) for k in ('x','y','z','w')]
    norm=math.sqrt(sum(x*x for x in qa)*sum(x*x for x in qb))
    if norm<1e-12:return None
    return math.degrees(2*math.acos(min(1.,abs(sum(x*y for x,y in zip(qa,qb))/norm))))

def stats(values):
    if not values:return None
    v=sorted(values)
    return dict(n=len(v),median=statistics.median(v),p99=v[int((len(v)-1)*.99)],minimum=v[0],maximum=v[-1])

def summarize(rows,start,end):
    source=[r for r in rows if r['ready']=='1' and r['source_changed']=='1']
    source_speeds=[];source_angles=[];repeats=0;source_pairs=0;gaps=0
    for a,b in zip(source,source[1:]):
        if not start<=float(b['wall_s'])<end or a['phase']!=b['phase']:continue
        delta=(int(b['sequence'])-int(a['sequence']))&0xffffffff
        if delta!=1:
            if 1<delta<0x80000000:gaps+=delta-1
            continue
        dt=(int(b['source_cycles'])-int(a['source_cycles']))/33868800
        if not 0<dt<.15:continue
        source_pairs+=1
        repeats+=all(a[k]==b[k] for k in [*(f'source_camera_{c}' for c in 'xyz'),*(f'source_matrix_{i}' for i in range(9))])
        source_speeds.append(math.sqrt(sum((float(b['source_camera_'+c])-float(a['source_camera_'+c]))**2 for c in 'xyz'))/dt)
        theta=angle(a,b,'source_q')
        if theta is not None:source_angles.append(theta/dt)
    speeds=[];angles=[];swap_intervals=[];holds=0
    for a,b in zip(rows,rows[1:]):
        if not start<=float(b['wall_s'])<end or a['ready']!='1' or b['ready']!='1':continue
        if int(a['marker']) or int(b['marker']):continue
        if a['phase']!=b['phase']:continue
        dt=float(b['sample_monotonic_s'])-float(a['sample_monotonic_s'])
        if dt<=0:continue
        speeds.append(math.sqrt(sum((float(b['camera_'+c])-float(a['camera_'+c]))**2 for c in 'xyz'))/dt)
        theta=angle(a,b,'render_q')
        if theta is not None:angles.append(theta/dt)
        swap_intervals.append(1000*(float(b['swap_end_s'])-float(a['swap_end_s'])))
        holds+=int(b['interpolation_held'])
    return dict(start_s=start,end_s=end,source_pairs=source_pairs,repeated_source_poses=repeats,skipped_source_snapshots=gaps,
                source_speed_units_per_guest_second=stats(source_speeds),source_turn_degrees_per_guest_second=stats(source_angles),
                rendered_speed_units_per_wall_second=stats(speeds),rendered_turn_degrees_per_wall_second=stats(angles),
                swap_return_interval_ms=stats(swap_intervals),interpolation_holds=holds)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('csv',type=Path);p.add_argument('--before',type=float,default=4);a=p.parse_args()
    if a.before<=0:p.error('--before must be positive')
    with a.csv.open() as f:rows=list(csv.DictReader(f))
    if not rows or 'source_matrix_0' not in rows[0]:p.error('This recording predates camera tracing; use the updated Mac build.')
    markers=[r for r in rows if int(r['marker'])]
    windows=[dict(marker=int(r['marker']),**summarize(rows,max(0,float(r['wall_s'])-a.before),float(r['wall_s']))) for r in markers]
    if not windows:windows=[summarize(rows,0,float(rows[-1]['wall_s'])+1)]
    print(json.dumps(dict(windows=windows,telemetry_dropped=int(rows[-1]['telemetry_dropped']),
                         note='Source poses are consumed render-boundary snapshots, not physics ticks. Swap return is not physical scanout. Marked capture frames and their successors are excluded from rendered-motion statistics.'),indent=2))
if __name__=='__main__':main()
