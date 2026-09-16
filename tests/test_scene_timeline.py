"""Verify motion interpolation independently of the guest physics engine."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
HARNESS = r'''
#include "timeline.h"
#include "presentation_timeline.h"
#include <cassert>
#include <set>
using namespace ridge;
int main() {
    Frame a{0,3,{0,0,0},{0,0,0,1},{0,0,0},3.1f};
    Frame b{1./30,3,{10,0,0},{0,0,0,-1},{20,0,0},-3.1f};
    auto f=interpolate(a,b,1./60);
    assert(std::abs(f.camera.x-5)<1e-5);
    assert(std::abs(f.car.x-10)<1e-5);
    assert(std::abs(f.rotation.w-1)<1e-5); // q and -q represent same rotation
    assert(std::abs(f.yaw-3.14159265f)<1e-4); // shortest path across wrap
    b.flags=4; assert(interpolate(a,b,1./60).camera.x==0);
    assert(interpolate(a,b,1./30).camera.x==10); // cut only at its timestamp
    b.flags=3;b.camera.x=5000;assert(interpolate(a,b,1./60).camera.x==0);
    b.camera.x=10;b.time=.3;assert(interpolate(a,b,.1).camera.x==0);
    a.models={{uint64_t(1)<<32|12,0,{0,0,0},{2,0,0,0,2,0,0,0,2}}};
    b.models={{uint64_t(1)<<32|12,0,{10,0,0},{-2,0,0,0,2,0,0,0,-2}}};b.time=1./30;
    auto turn=interpolate(a,b,1./60).models.front();
    assert(std::abs(turn.position.x-5)<1e-5);
    assert(std::abs(turn.matrix[0])<1e-5);
    assert(std::abs(std::abs(turn.matrix[6])-2)<1e-5); // no shrinking
    b.models[0].key=99;
    assert(interpolate(a,b,1./60).models[0].position.x==0); // never blend different objects
    assert(interpolate(a,b,b.time).models[0].key==99);
    // A wheel model is reused for front and rear at the same guest call site.
    auto front=modelKey(0x801f9b18,0x8001bd34,1),rear=modelKey(0x801f9b18,0x8001bd34,3);
    assert(front!=rear);
    auto basis=std::array<float,9>{1,0,0,0,1,0,0,0,1};
    a.models={{front,43,{0,0,0},basis},{rear,43,{0,0,80},basis}};
    b.models={{rear,45,{10,0,80},basis},{front,45,{10,0,0},basis}};
    auto wheels=interpolate(a,b,1./60).models;
    assert(wheels[0].position.x==5&&wheels[0].position.z==0);
    assert(wheels[1].position.x==5&&wheels[1].position.z==80);
    a.sky={0,4090,0,0,1,0x808080,1};b.sky={0,6,0,0,2,0xffffff,1};
    a.hud={1,2};b.hud={3,4};a.hudDisplayY=240;b.hudDisplayY=0;
    auto skyMid=interpolate(a,b,1./60);
    assert(std::abs(skyMid.sky.yaw-4096)<1e-5);
    assert(skyMid.sky.clut==1 && skyMid.hud==a.hud);
    assert(skyMid.hudDisplayY==240);assert(interpolate(a,b,b.time).hudDisplayY==0);
    assert(interpolate(a,b,b.time).hud==b.hud);
    b.sky.mirror=1;assert(interpolate(a,b,1./60).sky.yaw==4090);
    // Panorama follows tile identity through reordering/wrap, at fractional
    // pixels. Garbage upper UV halfwords are not part of the tile identity.
    std::vector<uint32_t> packets={9,0x2c808080,0,0x7b850000,64,0x150040,0x00800000,0x12347f00,0x00800040,0x56787f40};
    auto panorama=decodeBackground(packets);assert(panorama.size()==1);
    assert(panorama[0].page==21&&panorama[0].clut==0x7b85);
    auto shifted=panorama;for(auto&v:shifted[0].xy){v[0]+=7;v[1]+=1;}
    BackgroundQuad another=shifted[0];another.page=22;shifted.insert(shifted.begin(),another);
    a.background=panorama;b.background=shifted;b.flags=a.flags;
    auto middle=interpolate(a,b,1./60).background;
    assert(middle.size()==1&&middle[0].xy[0][0]==3.5f&&middle[0].xy[0][1]==.5f);
    assert(interpolate(a,b,b.time).background.size()==2);
    b.flags=a.flags+1;assert(interpolate(a,b,1./60).background[0].xy[0][0]==0);b.flags=a.flags;
    for(int rate:{60,75,120,144}) {
        float previous=-1;
        for(int tick=0;tick<rate/30;tick++) {
            auto sky=interpolate(a,b,double(tick)/rate).background;
            assert(sky[0].xy[0][0]>previous);previous=sky[0].xy[0][0];
        }
    }
    // Polling at 75/144 Hz must not reset the interpolation phase to zero
    // whenever a 29.97 Hz producer publishes a frame between display ticks.
    for(int fps:{75,144}) {
        constexpr double step=1001./30000;
        double prior=-1;
        for(int tick=20;tick<fps*3;tick++){
            double now=double(tick)/fps;int sequence=int(now/step);
            Frame before{},after{};before.time=(sequence-1)*step;after.time=sequence*step;
            before.camera.x=before.time*100;after.camera.x=after.time*100;
            auto pose=interpolate(before,after,before.time+(now-after.time));
            if(prior>=0)assert(std::abs(pose.camera.x-prior-100./fps)<.001);
            prior=pose.camera.x;
        }
    }
    for(int fps:{75,144}) {
        PresentationTimeline playback;int next=0;float last=0;bool haveLast=false;
        constexpr double sourceStep=1001./30000;
        for(int tick=0;tick<fps*5;tick++){
            double now=double(tick)/fps;
            while(next*sourceStep+.003*std::sin(next*1.7)<=now){
                Frame frame{};frame.time=next*sourceStep;frame.camera.x=frame.time*100;
                playback.push(frame,next*sourceStep+.003*std::sin(next*1.7));next++;
            }
            auto frame=playback.at(now,1./fps);
            if(now>1&&haveLast)assert(std::abs(frame.camera.x-last-100./fps)<.05);
            last=frame.camera.x;haveLast=true;
        }
    }
    // Variable decode work must not move the time represented by a display tick.
    // Compare the old post-decode sampling with the latched timestamp at each rate.
    for(int fps:{60,75,120,144}) {
        PresentationTimeline playback;int next=0;double last=0,oldLast=0,maxError=0,oldMaxError=0;
        for(int tick=0;tick<fps*4;tick++) {
            double latched=double(tick)/fps;bool changed=false;
            while(next/30.<=latched){Frame f{};f.time=next/30.;f.camera.x=f.time*100;playback.push(f,f.time);next++;changed=true;}
            double work=changed?.0015:.000005;
            auto fixed=playback.at(latched,1./fps),old=playback.at(latched+work,1./fps);
            if(tick>fps){maxError=std::max(maxError,std::abs(fixed.time-last-1./fps));oldMaxError=std::max(oldMaxError,std::abs(old.time-oldLast-1./fps));}
            last=fixed.time;oldLast=old.time;
        }
        assert(maxError<1e-9);assert(oldMaxError>.001);
    }
    {
        PresentationTimeline playback;Frame a{},b{};a.time=1;b.time=1+1./30;b.camera.x=10;
        playback.push(a,1);playback.push(b,b.time);PresentationSampleInfo info;
        auto middle=playback.at(1+1./60+1./30+1./75,1./75,&info);
        assert(info.bracketStart==a.time&&info.bracketEnd==b.time);
        assert(!info.held&&std::abs(info.alpha-.5)<1e-9&&std::abs(middle.camera.x-5)<1e-5);
        playback.at(2,1./75,&info);assert(info.held);
        playback.at(0,1./75,&info);assert(info.held);
        playback.clear();playback.at(0,1./75,&info);assert(!info.held&&info.alpha==-1);
        a.camera.x=0;b.camera.x=5000;playback.push(a,1);playback.push(b,b.time);
        playback.at(1+1./60+1./30+1./75,1./75,&info);assert(info.held); // camera cut
    }
    {
        // Distinct replay shots can share a camera position. Cut on identity,
        // not just distance; continuous motion within a shot still interpolates.
        Frame a{},b{};a.flags=b.flags=32;a.time=1;b.time=1+1./30;
        a.cameraIdentity={1,0,0x8007c438,7};b.cameraIdentity=a.cameraIdentity;
        b.rotation={0,1,0,0};
        assert(std::abs(interpolate(a,b,1+1./60).rotation.w)<.8);
        for(int field=1;field<4;field++){
            b.cameraIdentity=a.cameraIdentity;b.cameraIdentity[field]++;
            assert(interpolate(a,b,1+1./60).rotation.w==1);
            assert(interpolate(a,b,b.time).rotation.w==0);
            PresentationTimeline t;t.push(a,a.time);t.push(b,b.time);
            auto held=t.at(1+1./60+1./30+1./75,1./75);
            assert(held.rotation.w==1);
        }
    }
    std::vector<Frame> frames;
    // NTSC-derived 29.97 sample rate vs exact 60/120/144 presentation.
    for(int i=0;i<=300;i++) {
        Frame x=a;x.time=i*(1001./30000);x.camera.x=x.car.x=float(x.time*100);frames.push_back(x);
    }
    for(int fps:{60,120,144}) {
        std::set<int> positions;
        for(int i=0;i<fps*10;i++) {
            double t=double(i)/fps;auto x=sample(frames,t);
            assert(std::abs(x.car.x-t*100)<.001);
            positions.insert(int(std::round(x.car.x*10000)));
        }
        assert(positions.size()==size_t(fps*10)); // real distinct transform samples
        assert(std::abs(sample(frames,10).car.x-1000)<.001); // same distance/duration
    }
    assert(sample(frames,-1).car.x==frames.front().car.x);
    assert(sample(frames,100).car.x==frames.back().car.x);
}
'''
class TimelineTests(unittest.TestCase):
    def test_pose_identity_cuts_rotations_and_presentation_rates(self):
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'test.cpp';source.write_text(HARNESS)
            binary=Path(d)/'test'
            subprocess.run(['c++','-std=c++17','-I',str(ROOT/'src/scene'),str(source),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True,capture_output=True)

    def test_motion_timestamp_is_latched_before_scene_work(self):
        code=(ROOT/'src/scene/native.cpp').read_text()
        self.assertLess(code.index('const double presentationWall=monotonicSeconds()'),code.index('bridge.poll(path,buttons'))
        self.assertIn('timeline.at(presentationWall,1/effective,&sampleInfo)',code)
        self.assertIn('mirrorTimeline.at(presentationWall,1/effective)',code)

    def test_bundled_hud_commands_and_truncated_packets(self):
        source_text=r'''#include "gp0_commands.h"
#include <cassert>
int main(){
    // Texture-page command, sprite, and polyline share one DMA packet.
    std::vector<uint32_t> packet={10,0xe1000000,0x64000000,0,0,0x00080008,0x480000ff,1,2,3,0x50005000};
    auto commands=splitDrawingCommands(packet);
    assert(commands.size()==13&&commands[0]==1&&commands[2]==4&&commands[7]==5);
    assert(commands[8]==0x480000ff&&commands.back()==0x50005000);
    assert(splitDrawingCommands({4,0x64000000,0}).empty());
    assert(splitDrawingCommands({2,0x64000000,0}).empty());
    auto line=splitDrawingCommands({4,0x500000ff,0,0x00ffffff,1});
    assert(line.size()==5&&line[0]==4);
}
'''
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'test.cpp';source.write_text(source_text)
            binary=Path(d)/'test'
            subprocess.run(['c++','-std=c++17','-I',str(ROOT/'src/scene'),str(source),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True,capture_output=True)
