"""Trim background terrain where the authored road passes through it.

The original ordering table hides these intersections. A per-pixel depth buffer
otherwise exposes their upper edges. Only positive-bias background faces are
trimmed, inside the road footprint and above its plane (including the renderer's road depth tolerance); collision is untouched.
"""
import math

def clip(poly, plane):
    inside=[];outside=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        da=sum(a[i]*plane[i] for i in range(3))+plane[3]
        db=sum(b[i]*plane[i] for i in range(3))+plane[3]
        (inside if da>=0 else outside).append(a)
        if (da>=0)!=(db>=0):
            t=da/(da-db);v=tuple(a[i]+t*(b[i]-a[i]) for i in range(5))
            inside.append(v);outside.append(v)
    return inside,outside

def road_prism(v):
    a,b,c=v
    cross=(b[0]-a[0])*(c[2]-a[2])-(b[2]-a[2])*(c[0]-a[0])
    if abs(cross)<1e-5:return None
    planes=[];sign=1 if cross>0 else -1
    for p,q in zip(v,v[1:]+v[:1]):
        dx=q[0]-p[0];dz=q[2]-p[2]
        planes.append((-dz*sign,0,dx*sign,(dz*p[0]-dx*p[2])*sign))
    # Solve road y = sx*x + sz*z + intercept; positive side is above road.
    sx=((b[1]-a[1])*(c[2]-a[2])-(c[1]-a[1])*(b[2]-a[2]))/cross
    sz=((b[0]-a[0])*(c[1]-a[1])-(c[0]-a[0])*(b[1]-a[1]))/cross
    planes.append((sx,-1,sz,a[1]-sx*a[0]-sz*a[2]+10.0))
    return planes

def repair_terrain(faces):
    grid={};roads=[]
    def cells(v):
        for x in range(math.floor(min(p[0] for p in v)/512),math.floor(max(p[0] for p in v)/512)+1):
            for z in range(math.floor(min(p[2] for p in v)/512),math.floor(max(p[2] for p in v)/512)+1):yield x,z
    for key,kind,bias,window,v in faces:
        if kind!=1:continue
        for ids in ((0,1,2),(2,1,3)):
            tri=[v[i] for i in ids];prism=road_prism(tri)
            if prism is None:continue
            index=len(roads);roads.append((prism,tri))
            for cell in cells(tri):grid.setdefault(cell,[]).append(index)
    result=[]
    for key,kind,bias,window,v in faces:
        if kind!=0 or bias<=0:result.append((key,kind,bias,window,v));continue
        candidates=sorted({i for cell in cells(v) for i in grid.get(cell,[])})
        polys=[[v[i] for i in ids] for ids in ((0,1,2),(2,1,3))];changed=False
        for index in candidates:
            planes,tri=roads[index]
            # Repair intersections, not elevated surfaces such as tunnel roofs.
            plane=planes[-1]
            distances=[sum(p[i]*plane[i] for i in range(3))+plane[3] for p in v]
            if min(distances)>0 or max(distances)<=0:continue
            kept=[]
            for poly in polys:
                remaining=poly;outside=[]
                for plane in planes:
                    remaining,part=clip(remaining,plane)
                    if len(part)>=3:outside.append(part)
                    if len(remaining)<3:break
                if len(remaining)>=3:changed=True;kept.extend(outside)
                else:kept.append(poly) # No removed area: retain original topology.
            polys=kept
        if not changed:result.append((key,kind,bias,window,v));continue
        for poly in polys:
            for i in range(1,len(poly)-1):result.append((key,kind,bias,window,[poly[0],poly[i],poly[i+1],poly[i+1]]))
    return result

def repair_panel_overlap(face,panel):
    """Remove a terrain protrusion only within a known foreground panel's outline."""
    vertices=face[-1];polys=[[vertices[i] for i in ids] for ids in ((0,1,2),(2,1,3))]
    def subtract(a,b):return tuple(a[i]-b[i] for i in range(3))
    def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
    def dot(a,b):return sum(a[i]*b[i] for i in range(3))
    changed=False
    for ids in ((0,1,2),(2,1,3)):
        tri=[panel[-1][i] for i in ids];a,b,c=tri
        normal=cross(subtract(b,a),subtract(c,a));length=math.sqrt(dot(normal,normal))
        if length<1e-8:continue
        normal=tuple(x/length for x in normal);planes=[]
        for p,q in zip(tri,tri[1:]+tri[:1]):
            inward=cross(normal,subtract(q,p));planes.append((*inward,-dot(inward,p)))
        # PS1 positive-area winding faces away from the viewer. Clear half a
        # unit behind the panel too, avoiding residual depth precision ties.
        planes.append((*(-x for x in normal),dot(normal,a)+.5))
        kept=[]
        for poly in polys:
            remaining=poly;outside=[]
            for plane in planes:
                remaining,part=clip(remaining,plane)
                if len(part)>=3:outside.append(part)
                if len(remaining)<3:break
            if len(remaining)>=3:changed=True;kept.extend(outside)
            else:kept.append(poly)
        polys=kept
    if not changed:return [face]
    return [(*face[:-1],[poly[0],poly[i],poly[i+1],poly[i+1]]) for poly in polys for i in range(1,len(poly)-1)]
