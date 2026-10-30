#include "application/usecases/GenerateSocketCad.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include "domain/lattice/lattice_estimator.h"

namespace aoi {
Result<SocketCadResult> GenerateSocketCad::generate(const SocketCadRequest& r, const CancelToken& cancel) const {
    try {
        if (!recognizer_) return makeError(ErrorCode::BackendUnavailable,"Socket CAD recognition is unavailable.");
        if (!r.image.valid() || !std::isfinite(r.region.x) || !std::isfinite(r.region.y) ||
            !std::isfinite(r.region.w) || !std::isfinite(r.region.h) || r.region.w<2 || r.region.h<2 ||
            r.region.x<0 || r.region.y<0 || r.region.right()>r.image.width || r.region.bottom()>r.image.height)
            return makeError(ErrorCode::InvalidArgument,"Select a valid image region.");
        const bool automatic = r.frameSize.w==0 && r.frameSize.h==0;
        if (!automatic && (!std::isfinite(r.frameSize.w) || !std::isfinite(r.frameSize.h) ||
            r.frameSize.w<8 || r.frameSize.h<8)) return makeError(ErrorCode::InvalidArgument,"CAD size must be at least 8 pixels.");
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled,"CAD registration cancelled.");
        auto detected = recognizer_->recognize(r.image,r.region,cancel);
        if (!detected) return detected.error();
        const auto& data = detected.value();
        if (data.joints.size()>12000) return makeError(ErrorCode::InvalidArgument,"Too many Socket observations.");
        for (const auto& j : data.joints) if (!std::isfinite(j.center.x) || !std::isfinite(j.center.y) ||
            !std::isfinite(j.structuralQuality) || j.structuralQuality<0 || j.structuralQuality>1)
            return makeError(ErrorCode::BackendFailed,"Invalid Socket observation.");
        SocketCadResult result; result.region=r.region; result.evaluated=data.evaluated;
        if (data.joints.empty()) return result;
        const Size2D size = automatic ? data.suggestedFrame : r.frameSize;
        if (!std::isfinite(size.w) || !std::isfinite(size.h) || size.w<8 || size.h<8)
            return makeError(ErrorCode::InvalidArgument,"Insufficient Socket structure to estimate CAD dimensions.");
        std::vector<Point2D> points; points.reserve(data.joints.size());
        for (const auto& j : data.joints) points.push_back(j.center);
        std::vector<Point2D> positions=points;
        std::vector<bool> snapped(points.size(),false);
        if (r.snapPositions && points.size()>=6) {
            const auto fit=LatticeEstimator().fitAll(points);
            for (const auto& model : fit.models) {
                if (model.confidence<0.6) continue;
                for (std::size_t k=0;k<model.inliers.size();++k) {
                    const std::size_t index=model.inliers[k];
                    const Point2D p=model.basis.at(model.cells[k]);
                    if (index<points.size() && distance(points[index],p)<=0.4*std::min(size.w,size.h)) {
                        positions[index]=p; snapped[index]=true;
                    }
                }
            }
        }
        bool left=false,right=false;
        for (std::size_t i=0;i<data.joints.size();++i) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled,"CAD registration cancelled.");
            const auto p=positions[i];
            const Rect body{p.x-size.w/2,p.y-size.h/2,size.w,size.h};
            if (body.x<r.region.x || body.y<r.region.y || body.right()>r.region.right() || body.bottom()>r.region.bottom()) continue;
            const bool isLeft=data.joints[i].padSide==SocketPadSide::Left;
            left=left||isLeft; right=right||!isLeft;
            result.placements.push_back({isLeft?"socket-cad.pad-left":"socket-cad.pad-right",
                {p.x,p.y,0},data.joints[i].structuralQuality,snapped[i]});
        }
        if (left) result.profiles.push_back({"socket-cad.pad-left","Socket 端點（焊墊在左）",size});
        if (right) result.profiles.push_back({"socket-cad.pad-right","Socket 端點（焊墊在右）",size});
        if (result.placements.size()>4900) return makeError(ErrorCode::InvalidArgument,"Too many CAD objects. Select a smaller region.");
        return result;
    } catch (const std::exception& e) { return makeError(ErrorCode::Internal,e.what()); }
    catch (...) { return makeError(ErrorCode::Internal,"CAD registration failed."); }
}
}  // namespace aoi
