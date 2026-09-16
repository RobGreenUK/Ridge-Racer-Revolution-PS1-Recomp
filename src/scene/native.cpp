/* Independently paced Revolution renderer. Original simulation remains authoritative. */
#include "timeline.h"
#include "presentation_timeline.h"
#include "shared.h"
#include <SDL3/SDL.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <csignal>
#include <time.h>
using namespace ridge;
static uint32_t u32(std::ifstream&in){uint32_t x;in.read((char*)&x,4);return x;}
static float f32(std::ifstream&in){float x;in.read((char*)&x,4);if(!std::isfinite(x))throw std::runtime_error("Invalid float");return x;}
static Vec vec(std::ifstream&in){float x=f32(in),y=f32(in),z=f32(in);return{x,y,z};}
#include "sky_renderer.h"
#include "mesh.h"
#include "hud_renderer.h"
#include "frame_metrics.h"
#include "frame_graph.h"
#include "display_pacer.h"
static volatile std::sig_atomic_t stopped=0;
static void stop(int){stopped=1;}
static double monotonicSeconds(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return double(t.tv_sec)+double(t.tv_nsec)/1e9;}
struct Bridge {
    int fd=-1;uint64_t published=0;RRVShared*shared=nullptr;std::unique_ptr<RRVSnapshot>snapshot=std::make_unique<RRVSnapshot>();
    ~Bridge(){if(shared)munmap(shared,sizeof *shared);if(fd>=0)close(fd);}
    double sourceAgeSeconds()const{
        if(!published)return 0;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);uint64_t now=uint64_t(t.tv_sec)*1000000000+t.tv_nsec;
        return now>=published?double(now-published)/1e9:0;
    }
    bool poll(const std::string&path,uint32_t buttons,bool active){
        if(!shared){
            fd=open(path.c_str(),O_RDWR);if(fd<0)return false;
            struct stat st{};if(fstat(fd,&st)||st.st_size!=sizeof(RRVShared)){close(fd);fd=-1;return false;}
            auto*p=mmap(nullptr,sizeof(RRVShared),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
            if(p==MAP_FAILED){close(fd);fd=-1;return false;}shared=(RRVShared*)p;
            if(shared->magic!=RRV_SHARED_MAGIC||shared->size!=sizeof *shared)throw std::runtime_error("Scene transport version mismatch");
        }
        if(flock(fd,LOCK_EX|LOCK_NB))return false;
        struct timespec time;clock_gettime(CLOCK_MONOTONIC,&time);shared->input_ns=uint64_t(time.tv_sec)*1000000000+time.tv_nsec;shared->input_buttons=buttons;shared->input_active=active;
        bool changed=shared->frame.sequence!=snapshot->sequence;
        if(changed){std::memcpy(snapshot.get(),&shared->frame,sizeof *snapshot);published=shared->published_ns;}
        flock(fd,LOCK_UN);
        if(changed&&(snapshot->model_count>RRV_MODEL_CAP||snapshot->hud_count>RRV_HUD_CAP||snapshot->sky_count>RRV_SKY_CAP||snapshot->screen_width>640||snapshot->screen_height>512))throw std::runtime_error("Invalid scene counts");
        return changed;
    }
};
static Quat rotation(const int16_t*m){std::array<float,9>a;for(int i=0;i<9;i++)a[i]=m[i]/4096.f;return matrixRotation(a);}
static Frame decode(const RRVSnapshot&s,bool mirror){
    Frame f{};f.time=double(s.cycles)/33868800.;f.flags=s.state;f.camera={float(s.camera[0]),float(s.camera[1]),float(s.camera[2])};f.rotation=rotation(mirror?s.mirror_matrix:s.matrix);
    Quat inverse{-f.rotation.x,-f.rotation.y,-f.rotation.z,f.rotation.w};
    for(unsigned i=0;i<s.model_count;i++){
        const auto&m=s.models[i];if(bool(m.pass)!=mirror)continue;
        ModelPose p{};p.key=modelKey(m.owner,m.site,m.part);p.model=m.model;p.paletteOffset=m.palette;
        p.position=f.camera+rotate(inverse,{int32_t(m.translation[0])/4.f,int32_t(m.translation[1])/4.f,int32_t(m.translation[2])/4.f});
        float mat[9];for(int k=0;k<9;k++)mat[k]=int16_t(m.rotation[k/2]>>((k%2)*16))/4096.f;
        for(int c=0;c<3;c++){Vec v=rotate(inverse,{mat[c],mat[c+3],mat[c+6]});p.matrix[c]=v.x;p.matrix[c+3]=v.y;p.matrix[c+6]=v.z;}
        f.models.push_back(p);
    }
    if(!mirror){f.background=decodeBackground(std::vector<uint32_t>(s.sky,s.sky+s.sky_count));f.hud.assign(s.hud,s.hud+s.hud_count);f.hudDisplayX=s.display_x;f.hudDisplayY=s.display_y;}
    return f;
}
int main(int argc,char**argv)try{
    std::string path,assets,metricsPath,shot,frozenSnapshot;unsigned width=1280,height=960;double fps=60,seconds=0,shotAt=0;std::string vsync="off";bool perspective=true,fullscreen=false,smooth=false,showGraph=false;
    for(int i=1;i<argc;i++){
        std::string arg=argv[i];if(arg=="--frame-graph"){showGraph=true;continue;}if(arg=="--affine"){perspective=false;continue;}if(arg=="--fullscreen"){fullscreen=true;continue;}
        if(i+1==argc)throw std::runtime_error("Missing option value");std::string v=argv[++i];
        if(arg=="--snapshot")frozenSnapshot=v;else if(arg=="--shared")path=v;else if(arg=="--assets")assets=v;else if(arg=="--metrics")metricsPath=v;else if(arg=="--width")width=std::stoul(v);else if(arg=="--height")height=std::stoul(v);else if(arg=="--fps")fps=std::stod(v);else if(arg=="--seconds")seconds=std::stod(v);else if(arg=="--shot")shot=v;else if(arg=="--shot-at")shotAt=std::stod(v);else if(arg=="--filter")smooth=v=="bilinear";else if(arg=="--vsync")vsync=v;else throw std::runtime_error("Unknown argument: "+arg);
    }
    if((path.empty()&&frozenSnapshot.empty())||assets.empty()||width<320||width>7680||height<240||height>4320||!std::isfinite(fps)||(fps!=0&&fps<30)||fps>360||!std::isfinite(seconds)||seconds<0)throw std::runtime_error("Invalid native settings");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
    SDL_Window*w;SDL_Renderer*r;if(!SDL_CreateWindowAndRenderer("Ridge Racer Revolution — native renderer preview",width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIDDEN,&w,&r))throw std::runtime_error(SDL_GetError());
    SDL_ShowWindow(w);
    if(fullscreen){if(!SDL_SetWindowFullscreen(w,true))throw std::runtime_error(SDL_GetError());SDL_SyncWindow(w);}
    SDL_SetRenderLogicalPresentation(r,width,height,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if(vsync!="on"&&vsync!="off"&&vsync!="adaptive")throw std::runtime_error("Invalid V-sync mode");
    int syncValue=vsync=="off"?0:vsync=="adaptive"?-1:1;
    bool synced=SDL_SetRenderVSync(r,syncValue)&&syncValue!=0;
    if(!synced&&syncValue) synced=SDL_SetRenderVSync(r,1);
    auto*target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height);
    int mw=height*176/240,mh=height*40/240;auto*mirrorTarget=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,mw,mh);
    SDL_Texture*screen=nullptr;unsigned sw=0,sh=0;
    if(!target||!mirrorTarget)throw std::runtime_error(SDL_GetError());
    std::unique_ptr<CourseMesh>mesh;HudRenderer hud,sky;Bridge bridge;FrameMetrics metrics(metricsPath);FrameGraph graph;
    if(!frozenSnapshot.empty()){std::ifstream in(frozenSnapshot,std::ios::binary);in.exceptions(std::ios::badbit|std::ios::failbit);in.read((char*)bridge.snapshot.get(),sizeof(RRVSnapshot));if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Snapshot size/version mismatch");const auto&s=*bridge.snapshot;if(s.model_count>RRV_MODEL_CAP||s.hud_count>RRV_HUD_CAP||s.sky_count>RRV_SKY_CAP||s.screen_width>640||s.screen_height>512||(s.valid&&s.course>3))throw std::runtime_error("Invalid snapshot counts");}
    Frame current{},currentMirror{};PresentationTimeline timeline,mirrorTimeline;uint32_t track=~0u;
    auto start=SDL_GetTicksNS(),last=start;double next=0;unsigned count=0;uint64_t keyboardUntil=0;bool running=true,mark=false,raised=false;std::string notice;uint64_t noticeUntil=0;
    std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
    const auto pacingDisplay=SDL_GetDisplayForWindow(w);const SDL_DisplayMode*mode=SDL_GetCurrentDisplayMode(pacingDisplay);double displayHz=mode?mode->refresh_rate:0;
    double requestedFps=fps;if(fps==0)fps=displayHz>0?displayHz:60;
    double effective=synced&&displayHz>0?std::min(fps,displayHz):fps;
    bool displayPaced=synced&&displayHz>0&&fps>=displayHz-.01;
    DisplayPacer displayPacer;GpuTimer renderTimer;bool earlyPresent=false;
    const char*override=SDL_getenv("RIDGE_MAC_EARLY_PRESENT");
    if(fullscreen&&syncValue==1&&displayPaced&&(!override||std::strcmp(override,"0")!=0)){
        if(displayPacer.open())earlyPresent=SDL_SetRenderVSync(r,0);
        if(!earlyPresent){std::cerr<<"Display-linked pacing unavailable: "<<SDL_GetError()<<"\n";displayPacer.close();}
    }
    std::cerr<<"Revolution presentation: "<<(earlyPresent?"display-link before render":"SDL swap")<<" at "<<effective<<" Hz\n";
    if(!metricsPath.empty()){
        int pw=0,ph=0;SDL_GetWindowSizeInPixels(w,&pw,&ph);
        std::ofstream out(metricsPath+".presentation.json");
        out<<"{\"requested_fps\":"<<requestedFps<<",\"effective_fps\":"<<effective<<",\"display_hz\":"<<displayHz<<",\"vsync\":"<<(synced?"true":"false")
           <<",\"presentation_strategy\":"<<std::quoted(earlyPresent?"mac-display-link-before-render":"sdl-swap")
           <<",\"camera_trace_version\":1,\"motion_sampling\":\"before-scene-processing\""
           <<",\"render_width\":"<<width<<",\"render_height\":"<<height<<",\"window_pixel_width\":"<<pw<<",\"window_pixel_height\":"<<ph
           <<",\"sdl_version\":"<<SDL_GetVersion()<<",\"gpu_renderer\":"<<std::quoted(reinterpret_cast<const char*>(glGetString(GL_RENDERER)))<<"}";
    }
    uint32_t previousSequence=0;uint64_t previousCycles=0,previousPublication=0,sequenceGaps=0;
    uint64_t lastPresentEnd=start;double previousRecord=0,waitMs=0,sleepMs=0,eventMs=0;unsigned waitTimeouts=0;
    while(running&&!stopped){
        if(earlyPresent){auto waitStart=SDL_GetTicksNS();if(!displayPacer.wait()){++waitTimeouts;earlyPresent=false;displayPacer.close();if(!SDL_SetRenderVSync(r,syncValue))throw std::runtime_error(SDL_GetError());std::cerr<<"Display callback timed out; restored SDL V-sync\n";}waitMs+=double(SDL_GetTicksNS()-waitStart)/1e6;}
        auto eventStart=SDL_GetTicksNS();
        auto now=SDL_GetTicksNS();double elapsed=double(now-start)/1e9;if(seconds&&elapsed>=seconds)break;
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(earlyPresent&&e.type==SDL_EVENT_WINDOW_DISPLAY_CHANGED&&SDL_GetDisplayForWindow(w)!=pacingDisplay){earlyPresent=false;displayPacer.close();SDL_SetRenderVSync(r,syncValue);}
if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat&&e.key.key==SDLK_G)showGraph=!showGraph;if(e.type==SDL_EVENT_QUIT||(e.type==SDL_EVENT_KEY_DOWN&&e.key.key==SDLK_ESCAPE))running=false;if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat&&(e.key.key==SDLK_P||e.key.key==SDLK_F8))mark=true;}
        eventMs+=double(SDL_GetTicksNS()-eventStart)/1e6;
        if(!running)break;
        now=SDL_GetTicksNS();elapsed=double(now-start)/1e9;
        if(!displayPaced&&elapsed<next){auto sleepStart=SDL_GetTicksNS();SDL_DelayPrecise(uint64_t((next-elapsed)*1e9));sleepMs+=double(SDL_GetTicksNS()-sleepStart)/1e6;continue;}next=(std::floor(elapsed*effective)+1)/effective;
        // Latch motion time before variable snapshot decoding/VRAM work.
        // Every displayed frame samples the same point in its pacing cycle.
        const double presentationWall=monotonicSeconds();
        auto inputStart=SDL_GetTicksNS();
        uint32_t buttons=0xffff;const bool*keys=SDL_GetKeyboardState(nullptr);
        for(auto binding:{std::pair{SDL_SCANCODE_RETURN,8},std::pair{SDL_SCANCODE_UP,16},std::pair{SDL_SCANCODE_RIGHT,32},std::pair{SDL_SCANCODE_DOWN,64},std::pair{SDL_SCANCODE_LEFT,128},std::pair{SDL_SCANCODE_X,0x4000},std::pair{SDL_SCANCODE_SPACE,0x4000},std::pair{SDL_SCANCODE_Z,0x8000},std::pair{SDL_SCANCODE_A,0x1000},std::pair{SDL_SCANCODE_S,0x2000},std::pair{SDL_SCANCODE_Q,0x400},std::pair{SDL_SCANCODE_E,0x800}})if(keys[binding.first])buttons&=~binding.second;
        if(buttons!=0xffff)keyboardUntil=now+100000000;
        auto readStart=SDL_GetTicksNS();bool changed=frozenSnapshot.empty()?bridge.poll(path,buttons,(SDL_GetWindowFlags(w)&SDL_WINDOW_INPUT_FOCUS)&&now<keyboardUntil):count==0;auto&s=*bridge.snapshot;
        double sourceAgeStart=bridge.sourceAgeSeconds()*1000;
        double sourceStepMs=-1,publicationStepMs=-1;
        if(changed&&frozenSnapshot.empty()){
            uint32_t delta=s.sequence-previousSequence;
            if(previousPublication&&delta>0&&delta<0x80000000u&&bridge.published>previousPublication&&s.cycles>=previousCycles){
                sequenceGaps+=delta-1;
                sourceStepMs=double(s.cycles-previousCycles)/33868.8;
                publicationStepMs=double(bridge.published-previousPublication)/1e6;
            }
            previousSequence=s.sequence;previousCycles=s.cycles;previousPublication=bridge.published;
        }
        bool ready=s.valid&&(!frozenSnapshot.empty()||bridge.sourceAgeSeconds()<.5);
        bool measureRender=!metricsPath.empty();
        if(measureRender)renderTimer.begin(count);
        if(changed){
            ready=s.valid;
            if(s.valid){
                if(track!=s.course){if(mesh)mesh->close();mesh=std::make_unique<CourseMesh>();mesh->fullCourse=s.full_scene;mesh->perspective=perspective;mesh->smooth=smooth;mesh->load(r,assets+"/course-"+std::to_string(s.course)+".rrassets");hud.close();sky.close();timeline.clear();mirrorTimeline.clear();track=s.course;}
                current=decode(s,false);currentMirror=decode(s,true);
                for(const auto&pose:current.models){auto found=std::find_if(currentMirror.models.begin(),currentMirror.models.end(),[&](const ModelPose&m){return m.key==pose.key&&m.model==pose.model;});if(found==currentMirror.models.end())currentMirror.models.push_back(pose);}
                timeline.push(current,bridge.published/1e9);mirrorTimeline.push(currentMirror,bridge.published/1e9);
                std::vector<uint16_t>vram(s.vram,s.vram+524288);hud.updateVram(mesh->sky.vram,vram);sky.updateVram(mesh->sky.vram,vram);mesh->updateVram(vram);
            }else{
                timeline.clear();mirrorTimeline.clear();
                if(s.screen_width&&s.screen_height){
                    if(sw!=s.screen_width||sh!=s.screen_height){if(screen)SDL_DestroyTexture(screen);sw=s.screen_width;sh=s.screen_height;screen=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,sw,sh);}
                    if(screen)SDL_UpdateTexture(screen,nullptr,s.screen,sw*4);
                }
            }
        }
        if(!raised&&(ready||screen)){SDL_ShowWindow(w);SDL_RaiseWindow(w);raised=true;}
        if(mesh){mesh->depthRenderer.measureGpu=false;mesh->depthRenderer.frameId=count;}
        double sourceAge=bridge.sourceAgeSeconds();auto drawStart=SDL_GetTicksNS();Frame f=current,m=currentMirror;PresentationSampleInfo sampleInfo;
        if(frozenSnapshot.empty()&&bridge.published){f=timeline.at(presentationWall,1/effective,&sampleInfo);m=mirrorTimeline.at(presentationWall,1/effective);}
        SDL_SetRenderTarget(r,target);SDL_SetRenderDrawColor(r,0,16,128,255);SDL_RenderClear(r);
        if(ready&&mesh){
            sky.drawBackground(r,f,mesh->sky.vram,width,height);
            mesh->focalScale=320.f/240;mesh->draw(r,f,f.camera,width,height);
            // Composite the mirror before HUD lettering and its border.
            if(s.mirror_enabled&&s.state==17){
                SDL_SetRenderTarget(r,mirrorTarget);SDL_SetRenderDrawColor(r,32,96,180,255);SDL_RenderClear(r);
                mesh->focalScale=8;mesh->draw(r,m,m.camera,mw,mh);mesh->focalScale=320.f/240;
                SDL_SetRenderTarget(r,target);float scale=height/240.f;SDL_FRect area{(width-176*scale)/2,16*scale,176*scale,40*scale};SDL_RenderTextureRotated(r,mirrorTarget,nullptr,&area,0,nullptr,SDL_FLIP_HORIZONTAL);
            }
            hud.draw(r,f,mesh->sky.vram,width,height,f.hudDisplayX,f.hudDisplayY,false,s.mirror_enabled&&s.state==17);
        }else if(screen){SDL_FRect area{(width-height*4.f/3)/2,0,height*4.f/3,float(height)};SDL_RenderTexture(r,screen,nullptr,&area);}else{SDL_SetRenderDrawColor(r,255,255,255,255);SDL_RenderDebugText(r,20,30,"Starting Revolution...");}
        if(count)graph.add(elapsed,double(now-last)/1e6);
        if(showGraph)graph.draw(r,width,height,elapsed,effective);
        unsigned marker=0;double markerMs=0;
        if(mark||(!shot.empty()&&ready&&elapsed>=shotAt)){
            auto begin=SDL_GetTicksNS();std::string dest=shot;
            if(mark){marker=++metrics.marker;dest=metricsPath.empty()?path+".marker-"+std::to_string(marker)+".bmp":metricsPath+".marker-"+std::to_string(marker)+".bmp";}
            auto*surface=SDL_RenderReadPixels(r,nullptr);bool saved=surface&&SDL_SaveBMP(surface,dest.c_str());if(surface)SDL_DestroySurface(surface);
            notice=saved?"Capture saved"+(marker?" #"+std::to_string(marker):""):"Capture failed - see launcher log";noticeUntil=SDL_GetTicksNS()+4000000000;
            if(!saved)std::cerr<<"Capture failed: "<<SDL_GetError()<<"\n";
            if(!metricsPath.empty()){std::ofstream out(dest+".snapshot",std::ios::binary);out.write((const char*)&s,sizeof s);out.close();if(!out){notice="Snapshot save failed - see launcher log";std::cerr<<"Snapshot write failed: "<<dest<<"\n";}}
            if(saved){
                std::ofstream meta(dest+".json");meta<<"{\"snapshot_magic\":"<<RRV_SHARED_MAGIC<<",\"snapshot_bytes\":"<<sizeof s<<",\"sequence\":"<<s.sequence<<",\"course\":"<<s.course<<",\"presentation_fps\":"<<effective<<",\"scene_time\":"<<f.time<<",\"width\":"<<width<<",\"height\":"<<height<<",\"full_scene\":"<<(s.full_scene?"true":"false")<<"}\n";
                std::cerr<<"Revolution capture saved: "<<dest<<"\n";
            }
            mark=false;shot.clear();markerMs=double(SDL_GetTicksNS()-begin)/1e6;
        }
        if(SDL_GetTicksNS()<noticeUntil){float oldX,oldY;SDL_GetRenderScale(r,&oldX,&oldY);float noticeScale=std::max(1.f,height/720.f);SDL_SetRenderScale(r,noticeScale,noticeScale);SDL_SetRenderDrawColor(r,0,0,0,220);SDL_FRect box{12,12,300,28};SDL_RenderFillRect(r,&box);SDL_SetRenderDrawColor(r,255,255,255,255);SDL_RenderDebugText(r,20,20,notice.c_str());SDL_SetRenderScale(r,oldX,oldY);}
        SDL_SetRenderTarget(r,nullptr);SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);SDL_RenderTexture(r,target,nullptr,nullptr);auto present=SDL_GetTicksNS();
        if(!SDL_FlushRenderer(r))throw std::runtime_error(SDL_GetError());if(measureRender)renderTimer.end();
        auto swap=SDL_GetTicksNS();SDL_RenderPresent(r);auto end=SDL_GetTicksNS();
        FrameDetail detail;
        std::copy_n(s.camera,3,detail.sourceCamera.begin());std::copy_n(s.matrix,9,detail.sourceMatrix.begin());
        detail.sourceRotation={current.rotation.x,current.rotation.y,current.rotation.z,current.rotation.w};
        detail.renderRotation={f.rotation.x,f.rotation.y,f.rotation.z,f.rotation.w};
        detail.sourceCycles=s.cycles;detail.sourcePublished=double(bridge.published)/1e9;detail.sampleWall=presentationWall;
        // SDL timestamps share the CSV wall_s epoch, not CLOCK_MONOTONIC's epoch.
        detail.swapStartWall=double(swap-start)/1e9;detail.swapEndWall=double(end-start)/1e9;
        detail.bracketStart=sampleInfo.bracketStart;detail.bracketEnd=sampleInfo.bracketEnd;
        detail.held=sampleInfo.held;detail.sequenceGaps=sequenceGaps;detail.sourceStep=sourceStepMs;detail.publicationStep=publicationStepMs;detail.sourceChanged=changed;detail.sourceGame=double(s.cycles)/33868800.;detail.interpolationAlpha=sampleInfo.alpha;detail.interpolationTarget=sampleInfo.target;detail.sourceAgeStart=sourceAgeStart;detail.gap=count?double(now-lastPresentEnd)/1e6:0;detail.events=eventMs;detail.input=double(readStart-inputStart)/1e6;detail.sleep=sleepMs;detail.displayWait=waitMs;detail.displayWaitTimeouts=waitTimeouts;detail.previousRecord=previousRecord;detail.renderFlush=double(swap-present)/1e6;detail.swap=double(end-swap)/1e6;detail.gpuRender=renderTimer.ms;detail.gpuRenderFrame=renderTimer.frame;detail.phase=s.state;detail.graph=showGraph;detail.focused=(SDL_GetWindowFlags(w)&SDL_WINDOW_INPUT_FOCUS)!=0;
        auto recordStart=SDL_GetTicksNS();
        metrics.record(count,elapsed,count?double(now-last)/1e6:0,count?std::max(0.,double(now-last)/1e6-1000/effective):0,double(drawStart-readStart)/1e6,0,0,double(present-drawStart)/1e6-markerMs,double(end-present)/1e6,ready,s.sequence,sourceAge*1000,f.models.size(),mesh?mesh->textureUpdates:0,f.camera.x,f.camera.y,f.camera.z,f.time,marker,markerMs,detail);
        previousRecord=double(SDL_GetTicksNS()-recordStart)/1e6;lastPresentEnd=end;eventMs=waitMs=sleepMs=0;last=now;count++;
    }
    renderTimer.close();displayPacer.close();bridge.poll(path,0xffff,false);if(mesh)mesh->close();hud.close();sky.close();if(screen)SDL_DestroyTexture(screen);SDL_DestroyTexture(mirrorTarget);SDL_DestroyTexture(target);SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();return 0;
}catch(const std::exception&e){std::cerr<<"Revolution native renderer: "<<e.what()<<"\n";return 1;}
