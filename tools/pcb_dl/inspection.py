"""Rigid component -> body -> pin/window frames, independent of model contour size."""
import copy
import math

KEYS=('x','y','w','h','angle')
MAX_BOXES=256


def validate_pose(pose):
    try:
        if any(not isinstance(pose[k],(int,float)) or isinstance(pose[k],bool) for k in KEYS):
            raise ValueError('檢測框座標須為數值')
        values=[float(pose[k]) for k in KEYS]
    except (KeyError,TypeError,ValueError) as exc:
        raise ValueError('檢測框幾何欄位無效') from exc
    if not all(math.isfinite(v) and abs(v)<=1000000 for v in values) or min(values[2:4])<=0:
        raise ValueError('檢測框須為有限座標與正尺寸（上限 1000000）')


def compose(parent,local):
    angle=math.radians(parent['angle']);c=math.cos(angle);s=math.sin(angle)
    return dict(x=parent['x']+c*local['x']+s*local['y'],
                y=parent['y']-s*local['x']+c*local['y'],w=local['w'],h=local['h'],
                angle=parent['angle']+local['angle'])


def relative(parent,world):
    angle=math.radians(parent['angle']);c=math.cos(angle);s=math.sin(angle)
    dx=world['x']-parent['x'];dy=world['y']-parent['y']
    return dict(x=c*dx-s*dy,y=s*dx+c*dy,w=world['w'],h=world['h'],angle=world['angle']-parent['angle'])


def frame_corners(pose):
    return [[p['x'],p['y']] for p in (compose(pose,dict(x=x,y=y,w=1,h=1,angle=0))
            for x,y in [(-pose['w']/2,-pose['h']/2),(pose['w']/2,-pose['h']/2),
                        (pose['w']/2,pose['h']/2),(-pose['w']/2,pose['h']/2)])]


def validate_inspection(group):
    if not isinstance(group,dict) or group.get('version')!=1:
        raise ValueError('不支援的檢測框群組版本')
    boxes=group.get('boxes')
    if not isinstance(boxes,list) or not 1<=len(boxes)<=MAX_BOXES:
        raise ValueError(f'檢測框群組須有 1–{MAX_BOXES} 個框')
    by_id={}
    for box in boxes:
        if not isinstance(box,dict):raise ValueError('檢測框格式無效')
        ident=box.get('id');parent=box.get('parent')
        if not isinstance(ident,str) or not ident or len(ident)>64 or ident=='component' or ident in by_id:
            raise ValueError('檢測框 ID 必須唯一且不得使用 component')
        if not isinstance(parent,str):raise ValueError('檢測框缺少父框')
        validate_pose(box.get('pose'));by_id[ident]=box
    if 'body' not in by_id or by_id['body'].get('kind')!='body' or by_id['body']['parent']!='component':
        raise ValueError('檢測框群組需要以 component 為父框的 body 本體')
    for box in boxes:
        kind=box.get('kind');parent=box['parent']
        if kind not in ('body','pin','window') or (kind=='body' and box['id']!='body'):
            raise ValueError('檢測框種類無效')
        if box['id']!='body' and (parent not in by_id or (kind=='pin' and parent!='body')):
            raise ValueError('導腳必須依附 body；檢測窗必須依附已有框')
        visited=set();current=box['id']
        while current!='component':
            if current in visited:raise ValueError('檢測框父子關係不可循環')
            if current not in by_id:raise ValueError('檢測框父框不存在')
            visited.add(current);current=by_id[current]['parent']


def inspection_boxes(component,profile):
    """Materialize world coordinates. Never trust a saved expanded-box cache."""
    group=profile.get('inspection')
    if group is None:return []
    validate_inspection(group)
    by_id={box['id']:box for box in group['boxes']};poses={'component':component}
    def world(ident):
        if ident not in poses:
            box=by_id[ident];poses[ident]=compose(world(box['parent']),box['pose'])
        return poses[ident]
    return [dict(id=box['id'],parent=box['parent'],kind=box['kind'],pose=world(box['id'])) for box in group['boxes']]


def fits_inspection(component,profile,roi):
    x,y,w,h=roi
    return all(x-1e-3<=px<=x+w+1e-3 and y-1e-3<=py<=y+h+1e-3
               for box in inspection_boxes(component,profile) for px,py in frame_corners(box['pose']))


def with_inspection(obj,profile):
    result=copy.deepcopy(obj);result.pop('inspection_boxes',None)
    boxes=inspection_boxes(obj['pose'],profile)
    if boxes:result['inspection_boxes']=boxes
    return result


def preset(w,h,pins=8):
    """Explicit starting geometry, never a claim of inferred package or polarity."""
    body=dict(id='body',parent='component',kind='body',pose=dict(x=0.,y=0.,w=w*.78,h=h*.28,angle=0.))
    boxes=[body]
    if pins==8:
        positions=[(x*w,y*h) for y in (-.32,.32) for x in (-.30,-.10,.10,.30)]
        pw,ph=w*.13,h*.16
    elif pins==2:
        body['pose'].update(w=w*.55,h=h*.75)
        positions=[(-w*.39,0),(w*.39,0)];pw,ph=w*.18,h*.70
    else:raise ValueError('起始版型只支援雙端或雙排八腳')
    for i,(x,y) in enumerate(positions,1):
        boxes.append(dict(id=f'pin-{i}',parent='body',kind='pin',pose=dict(x=x,y=y,w=pw,h=ph,angle=0.)))
    return dict(version=1,boxes=boxes)
