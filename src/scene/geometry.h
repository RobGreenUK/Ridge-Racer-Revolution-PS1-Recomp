#pragma once
struct MeshVertex { Vec position; float u,v; };
// Bound the affine interpolation error of SDL geometry against perspective
// interpolation. Splits preserve world positions/UVs and projected coverage.
template<class Emit>
void perspectiveTriangles(MeshVertex a,MeshVertex b,MeshVertex c,const Emit&emit,int level=0) {
    MeshVertex v[3]={a,b,c};float error=0;int edge=0;
    for(int i=0;i<3;i++) {
        auto x=v[i],y=v[(i+1)%3];
        float e=256*std::max(std::abs(x.u-y.u),std::abs(x.v-y.v))*std::abs(x.position.z-y.position.z)/(2*(x.position.z+y.position.z));
        if(e>error){error=e;edge=i;}
    }
    if(error>0.75f&&level<14){
        auto x=v[edge],y=v[(edge+1)%3],z=v[(edge+2)%3];
        MeshVertex m{(x.position+y.position)*.5f,(x.u+y.u)*.5f,(x.v+y.v)*.5f};
        perspectiveTriangles(x,m,z,emit,level+1);perspectiveTriangles(m,y,z,emit,level+1);return;
    }
    emit(a,b,c);
}
