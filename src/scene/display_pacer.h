#pragma once
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <CoreVideo/CVDisplayLink.h>
// Schedule work at the display callback, before drawing. SDL's Cocoa GL swap
// path instead waits immediately before submission, leaving little headroom.
// Resolve CoreVideo dynamically so other platforms/build dependencies stay put.
struct DisplayPacer {
    SDL_SharedObject*library=nullptr;CVDisplayLinkRef link=nullptr;
    SDL_Mutex*mutex=nullptr;SDL_Condition*condition=nullptr;
    uint64_t produced=0,consumed=0;
    using Create=CVReturn(*)(CVDisplayLinkRef*);
    using Bind=CVReturn(*)(CVDisplayLinkRef,CGLContextObj,CGLPixelFormatObj);
    using Callback=CVReturn(*)(CVDisplayLinkRef,CVDisplayLinkOutputCallback,void*);
    using Start=CVReturn(*)(CVDisplayLinkRef);using Stop=CVReturn(*)(CVDisplayLinkRef);using Release=void(*)(CVDisplayLinkRef);
    Bind bind=nullptr;Stop stop=nullptr;Release release=nullptr;
    static CVReturn tick(CVDisplayLinkRef,const CVTimeStamp*,const CVTimeStamp*,CVOptionFlags,CVOptionFlags*,void*context){
        auto*self=static_cast<DisplayPacer*>(context);
        SDL_LockMutex(self->mutex);++self->produced;SDL_SignalCondition(self->condition);SDL_UnlockMutex(self->mutex);
        return kCVReturnSuccess;
    }
    template<class T>T symbol(const char*name){return reinterpret_cast<T>(SDL_LoadFunction(library,name));}
    bool open(){
        library=SDL_LoadObject("/System/Library/Frameworks/CoreVideo.framework/CoreVideo");if(!library)return false;
        auto create=symbol<Create>("CVDisplayLinkCreateWithActiveCGDisplays");bind=symbol<Bind>("CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext");
        auto callback=symbol<Callback>("CVDisplayLinkSetOutputCallback");auto start=symbol<Start>("CVDisplayLinkStart");
        stop=symbol<Stop>("CVDisplayLinkStop");release=symbol<Release>("CVDisplayLinkRelease");
        if(!create||!bind||!callback||!start||!stop||!release)return false;
        mutex=SDL_CreateMutex();condition=SDL_CreateCondition();
        if(!mutex||!condition)return false;
        CVReturn status=create(&link);if(status!=kCVReturnSuccess){SDL_SetError("Display-link create failed (%d)",status);return false;}
        auto context=CGLGetCurrentContext();
        if(!context)return SDL_SetError("No current OpenGL context");
        status=bind(link,context,CGLGetPixelFormat(context));if(status!=kCVReturnSuccess)return SDL_SetError("Display-link bind failed (%d)",status);
        status=callback(link,tick,this);if(status!=kCVReturnSuccess)return SDL_SetError("Display-link callback failed (%d)",status);
        status=start(link);if(status!=kCVReturnSuccess)return SDL_SetError("Display-link start failed (%d)",status);
        return true;
    }
    bool wait(){
        SDL_LockMutex(mutex);
        if(produced==consumed)SDL_WaitConditionTimeout(condition,mutex,100);
        bool arrived=produced!=consumed;consumed=produced;SDL_UnlockMutex(mutex);return arrived;
    }
    void close(){
        if(link){stop(link);release(link);link=nullptr;}
        if(condition){SDL_DestroyCondition(condition);condition=nullptr;}
        if(mutex){SDL_DestroyMutex(mutex);mutex=nullptr;}
        if(library){SDL_UnloadObject(library);library=nullptr;}
    }
    ~DisplayPacer(){close();}
};
#endif
