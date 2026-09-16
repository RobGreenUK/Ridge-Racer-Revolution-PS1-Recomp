#pragma once
#include <cstdio>
#include <string>
#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <stdexcept>
// Additional fields use milliseconds unless explicitly identified otherwise.
struct FrameDetail {
    std::array<int32_t,3> sourceCamera{};
    std::array<int16_t,9> sourceMatrix{};
    std::array<float,4> sourceRotation{},renderRotation{};
    uint64_t sourceCycles=0;
    double sourcePublished=0,sampleWall=0,swapStartWall=0,swapEndWall=0,bracketStart=-1,bracketEnd=-1;

    double sourceStep=-1,publicationStep=-1,sourceGame=0,interpolationAlpha=-1,interpolationTarget=0;unsigned sourceChanged=0;
    double gap=0,events=0,input=0,sleep=0,previousRecord=0,sourceAgeStart=-1;
    double meshBuild=0,textureUpload=0,sort=0,depth=0;
    double renderFlush=0,swap=0,gpuRender=-1,displayWait=0;long long gpuRenderFrame=-1;unsigned displayWaitTimeouts=0;
    double gpuMs=-1;long long gpuFrame=-1;
    unsigned candidates=0,faces=0,chunksCulled=0,modelsCulled=0;
    unsigned phase=0,paused=0,focused=0,graph=0,held=0;uint64_t sequenceGaps=0,incompleteFrames=0;
};
// Single producer/single consumer ring: the renderer never formats text, locks,
// waits for space, or writes files. A stalled writer drops telemetry, not frames.
struct FrameMetrics {
    struct Row {
        unsigned frame,sequence,models,uploads,mark;bool ready;
        double wall,interval,late,scene,screen,vram,draw,present,age,game,markerMs;
        float x,y,z;FrameDetail detail;uint64_t dropped;
    };
    static constexpr size_t capacity=4096;
    FILE*file=nullptr;unsigned marker=0;
    std::unique_ptr<std::array<Row,capacity>>ring;
    std::atomic<size_t>head{0},tail{0};std::atomic<bool>finished{false};
    std::atomic<uint64_t>dropped{0};std::atomic<bool>writeFailed{false};std::thread writer;
    explicit FrameMetrics(const std::string&path){
        if(path.empty())return;
        file=std::fopen(path.c_str(),"w");if(!file)throw std::runtime_error("cannot open frame metrics");
        try{ring=std::make_unique<std::array<Row,capacity>>();writer=std::thread([this]{run();});}
        catch(...){std::fclose(file);file=nullptr;throw;}
    }
    FrameMetrics(const FrameMetrics&)=delete;FrameMetrics&operator=(const FrameMetrics&)=delete;
    void run(){
        std::setvbuf(file,nullptr,_IOFBF,262144);
        std::fputs("frame,wall_s,interval_ms,late_ms,scene_ms,screen_ms,vram_ms,draw_ms,present_ms,ready,sequence,source_age_ms,models,texture_updates,camera_x,camera_y,camera_z,game_s,marker,marker_ms,post_present_gap_ms,event_ms,input_ms,sleep_ms,previous_record_ms,source_age_start_ms,mesh_build_ms,texture_upload_ms,sort_ms,depth_ms,gpu_depth_ms,gpu_frame,candidate_triangles,faces,chunks_culled,models_culled,phase,paused,focused,graph,telemetry_dropped,sequence_gaps,incomplete_frames,interpolation_held,render_flush_ms,swap_ms,gpu_render_ms,gpu_render_frame,display_wait_ms,display_wait_timeouts,source_changed,source_step_ms,publication_step_ms,source_game_s,interpolation_alpha,interpolation_target_s,source_camera_x,source_camera_y,source_camera_z,source_matrix_0,source_matrix_1,source_matrix_2,source_matrix_3,source_matrix_4,source_matrix_5,source_matrix_6,source_matrix_7,source_matrix_8,source_qx,source_qy,source_qz,source_qw,render_qx,render_qy,render_qz,render_qw,source_cycles,source_published_monotonic_s,sample_monotonic_s,swap_start_s,swap_end_s,bracket_start_game_s,bracket_end_game_s\n",file);
        double flushed=0;
        for(;;){
            size_t t=tail.load(std::memory_order_relaxed);
            if(t==head.load(std::memory_order_acquire)){
                if(finished.load(std::memory_order_acquire)&&t==head.load(std::memory_order_acquire))break;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;
            }
            Row r=(*ring)[t];tail.store((t+1)%capacity,std::memory_order_release);
            const auto&d=r.detail;
            if(std::fprintf(file,"%u,%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%u,%.4f,%u,%u,%.2f,%.2f,%.2f,%.6f,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%lld,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%llu,%u,%.4f,%.4f,%.4f,%lld,%.4f,%u,%u,%.4f,%.4f,%.6f,%.6f,%.6f",
              r.frame,r.wall,r.interval,r.late,r.scene,r.screen,r.vram,r.draw,r.present,int(r.ready),r.sequence,r.age,r.models,r.uploads,r.x,r.y,r.z,r.game,r.mark,r.markerMs,
              d.gap,d.events,d.input,d.sleep,d.previousRecord,d.sourceAgeStart,d.meshBuild,d.textureUpload,d.sort,d.depth,d.gpuMs,d.gpuFrame,d.candidates,d.faces,d.chunksCulled,d.modelsCulled,d.phase,d.paused,d.focused,d.graph,(unsigned long long)r.dropped,(unsigned long long)d.sequenceGaps,(unsigned long long)d.incompleteFrames,d.held,d.renderFlush,d.swap,d.gpuRender,d.gpuRenderFrame,d.displayWait,d.displayWaitTimeouts,d.sourceChanged,d.sourceStep,d.publicationStep,d.sourceGame,d.interpolationAlpha,d.interpolationTarget)<0)writeFailed=true;
            for(auto v:d.sourceCamera)if(std::fprintf(file,",%d",int(v))<0)writeFailed=true;
            for(auto v:d.sourceMatrix)if(std::fprintf(file,",%d",int(v))<0)writeFailed=true;
            for(auto v:d.sourceRotation)if(std::fprintf(file,",%.9f",double(v))<0)writeFailed=true;
            for(auto v:d.renderRotation)if(std::fprintf(file,",%.9f",double(v))<0)writeFailed=true;
            if(std::fprintf(file,",%llu,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
                (unsigned long long)d.sourceCycles,d.sourcePublished,d.sampleWall,d.swapStartWall,d.swapEndWall,d.bracketStart,d.bracketEnd)<0)writeFailed=true;
            if(r.wall-flushed>=1){if(std::fflush(file))writeFailed=true;flushed=r.wall;}
        }
        if(std::fclose(file))writeFailed=true;
    }
    void record(unsigned frame,double wall,double interval,double late,double scene,double screen,double vram,double draw,double present,bool ready,unsigned sequence,double age,unsigned models,unsigned uploads,float x,float y,float z,double game,unsigned mark,double markerMs,const FrameDetail&detail={}){
        if(!file)return;
        size_t h=head.load(std::memory_order_relaxed),next=(h+1)%capacity;
        if(next==tail.load(std::memory_order_acquire)){dropped.fetch_add(1,std::memory_order_relaxed);return;}
        (*ring)[h]={frame,sequence,models,uploads,mark,ready,wall,interval,late,scene,screen,vram,draw,present,age,game,markerMs,x,y,z,detail,dropped.load(std::memory_order_relaxed)};
        head.store(next,std::memory_order_release);
    }
    ~FrameMetrics(){finished.store(true,std::memory_order_release);if(writer.joinable())writer.join();if(dropped.load()||writeFailed.load())std::fprintf(stderr,"frame metrics: dropped=%llu write_failed=%d\n",(unsigned long long)dropped.load(),int(writeFailed.load()));}
};
