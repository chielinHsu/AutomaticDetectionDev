#include "application/interaction/SolderTeachingSession.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_set>
#include "domain/lattice/non_max_suppression.h"

namespace aoi {
std::optional<Size2D> SolderTeachingSession::cadFrameSize() const {
    const double scale=current_.board.umPerPixel>0?current_.board.umPerPixel:1;
    std::optional<Size2D> edited;
    for (const auto& type : current_.board.types) if (type.libId.starts_with("socket-cad.") && !type.groups.empty()) {
        const Size2D size{type.groups.front().widthUm/scale,type.groups.front().heightUm/scale};
        if (type.id.find("-edit-")==std::string::npos) return size;
        if (!edited) edited=size;
    }
    return edited;
}

bool SolderTeachingSession::replaceCadRegion(const SocketCadResult& cad) {
    if (!validRect(cad.region) || cad.placements.empty() || cad.placements.size()>4900 || cad.profiles.empty()) return false;
    const double scale=current_.board.umPerPixel>0?current_.board.umPerPixel:1;
    for (const auto& c : cad.placements) {
        const auto it=std::find_if(cad.profiles.begin(),cad.profiles.end(),[&](const auto& p) { return p.id==c.profileId; });
        if (it==cad.profiles.end() || !std::isfinite(c.structuralQuality) || c.structuralQuality<0 || c.structuralQuality>1 || c.pose.angleDeg!=0) return false;
        const Rect r{c.pose.x-it->bodyPx.w/2,c.pose.y-it->bodyPx.h/2,it->bodyPx.w,it->bodyPx.h};
        if (!validRect(r) || r.x<cad.region.x || r.y<cad.region.y || r.right()>cad.region.right() || r.bottom()>cad.region.bottom()) return false;
    }
    std::unordered_set<std::string> profileIds;
    for (const auto& p : cad.profiles) if (p.id.empty() || !profileIds.insert(p.id).second ||
        !std::isfinite(p.bodyPx.w) || !std::isfinite(p.bodyPx.h) || p.bodyPx.w<8 || p.bodyPx.h<8 ||
        !std::isfinite(p.windowWidthRatio) || !std::isfinite(p.windowHeightRatio) ||
        p.windowWidthRatio<=0 || p.windowWidthRatio>1 || p.windowHeightRatio<=0 || p.windowHeightRatio>1) return false;
    Recipe next=current_;
    std::erase_if(next.board.components,[&](const auto& c) {
        if (c.origin!=ObjectOrigin::Auto) return false;
        auto r=box(c.id);
        return r && r->x>=cad.region.x && r->y>=cad.region.y && r->right()<=cad.region.right() && r->bottom()<=cad.region.bottom();
    });
    if (next.board.components.size()+cad.placements.size()>5000) return false;
    std::erase_if(next.board.types,[&](const auto& t) {
        return std::none_of(next.board.components.begin(),next.board.components.end(),[&](const auto& c) { return c.typeId==t.id; });
    });
    std::map<std::string,std::string> typeIds;
    for (const auto& p : cad.profiles) {
        auto existing=std::find_if(next.board.types.begin(),next.board.types.end(),[&](const auto& t) {
            return t.libId==p.id && t.groups.size()==2 &&
                std::abs(t.groups.front().widthUm-p.bodyPx.w*scale)<1e-8 &&
                std::abs(t.groups.front().heightUm-p.bodyPx.h*scale)<1e-8;
        });
        if (existing!=next.board.types.end()) { typeIds[p.id]=existing->id; continue; }
        ComponentType type; type.id=p.id;
        while (std::any_of(next.board.types.begin(),next.board.types.end(),[&](const auto& t) { return t.id==type.id; })) type.id+="-new";
        type.libId=p.id; type.name=p.name; type.partNumber="SocketJoint";
        BoxGroup body; body.id=1; body.widthUm=p.bodyPx.w*scale; body.heightUm=p.bodyPx.h*scale;
        type.groups.push_back(body);
        BoxGroup window; window.id=2; window.kind=BoxGroupKind::Window;
        window.window.parentGroupId=1; window.window.widthRatio=p.windowWidthRatio; window.window.heightRatio=p.windowHeightRatio;
        window.window.algo="SocketJointROI";
        type.groups.push_back(window);
        typeIds[p.id]=type.id; next.board.types.push_back(std::move(type));
    }
    int id=1; for (const auto& c : next.board.components) id=std::max(id,c.id+1);
    for (const auto& c : cad.placements) {
        const auto& typeId=typeIds.at(c.profileId);
        const auto& p=*std::find_if(cad.profiles.begin(),cad.profiles.end(),[&](const auto& profile) { return profile.id==c.profileId; });
        const RectObj box{c.pose,p.bodyPx.w,p.bodyPx.h};
        const bool occupied=std::any_of(next.board.components.begin(),next.board.components.end(),[&](const auto& old) {
            if (old.origin!=ObjectOrigin::Manual || old.state==ObjectState::Sample) return false;
            const auto t=std::find_if(next.board.types.begin(),next.board.types.end(),[&](const auto& v) { return v.id==old.typeId; });
            if (t==next.board.types.end()) return false;
            return overlapRatio(box,{old.pose,t->groups.front().widthUm/scale,t->groups.front().heightUm/scale},OverlapMeasure::IntersectionOverSmaller)>0.30;
        });
        if (occupied) continue;
        ComponentInstance item; item.id=id++; item.name="CAD"+std::to_string(item.id); item.typeId=typeId;
        item.pose=c.pose; item.score=c.structuralQuality; item.origin=ObjectOrigin::Auto;
        next.board.components.push_back(std::move(item));
    }
    commit(std::move(next)); return true;
}
}  // namespace aoi
