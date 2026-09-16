#pragma once
#include "timeline.h"
#include <deque>
// Keep presentation advancing continuously rather than restarting on receipt.
// A short buffer absorbs the fractional producer/display sampling relationship.
struct PresentationSampleInfo { bool held=false;double alpha=-1,target=0,bracketStart=-1,bracketEnd=-1; };
struct PresentationTimeline {
    std::deque<ridge::Frame> frames;double offset=0,interval=1./30;
    void clear(){frames.clear();offset=0;interval=1./30;}
    void push(ridge::Frame frame,double produced){
        double observed=frame.time-produced;
        if(frames.empty())offset=observed;
        else {
            double step=frame.time-frames.back().time;
            if(step<=0||step>.15||frame.flags!=frames.back().flags){clear();offset=observed;}
            else {
                // Correct slow clock drift, not per-frame scheduling noise.
                offset+=std::clamp(observed-offset,-.005,.005)*.01;
                if(step>interval*.5&&step<interval*1.5)interval+=(step-interval)*.05;
            }
        }
        frames.push_back(std::move(frame));while(frames.size()>8)frames.pop_front();
    }
    ridge::Frame at(double now,double displayInterval,PresentationSampleInfo*info=nullptr)const{
        if(info)*info={};
        if(frames.empty())return {};
        double time=now+offset-interval-displayInterval;
        if(info)info->target=time;
        if(time<=frames.front().time){if(info){info->held=time<frames.front().time;info->bracketStart=info->bracketEnd=frames.front().time;}return frames.front();}
        auto upper=std::upper_bound(frames.begin(),frames.end(),time,[](double t,const ridge::Frame&f){return t<f.time;});
        if(upper==frames.end()){if(info){info->held=time>frames.back().time;info->bracketStart=info->bracketEnd=frames.back().time;}return frames.back();}
        auto result=ridge::interpolate(*(upper-1),*upper,time);
        if(info){info->bracketStart=(upper-1)->time;info->bracketEnd=upper->time;info->alpha=(time-(upper-1)->time)/(upper->time-(upper-1)->time);info->held=result.time!=time;}
        return result;
    }
};
