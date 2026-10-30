#include "infrastructure/backend_opencv/OpenCvSocketRegionRecognizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <opencv2/imgproc.hpp>
#include "infrastructure/backend_opencv/OpenCvImages.h"

namespace aoi::infra {
namespace {
struct Blob {
    double x=0,y=0,w=0,h=0,area=0;
    bool boundary=false;
    Point2D center() const { return {x+w/2,y+h/2}; }
};
struct Paired { Blob blob; SocketPadSide side; double quality; };
struct Family { double w=0,h=0; std::vector<Paired> joints; };
double median(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(),v.end());
    return v.size()%2 ? v[v.size()/2] : 0.5*(v[v.size()/2-1]+v[v.size()/2]);
}
bool sameScale(const Blob& b,double w,double h) {
    return b.w/w>=0.75 && b.w/w<=1.30 && b.h/h>=0.75 && b.h/h<=1.30;
}
std::vector<Paired> uniqueJoints(const std::vector<Paired>& input) {
    std::vector<Paired> result;
    for (const auto& p : input) {
        auto it=std::find_if(result.begin(),result.end(),[&](const auto& q) {
            return distance(p.blob.center(),q.blob.center())<0.5*std::min(p.blob.h,q.blob.h);
        });
        if (it==result.end()) result.push_back(p);
        else if (p.blob.area>it->blob.area) *it=p;
    }
    return result;
}
}

