"""A blocked telemetry sink must never block the frame producer."""
import csv
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
HARNESS=r'''
#include "frame_metrics.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <fstream>
int main(int argc,char**argv){
    assert(mkfifo(argv[1],0600)==0);
    int reader=open(argv[1],O_RDONLY|O_NONBLOCK);assert(reader>=0);
    std::string collected;std::thread drain;
    {
        FrameMetrics metrics(argv[1]);
        FrameDetail detail;detail.sourceCamera={-123,456,789};detail.sourceMatrix={4096,0,0,0,4096,0,0,0,4096};detail.sourceRotation={0,0,0,1};detail.renderRotation={0,0,1,0};detail.sourceCycles=1234567890123ULL;detail.sourcePublished=123.25;detail.sampleWall=124.5;detail.swapStartWall=1.125;detail.swapEndWall=1.25;detail.bracketStart=12.25;detail.bracketEnd=12.5;detail.sourceChanged=1;detail.sourceStep=33.3667;detail.publicationStep=34.5;detail.sourceGame=12.5;detail.interpolationAlpha=.25;detail.interpolationTarget=12.4;detail.held=1;detail.sequenceGaps=2;
        for(unsigned i=0;i<100000;i++)metrics.record(i,i/120.,8,0,1,2,3,4,5,true,i,6,1,7,1,2,3,4,0,0,detail);
        // No reader drained the FIFO during these calls: a blocking producer
        // would hang before reaching this assertion/start of the drain thread.
        assert(metrics.dropped.load()>0);
        fcntl(reader,F_SETFL,0);
        drain=std::thread([&]{char buffer[8192];ssize_t n;while((n=read(reader,buffer,sizeof buffer))>0)collected.append(buffer,n);close(reader);});
    }
    drain.join();std::ofstream out(argv[2]);out<<collected;
}
'''
class AsyncMetricsTests(unittest.TestCase):
    def test_blocked_writer_drops_telemetry_and_drains_cleanly(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'test.cpp').write_text(HARNESS)
            subprocess.run(['c++','-std=c++17','-O2','-pthread','-I'+str(ROOT/'src/scene'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
            subprocess.run([str(p/'test'),str(p/'pipe'),str(p/'out.csv')],timeout=15,check=True,capture_output=True)
            with (p/'out.csv').open() as stream:rows=list(csv.DictReader(stream))
            self.assertGreater(len(rows),0)
            self.assertLess(len(rows),100000)
            ids=[int(r['frame']) for r in rows]
            self.assertEqual(ids,sorted(set(ids)))
            self.assertTrue(all(r['sequence']==r['frame'] and None not in r for r in rows))

            self.assertTrue(all(r['source_changed']=='1' and r['interpolation_held']=='1' and r['sequence_gaps']=='2' for r in rows))
            self.assertTrue(all(float(r['source_step_ms'])==33.3667 and float(r['publication_step_ms'])==34.5 for r in rows))
            self.assertTrue(all(float(r['source_game_s'])==12.5 and float(r['interpolation_alpha'])==.25 and float(r['interpolation_target_s'])==12.4 for r in rows))

            self.assertTrue(all(r['source_camera_x']=='-123' and r['source_matrix_8']=='4096' and r['source_cycles']=='1234567890123' for r in rows))
            self.assertTrue(all(float(r['source_qw'])==1 and float(r['render_qz'])==1 and float(r['sample_monotonic_s'])==124.5 and float(r['bracket_end_game_s'])==12.5 for r in rows))
            self.assertTrue(all(None not in r and all(v is not None for v in r.values()) for r in rows))
