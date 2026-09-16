#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
// Split bounded DMA packets into the GP0 drawing commands they contain.
inline std::vector<uint32_t> splitDrawingCommands(const std::vector<uint32_t>&packets){
        std::vector<uint32_t> commands;
        for(size_t at=0;at<packets.size();){
            unsigned size=packets[at++];if(size>packets.size()-at)break;
            size_t end=at+size;
            while(at<end){
                unsigned cmd=packets[at]>>24,len=1;
                if(cmd>=0x20&&cmd<0x40){unsigned vertices=(cmd&8)?4:3;len=1+vertices*(1+bool(cmd&4)+bool(cmd&16))-bool(cmd&16);}
                else if(cmd>=0x40&&cmd<0x60){
                    len=(cmd&16)?4:3;
                    if(cmd&8){len=1;while(at+len<end&&(packets[at+len]&0xf000f000)!=0x50005000)len++;if(at+len<end)len++;}
                }else if(cmd>=0x60&&cmd<0x80)len=2+bool(cmd&4)+((cmd&0x18)==0);
                else if(cmd==2)len=3;
                if(len>end-at)break;
                commands.push_back(len);commands.insert(commands.end(),packets.begin()+at,packets.begin()+at+len);at+=len;
            }
            at=end;
        }
    return commands;
}