Result<SocketRegionObservation> OpenCvSocketRegionRecognizer::recognize(const ImageView& image,const Rect& region,
                                                                       const CancelToken& cancel) const {
    try {
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled,"Socket recognition cancelled.");
        if (!image.valid() || !std::isfinite(region.x) || !std::isfinite(region.y) ||
            !std::isfinite(region.w) || !std::isfinite(region.h) || region.w<2 || region.h<2 ||
            region.x<0 || region.y<0 || region.right()>image.width || region.bottom()>image.height)
            return makeError(ErrorCode::InvalidArgument,"Invalid Socket region.");
        // 周邊僅作類型／尺度證據；generator 仍只在使用者 ROI 內建立 CAD。
        const int x=std::max(0,static_cast<int>(std::floor(region.x))-256);
        const int y=std::max(0,static_cast<int>(std::floor(region.y))-256);
        const int right=std::min(image.width,static_cast<int>(std::ceil(region.right()))+256);
        const int bottom=std::min(image.height,static_cast<int>(std::ceil(region.bottom()))+256);
        const auto gray=toGray8(image,{x,y,right-x,bottom-y});
        if (gray.mat.empty()) return makeError(ErrorCode::FormatInvalid,"Cannot read the Socket image.");
        cv::Mat mask;
        const double otsu=cv::threshold(gray.mat,mask,0,255,cv::THRESH_BINARY|cv::THRESH_OTSU);
        std::vector<int> levels{static_cast<int>(std::max(32.0,otsu*0.70)),
            static_cast<int>(std::max(48.0,otsu*0.90)),static_cast<int>(otsu),
            static_cast<int>(std::min(240.0,std::max(200.0,otsu)))};
        std::sort(levels.begin(),levels.end()); levels.erase(std::unique(levels.begin(),levels.end()),levels.end());
        std::vector<Paired> paired;
        SocketRegionObservation result;
        for (int level : levels) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled,"Socket recognition cancelled.");
            cv::threshold(gray.mat,mask,level,255,cv::THRESH_BINARY);
            cv::Mat labels,stats,centroids;
            const int count=cv::connectedComponentsWithStats(mask,labels,stats,centroids,8,CV_32S);
            std::vector<Blob> blobs;
            for (int i=1;i<count;++i) {
                const int w=stats.at<int>(i,cv::CC_STAT_WIDTH),h=stats.at<int>(i,cv::CC_STAT_HEIGHT);
                const int a=stats.at<int>(i,cv::CC_STAT_AREA);
                if (w<6 || h<4 || w>512 || h>256 || a<20 || static_cast<double>(a)/(w*h)<0.30) continue;
                const int bx=stats.at<int>(i,cv::CC_STAT_LEFT),by=stats.at<int>(i,cv::CC_STAT_TOP);
                blobs.push_back({static_cast<double>(x+bx),static_cast<double>(y+by),static_cast<double>(w),
                    static_cast<double>(h),static_cast<double>(a),bx==0 || by==0 || bx+w>=mask.cols || by+h>=mask.rows});
            }
            result.evaluated+=blobs.size();
            for (const auto& pin : blobs) {
                if (cancel.cancelled()) return makeError(ErrorCode::Cancelled,"Socket recognition cancelled.");
                const double aspect=pin.w/pin.h;
                if (pin.boundary || aspect<1.40 || aspect>3.5) continue;
                const auto pc=pin.center();
                double best=0; SocketPadSide side=SocketPadSide::Left;
                for (const auto& pad : blobs) {
                    if (pad.w<pin.w*1.6 || pad.h<pin.h*1.6 || pad.area<pin.area*4 || pad.area>pin.area*30) continue;
                    const auto c=pad.center();
                    const double gap=std::abs(c.x-pc.x)-(pin.w+pad.w)/2;
                    const double dy=std::abs(c.y-pc.y),limit=0.45*pad.h+0.25*pin.h;
                    if (gap<-0.1*pin.w || gap>2.25*pin.w || dy>limit) continue;
                    const double quality=0.7+0.3*(1.0-dy/std::max(1.0,limit));
                    if (quality>best) { best=quality; side=c.x<pc.x?SocketPadSide::Left:SocketPadSide::Right; }
                }
                if (best>0) paired.push_back({pin,side,best});
            }
        }
        if (paired.empty()) return result;
        std::sort(paired.begin(),paired.end(),[](const auto& a,const auto& b) { return a.blob.area>b.blob.area; });
        std::vector<Family> families;
        for (const auto& p : paired) {
            auto it=std::find_if(families.begin(),families.end(),[&](const auto& f) { return sameScale(p.blob,f.w,f.h); });
            if (it==families.end()) families.push_back({p.blob.w,p.blob.h,{p}});
            else it->joints.push_back(p);
        }
        std::vector<Paired> best;
        double bestSupport=0;
        for (const auto& family : families) {
            auto joints=uniqueJoints(family.joints);
            if (joints.size()<3) continue;
            std::vector<double> areas;
            for (const auto& j : joints) areas.push_back(j.blob.area);
            const double support=static_cast<double>(joints.size())*std::sqrt(median(areas));
            if (support>bestSupport) { bestSupport=support; best=std::move(joints); }
        }
        if (best.empty()) return result;
        std::vector<double> widths,heights;
        for (const auto& j : best) { widths.push_back(j.blob.w); heights.push_back(j.blob.h); }
        const double mw=median(widths),mh=median(heights);
        result.suggestedFrame={std::max(8.0,std::round(mw+4)),std::max(8.0,std::round(mh+4))};
        for (const auto& j : best) {
            if (!sameScale(j.blob,mw,mh)) continue;
            const auto c=j.blob.center();
            const bool repeated=std::any_of(best.begin(),best.end(),[&](const auto& other) {
                const auto p=other.blob.center(); const double d=distance(c,p);
                return d>1.5*std::max(mw,mh) && d<8*std::max(mw,mh) &&
                    (std::abs(c.x-p.x)<mw || std::abs(c.y-p.y)<mh);
            });
            if (!repeated) continue;
            const double scale=std::exp(-std::abs(std::log(j.blob.w/mw))-std::abs(std::log(j.blob.h/mh)));
            result.joints.push_back({c,{j.blob.w,j.blob.h},j.side,0.5*j.quality+0.5*scale});
        }
        if (result.joints.size()>12000) return makeError(ErrorCode::InvalidArgument,"Too many structures. Select a smaller region.");
        return result;
    } catch (const cv::Exception& e) { return makeError(ErrorCode::BackendFailed,e.what()); }
    catch (const std::exception& e) { return makeError(ErrorCode::BackendFailed,e.what()); }
    catch (...) { return makeError(ErrorCode::BackendFailed,"Socket recognition failed."); }
}
}  // namespace aoi::infra
