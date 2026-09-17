#pragma once
#include <map>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#endif
#if defined(__APPLE__)
#include "gpu_timer.h"
#endif
// Draw into SDL's active texture target, saving the GL state SDL caches.
// A real depth buffer keeps overlapping geometry ordered per pixel. The game's
// signed ordering bias separates authored layers, with polygon offset breaking
// residual depth-buffer precision ties.
struct DepthRenderer {
    bool measureGpu=true;long long frameId=0;double gpuMs=-1;long long gpuFrame=-1;
#if defined(__APPLE__)
    GpuTimer timer;
    struct PackedVertex {float position[4],uv[4];};
    std::vector<PackedVertex> vertices;GLuint vertexBuffer=0;
    std::map<std::pair<int,int>,GLuint> buffers;
    void close(){timer.close();if(vertexBuffer)glDeleteBuffers(1,&vertexBuffer);vertexBuffer=0;for(auto&entry:buffers)glDeleteRenderbuffersEXT(1,&entry.second);buffers.clear();}
    template<class Faces>
    bool draw(SDL_Renderer*renderer,const std::vector<SDL_Texture*>&textures,const Faces&faces,int width,int height,bool perspective=true,float focalScale=320.f/240,bool smooth=false,float offsetX=0,float offsetY=0){
        if(std::strcmp(SDL_GetRendererName(renderer),"opengl")||!SDL_GetRenderTarget(renderer))return false;
        if(!SDL_FlushRenderer(renderer))throw std::runtime_error(SDL_GetError());
        if(measureGpu){timer.begin(frameId);gpuMs=timer.ms;gpuFrame=timer.frame;}
        GLint oldBuffer;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);glClientActiveTexture(GL_TEXTURE0);
        GLint oldProgram,oldMode,oldActive,oldRenderbuffer,oldDepthType,oldDepthName;
        glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_MATRIX_MODE,&oldMode);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT,&oldRenderbuffer);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT,&oldDepthType);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT,&oldDepthName);
        auto&depth=buffers[{width,height}];bool fresh=!depth;
        if(fresh)glGenRenderbuffersEXT(1,&depth);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,depth);
        if(fresh)glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT24,width,height);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,depth);
        GLenum complete=glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        glPushAttrib(GL_ALL_ATTRIB_BITS);glUseProgram(0);glActiveTexture(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
        double near=20,far=150000,focal=height*focalScale;
        glFrustum((-width*.5-offsetX)*near/focal,(width*.5-offsetX)*near/focal,(-height*.5-offsetY)*near/focal,(height*.5-offsetY)*near/focal,near,far);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0);glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4f(1,1,1,1);
        glEnable(GL_POLYGON_OFFSET_FILL);
        vertices.clear();vertices.reserve(faces.size()*3);
        for(const auto&face:faces){
            // Give authored surface layers a small, camera-independent separation.
            // Packed depth-buffer units alone cannot cover the quarter-unit
            // differences between the original barrier and arrow meshes.
            const auto&a=face.vertices[0].position;const auto&b=face.vertices[1].position;const auto&c=face.vertices[2].position;
            Vec ab=b-a,ac=c-a,n{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
            float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z),plane=n.x*a.x+n.y*a.y+n.z*a.z;
            float scale=plane>0?1+face.layerOffset*length/plane:1;
            for(const auto&v:face.vertices){
                float biasedZ=std::max(20.001f,v.position.z*scale);
                // Multiplying S/T/Q by clip W cancels perspective interpolation
                // for affine mode; it does not change geometry or depth testing.
                PackedVertex packed{{v.position.x,v.position.y,-v.position.z,v.position.z/biasedZ},{v.u,v.v,0,1}};
                if(!perspective){packed.uv[0]*=v.position.z;packed.uv[1]*=v.position.z;packed.uv[3]=v.position.z;}
                vertices.push_back(packed);
            }
        }
        if(!vertexBuffer)glGenBuffers(1,&vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer);
        // Orphan the previous store so queued GPU draws retain their data.
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(PackedVertex),nullptr,GL_STREAM_DRAW);
        if(!vertices.empty())glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(PackedVertex),vertices.data());
        glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_COLOR_ARRAY);
        glVertexPointer(4,GL_FLOAT,sizeof(PackedVertex),nullptr);
        glTexCoordPointer(4,GL_FLOAT,sizeof(PackedVertex),reinterpret_cast<void*>(sizeof(float)*4));
        if(complete==GL_FRAMEBUFFER_COMPLETE_EXT)for(size_t start=0;start<faces.size();){
            size_t end=start+1;while(end<faces.size()&&faces[end].texture==faces[start].texture&&faces[end].bias==faces[start].bias)end++;
            auto properties=SDL_GetTextureProperties(textures[faces[start].texture]);
            glBindTexture(GL_TEXTURE_2D,GLuint(SDL_GetNumberProperty(properties,SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER,0)));
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,smooth?GL_LINEAR:GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,smooth?GL_LINEAR:GL_NEAREST);
            glPolygonOffset(0,float(faces[start].bias));glDrawArrays(GL_TRIANGLES,GLint(start*3),GLsizei((end-start)*3));start=end;
        }
        glPopClientAttrib();glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);
        if(measureGpu)timer.end();
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_TEXTURE);glPopMatrix();
        glPopAttrib();glUseProgram(oldProgram);glActiveTexture(oldActive);glMatrixMode(oldMode);
        if(oldDepthType==GL_TEXTURE)glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,oldDepthName,0);
        else glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,oldDepthName);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,oldRenderbuffer);
        GLenum error=glGetError();
        if(complete!=GL_FRAMEBUFFER_COMPLETE_EXT||error!=GL_NO_ERROR)throw std::runtime_error("native depth pass failed: "+std::to_string(complete)+" / "+std::to_string(error));
        return true;
    }
#else
    void close(){}
    template<class Faces>bool draw(SDL_Renderer*,const std::vector<SDL_Texture*>&,const Faces&,int,int,bool=true){return false;}
#endif
};
