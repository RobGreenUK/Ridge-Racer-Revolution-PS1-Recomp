#pragma once
// Optional elapsed-time queries. Never wait for a GPU result; skip collection
// when the ring is full. Frame IDs identify the older submission being measured.
#ifdef _WIN32
#define RR_GPU_CALL APIENTRY
#else
#define RR_GPU_CALL
#endif
struct GpuTimer {
    using Gen=void(RR_GPU_CALL*)(GLsizei,GLuint*);using Del=void(RR_GPU_CALL*)(GLsizei,const GLuint*);
    using Begin=void(RR_GPU_CALL*)(GLenum,GLuint);using End=void(RR_GPU_CALL*)(GLenum);
    using Available=void(RR_GPU_CALL*)(GLuint,GLenum,GLint*);using Result=void(RR_GPU_CALL*)(GLuint,GLenum,uint64_t*);
    Gen gen=nullptr;Del del=nullptr;Begin beginQuery=nullptr;End endQuery=nullptr;Available available=nullptr;Result result=nullptr;
    struct Slot {GLuint query=0;long long frame=-1;bool pending=false;};std::array<Slot,8>slots{};
    bool initialized=false;int active=-1;double ms=-1;long long frame=-1;
    void init(){
        if(initialized)return;initialized=true;
        if(!SDL_GL_ExtensionSupported("GL_ARB_timer_query")&&!SDL_GL_ExtensionSupported("GL_EXT_timer_query"))return;
        gen=reinterpret_cast<Gen>(SDL_GL_GetProcAddress("glGenQueries"));del=reinterpret_cast<Del>(SDL_GL_GetProcAddress("glDeleteQueries"));
        beginQuery=reinterpret_cast<Begin>(SDL_GL_GetProcAddress("glBeginQuery"));endQuery=reinterpret_cast<End>(SDL_GL_GetProcAddress("glEndQuery"));
        available=reinterpret_cast<Available>(SDL_GL_GetProcAddress("glGetQueryObjectiv"));result=reinterpret_cast<Result>(SDL_GL_GetProcAddress("glGetQueryObjectui64v"));
        if(!result)result=reinterpret_cast<Result>(SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT"));
        if(!gen||!del||!beginQuery||!endQuery||!available||!result){beginQuery=nullptr;return;}
        for(auto&s:slots)gen(1,&s.query);
    }
    void begin(long long current){
        init();active=-1;ms=-1;frame=-1;if(!beginQuery)return;
        for(auto&s:slots)if(s.pending){GLint ready=0;available(s.query,0x8867,&ready);if(ready){uint64_t ns=0;result(s.query,0x8866,&ns);if(s.frame>frame){ms=double(ns)/1e6;frame=s.frame;}s.pending=false;}}
        for(size_t i=0;i<slots.size();i++)if(!slots[i].pending){active=int(i);slots[i].frame=current;beginQuery(0x88bf,slots[i].query);return;}
    }
    void end(){if(active>=0){endQuery(0x88bf);slots[active].pending=true;active=-1;}}
    void close(){if(del)for(auto&s:slots)if(s.query){del(1,&s.query);s.query=0;}}
};
