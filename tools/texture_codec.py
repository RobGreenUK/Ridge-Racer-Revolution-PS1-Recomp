"""Shared PS1 TMS/VRAM decoding from the final Ridge Racer renderer."""
import array
import struct

def decode_tms(data, vram):
    if len(data)<8 or struct.unpack_from('<I',data)[0]!=256:
        raise ValueError('Unsupported TMS header')
    offset, count = 4, 0
    while offset+4<=len(data):
        size=struct.unpack_from('<I',data,offset)[0];offset+=4
        if size==0:
            if offset!=len(data): raise ValueError('Data after TMS terminator')
            return count
        if size<20 or offset+size>len(data):raise ValueError('Truncated TMS image')
        image=data[offset:offset+size];offset+=size
        magic,flags=struct.unpack_from('<II',image)
        if magic!=16 or flags&~15:raise ValueError('Unsupported TIM header')
        cursor=8
        for block in range(2 if flags&8 else 1):
            if cursor+12>len(image):raise ValueError('Truncated TIM rectangle')
            claimed,x,y,w,h=struct.unpack_from('<I4H',image,cursor)
            end=cursor+12+w*h*2
            if x+w>1024 or y+h>512 or end>len(image):raise ValueError('Invalid TIM upload')
            # TMS's image block length can count twice the actual pixel bytes;
            # the enclosing TMS size and upload rectangle delimit stored data.
            if claimed not in (12+w*h*2,12+w*h*4):raise ValueError('Unknown TIM block length')
            pixels=struct.unpack_from('<'+str(w*h)+'H',image,cursor+12)
            for row in range(h):
                vram[(y+row)*1024+x:(y+row)*1024+x+w]=array.array('H',pixels[row*w:(row+1)*w])
            cursor=end
        if cursor!=len(image):raise ValueError('Unconsumed TIM bytes')
        count+=1
    raise ValueError('Missing TMS terminator')

def texture_rgba(vram,page,clut):
    if page==-1:
        return bytes([clut&255,(clut>>8)&255,(clut>>16)&255,255])*(256*256)
    mode=(page>>7)&3
    if mode==3:raise ValueError('Reserved texture depth')
    xbase=(page&15)*64;ybase=((page>>4)&1)*256
    cx=(clut&63)*16;cy=(clut>>6)&511
    out=bytearray()
    for y in range(256):
        for x in range(256):
            word=vram[((ybase+y)&511)*1024+((xbase+(x>>(2-mode)))&1023)]
            if mode==0:word=vram[cy*1024+((cx+((word>>((x&3)*4))&15))&1023)]
            elif mode==1:word=vram[cy*1024+((cx+((word>>((x&1)*8))&255))&1023)]
            channels=[word&31,(word>>5)&31,(word>>10)&31]
            out.extend([*((c<<3)|(c>>2) for c in channels),0 if word==0 else 255])
    return out
