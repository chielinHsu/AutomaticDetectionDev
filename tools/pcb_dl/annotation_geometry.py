"""Annotation intent is distinct from the polygon coordinates used for dataset export."""
import math
import numpy as np


def is_rectangle(points):
    p=np.asarray(points,dtype=float)
    if p.shape!=(4,2) or not np.isfinite(p).all():return False
    edges=np.roll(p,-1,axis=0)-p;lengths=np.linalg.norm(edges,axis=1)
    if min(lengths)<=1e-6:return False
    right=all(abs(float(np.dot(edges[i],edges[(i+1)%4])))<=1e-4*lengths[i]*lengths[(i+1)%4] for i in range(4))
    return right and bool(np.allclose(edges[0],-edges[2],rtol=1e-4,atol=1e-4) and np.allclose(edges[1],-edges[3],rtol=1e-4,atol=1e-4))


def annotation_shape(label):
    shape=label.get('shape')
    if shape is None:return 'rectangle' if is_rectangle(label['points']) else 'polygon'
    if shape not in ('rectangle','polygon'):raise ValueError('不支援的標註形狀類型')
    if shape=='rectangle' and not is_rectangle(label['points']):raise ValueError('矩形標註必須保持四個直角')
    return shape


def resized_vertex(label,index,x,y):
    points=np.asarray(label['points'],dtype=float).copy()
    if not 0<=index<len(points) or not math.isfinite(x) or not math.isfinite(y):raise ValueError('無效的頂點座標')
    if annotation_shape(label)=='polygon':
        points[index]=[x,y];return points.tolist()
    following=(index+1)%4;previous=(index-1)%4;opposite=(index+2)%4
    u=points[following]-points[index];u/=np.linalg.norm(u)
    v=points[previous]-points[index];v/=np.linalg.norm(v)
    anchor=points[opposite].copy();delta=np.asarray([x,y])-anchor
    a=float(np.dot(delta,u));b=float(np.dot(delta,v))
    # Crossing the fixed corner flips the rectangle; a short side never degenerates to zero.
    a=math.copysign(max(abs(a),2.0),a if a else -1)
    b=math.copysign(max(abs(b),2.0),b if b else -1)
    points[index]=anchor+a*u+b*v;points[following]=anchor+b*v;points[previous]=anchor+a*u
    return points.tolist()
