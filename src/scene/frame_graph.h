#pragma once
#include <deque>
#include <cstdio>
// Presentation history is retained in seconds, independent of the FPS target.
struct FrameGraph {
    struct Point{double time,ms;};std::deque<Point> points;
    void add(double time,double ms){if(ms>0)points.push_back({time,ms});while(!points.empty()&&points.front().time<time-5)points.pop_front();}
    void draw(SDL_Renderer*r,int width,int height,double time,double target){
        float scale=std::max(1.f,height/900.f),w=std::min(width-24.f,460*scale),h=140*scale,x=12,y=height-h-12;
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,0,0,0,215);SDL_FRect area{x,y,w,h};SDL_RenderFillRect(r,&area);
        const float plotTop=y+40*scale,plotBottom=y+h-18*scale,plotLeft=x+8*scale,plotRight=x+w-8*scale;
        double ceiling=std::max(33.333,2000./target),sum=0,peak=0;for(auto p:points){sum+=p.ms;peak=std::max(peak,p.ms);}
        auto ordinate=[&](double ms){return plotBottom-float(std::min(ms/ceiling,1.))*(plotBottom-plotTop);};
        SDL_SetRenderDrawColor(r,120,140,160,170);SDL_RenderLine(r,plotLeft,ordinate(1000./target),plotRight,ordinate(1000./target));
        bool have=false;SDL_FPoint prior{};for(auto p:points){SDL_FPoint point{plotRight-float((time-p.time)/5)*(plotRight-plotLeft),ordinate(p.ms)};SDL_SetRenderDrawColor(r,p.ms>1500./target?255:70,p.ms>1500./target?100:220,100,255);if(have)SDL_RenderLine(r,prior.x,prior.y,point.x,point.y);prior=point;have=true;}
        char text[160];double mean=points.empty()?0:sum/points.size();std::snprintf(text,sizeof text,"%.1f FPS | %.2f ms avg | %.2f ms max",mean?1000/mean:0,mean,peak);
        float oldX,oldY;SDL_GetRenderScale(r,&oldX,&oldY);SDL_SetRenderScale(r,scale,scale);
        SDL_SetRenderDrawColor(r,240,240,240,255);
        SDL_RenderDebugText(r,x/scale+8,y/scale+8,"DEV FRAME TIMES  [G] hide  [P] capture");
        SDL_RenderDebugText(r,x/scale+8,y/scale+22,text);
        SDL_RenderDebugText(r,plotLeft/scale,plotBottom/scale+5,"-5 seconds");SDL_RenderDebugText(r,plotRight/scale-24,plotBottom/scale+5,"now");
        SDL_SetRenderScale(r,oldX,oldY);
    }
};
