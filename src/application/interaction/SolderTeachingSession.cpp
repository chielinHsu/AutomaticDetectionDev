#include "application/interaction/SolderTeachingSession.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace aoi {
namespace {
double units(const Board& board) { return board.umPerPixel > 0 ? board.umPerPixel : 1.0; }
const ComponentType* typeOf(const Board& board, const std::string& id) {
    const auto it = std::find_if(board.types.begin(), board.types.end(), [&](const auto& t) { return t.id == id; });
    return it == board.types.end() ? nullptr : &*it;
}
bool finiteRect(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);
}
bool contains(const Rect& region, const Pose2D& pose) {
    return pose.x >= region.x && pose.x < region.x + region.w && pose.y >= region.y && pose.y < region.y + region.h;
}
}  // namespace

void SolderTeachingSession::resetHistory() {
    history_.clear(); cursor_ = 0; savedCursor_ = 0; ++revision_;
}

void SolderTeachingSession::openImage(std::string path, int width, int height, double umPerPixel) {
    current_ = Recipe{};
    current_.board.name = std::move(path);
    current_.board.imageWidth = width;
    current_.board.imageHeight = height;
    current_.board.umPerPixel = umPerPixel;
    current_.params.minScore = 0.90;
    current_.params.angleMinDeg = current_.params.angleMaxDeg = 0;
    current_.params.maxMatches = 5000;
    resetHistory();
}

bool SolderTeachingSession::validRect(const Rect& r) const {
    return finiteRect(r) && r.w >= 2 && r.h >= 2 && r.x >= 0 && r.y >= 0 &&
           r.x + r.w <= current_.board.imageWidth && r.y + r.h <= current_.board.imageHeight;
}

Result<void> SolderTeachingSession::load(Recipe recipe) {
    const Board& b = recipe.board;
    if (b.imageWidth <= 0 || b.imageHeight <= 0 || b.components.size() > 5000 ||
        !std::isfinite(b.umPerPixel) || b.umPerPixel < 0)
        return makeError(ErrorCode::InvalidArgument, "Invalid teaching recipe dimensions or capacity.");
    std::unordered_set<int> ids;
    std::unordered_set<std::string> types;
    for (const auto& t : b.types) {
        if (!types.insert(t.id).second || t.groups.empty() || t.groups.size()>8 || t.groups.front().kind!=BoxGroupKind::Body)
            return makeError(ErrorCode::InvalidArgument,"Invalid CAD profile.");
        for (std::size_t i=1;i<t.groups.size();++i) {
            const auto& g=t.groups[i]; const auto& w=g.window;
            if (g.kind!=BoxGroupKind::Window || w.parentGroupId!=t.groups.front().id ||
                !std::isfinite(w.widthRatio) || !std::isfinite(w.heightRatio) ||
                !std::isfinite(w.offsetXRatio) || !std::isfinite(w.offsetYRatio) ||
                w.widthRatio<=0 || w.heightRatio<=0 || std::abs(w.offsetXRatio)+w.widthRatio/2>0.5 ||
                std::abs(w.offsetYRatio)+w.heightRatio/2>0.5)
                return makeError(ErrorCode::InvalidArgument,"Invalid CAD inspection window.");
        }
    }
    for (const auto& c : b.components) {
        const ComponentType* t = typeOf(b, c.typeId);
        if (c.id <= 0 || c.id >= 1000000000 || !ids.insert(c.id).second ||
            !t || t->groups.front().kind != BoxGroupKind::Body ||
            !std::isfinite(c.pose.x) || !std::isfinite(c.pose.y) || c.pose.angleDeg != 0)
            return makeError(ErrorCode::InvalidArgument, "This editor requires axis-aligned CAD body boxes.");
        const auto& g = t->groups.front();
        const double w = g.widthUm / units(b), h = g.heightUm / units(b);
        if (!std::isfinite(w) || !std::isfinite(h) || w < 2 || h < 2 || g.centerXUm != 0 || g.centerYUm != 0 ||
            c.pose.x - w / 2 < 0 || c.pose.y - h / 2 < 0 || c.pose.x + w / 2 > b.imageWidth ||
            c.pose.y + h / 2 > b.imageHeight)
            return makeError(ErrorCode::InvalidArgument, "A box is outside the image or has invalid geometry.");
    }
    current_ = std::move(recipe);
    resetHistory();
    return Result<void>::success();
}

std::optional<Rect> SolderTeachingSession::box(int id) const {
    const Board& b = current_.board;
    for (const auto& c : b.components) {
        if (c.id != id) continue;
        const ComponentType* t = typeOf(b, c.typeId);
        if (!t || t->groups.empty()) return std::nullopt;
        const auto& g = t->groups.front();
        const double w = g.widthUm / units(b), h = g.heightUm / units(b);
        return Rect{c.pose.x - w / 2, c.pose.y - h / 2, w, h};
    }
    return std::nullopt;
}

void SolderTeachingSession::commit(Recipe next) {
    if (savedCursor_ && *savedCursor_ > cursor_) savedCursor_.reset();
    history_.resize(cursor_);
    // 快照歷史有界，避免大型配方在長時間手動教學中無限制累積。
    if (history_.size() == 64) {
        history_.erase(history_.begin());
        --cursor_;
        if (savedCursor_ && *savedCursor_ == 0) savedCursor_.reset();
        else if (savedCursor_) --*savedCursor_;
    }
    history_.push_back(current_);
    ++cursor_;
    current_ = std::move(next);
    ++revision_;
}

void SolderTeachingSession::appendBox(Recipe& recipe, const Rect& r, ObjectOrigin origin) {
    Board& b = recipe.board;
    int id = 1;
    for (const auto& c : b.components) id = std::max(id, c.id + 1);
    std::string typeId = "socket-" + std::to_string(id);
    while (typeOf(b, typeId)) typeId += "-new";
    ComponentType type;
    type.id = typeId; type.name = "Socket ROI " + std::to_string(id);
    BoxGroup body;
    body.id = 1; body.widthUm = r.w * units(b); body.heightUm = r.h * units(b);
    type.groups.push_back(body);
    b.types.push_back(std::move(type));
    ComponentInstance c;
    c.id = id; c.name = "ROI" + std::to_string(id); c.typeId = typeId;
    c.pose = {r.x + r.w / 2, r.y + r.h / 2, 0}; c.origin = origin;
    b.components.push_back(std::move(c));
}

bool SolderTeachingSession::replaceRegion(const Rect& region, const std::vector<Rect>& boxes) {
    if (!validRect(region) || boxes.empty() || boxes.size() > 5000) return false;
    for (const auto& r : boxes) {
        if (!validRect(r) || r.x < region.x || r.y < region.y || r.x+r.w > region.x+region.w ||
            r.y+r.h > region.y+region.h) return false;
    }
    Recipe next = current_;
    auto& b = next.board;
    std::erase_if(b.components, [&](const auto& c) { return c.origin == ObjectOrigin::Auto && contains(region, c.pose); });
    if (b.components.size() + boxes.size() > 5000) return false;
    std::erase_if(b.types, [&](const auto& t) {
        return std::none_of(b.components.begin(), b.components.end(), [&](const auto& c) { return c.typeId == t.id; });
    });
    for (const auto& r : boxes) {
        // 手動畫的框優先：避免再次掃描時在同一位置疊一個自動框。
        const bool manual = std::any_of(b.components.begin(), b.components.end(), [&](const auto& c) {
            return c.origin == ObjectOrigin::Manual && contains(r, c.pose);
        });
        if (!manual) appendBox(next, r, ObjectOrigin::Auto);
    }
    commit(std::move(next));
    return true;
}

bool SolderTeachingSession::addBox(const Rect& r) {
    if (!validRect(r) || current_.board.components.size() >= 5000) return false;
    Recipe next = current_; appendBox(next, r, ObjectOrigin::Manual); commit(std::move(next)); return true;
}

std::vector<Rect> SolderTeachingSession::samples() const {
    std::vector<Rect> result;
    for (const auto& c : current_.board.components) {
        if (c.state == ObjectState::Sample) if (auto r = box(c.id)) result.push_back(*r);
    }
    return result;
}

bool SolderTeachingSession::addSample(const Rect& r) {
    if (!validRect(r) || r.w < 8 || r.h < 8 || samples().size() >= 16 || current_.board.components.size() >= 5000) return false;
    Recipe next = current_;
    appendBox(next,r,ObjectOrigin::Manual);
    auto& c = next.board.components.back();
    c.state = ObjectState::Sample;
    c.name = "Sample" + std::to_string(c.id);
    commit(std::move(next));
    return true;
}

bool SolderTeachingSession::setMatchScore(double score) {
    if (!std::isfinite(score) || score < 0.50 || score > 0.99 || current_.params.minScore == score) return false;
    Recipe next = current_;
    next.params.minScore = score;
    commit(std::move(next));
    return true;
}

bool SolderTeachingSession::replaceMatches(const std::vector<SocketSampleMatch>& matches) {
    const auto refs = samples();
    if (refs.empty() || matches.empty() || matches.size() > 4900) return false;
    for (const auto& m : matches) {
        if (!validRect(m.rect) || !std::isfinite(m.score) || m.score < 0 || m.score > 1 ||
            std::abs(m.rect.w-refs.front().w)>1e-8 || std::abs(m.rect.h-refs.front().h)>1e-8) return false;
    }
    Recipe next = current_;
    auto& b = next.board;
    std::erase_if(b.components,[](const auto& c) { return c.origin == ObjectOrigin::Auto; });
    if (b.components.size() + matches.size() > 5000) return false;
    std::erase_if(b.types,[&](const auto& t) {
        return std::none_of(b.components.begin(),b.components.end(),[&](const auto& c) { return c.typeId == t.id; });
    });
    for (const auto& m : matches) {
        const bool manual = std::any_of(b.components.begin(),b.components.end(),[&](const auto& c) {
            return c.origin == ObjectOrigin::Manual && c.state != ObjectState::Sample && contains(m.rect,c.pose);
        });
        if (!manual) {
            appendBox(next,m.rect,ObjectOrigin::Auto);
            b.components.back().score = m.score;
        }
    }
    commit(std::move(next));
    return true;
}

bool SolderTeachingSession::move(const std::vector<int>& ids, Point2D delta) {
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || (delta.x == 0 && delta.y == 0)) return false;
    bool changed = false;
    for (int id : ids) if (auto r = box(id)) {
        r->x += delta.x; r->y += delta.y;
        if (!validRect(*r)) return false;
        changed = true;
    }
    if (!changed) return false;
    Recipe next = current_;
    for (auto& c : next.board.components) if (std::find(ids.begin(), ids.end(), c.id) != ids.end()) {
        c.pose.x += delta.x; c.pose.y += delta.y;
        if (const auto* type=typeOf(next.board,c.typeId); type && type->libId.starts_with("socket-cad.")) {
            c.origin=ObjectOrigin::Manual; c.score.reset();
        }
    }
    commit(std::move(next)); return true;
}

bool SolderTeachingSession::resize(int id, const Rect& r) {
    if (!validRect(r) || !box(id)) return false;
    Recipe next = current_;
    for (auto& c : next.board.components) if (c.id == id) {
        c.pose = {r.x+r.w/2, r.y+r.h/2, 0};
        const auto* original=typeOf(next.board,c.typeId);
        const bool cad=original && original->libId.starts_with("socket-cad.");
        const auto count=std::count_if(next.board.components.begin(),next.board.components.end(),[&](const auto& other) { return other.typeId==c.typeId; });
        if (count>1) {
            ComponentType copy=*original;
            copy.id=c.typeId+"-edit-"+std::to_string(id);
            while (typeOf(next.board,copy.id)) copy.id+="-new";
            c.typeId=copy.id;
            next.board.types.push_back(std::move(copy));
        }
        if (cad) { c.origin=ObjectOrigin::Manual; c.score.reset(); }
        for (auto& t : next.board.types) if (t.id == c.typeId) {
            t.groups.front().widthUm = r.w * units(next.board);
            t.groups.front().heightUm = r.h * units(next.board);
        }
    }
    commit(std::move(next)); return true;
}

bool SolderTeachingSession::remove(const std::vector<int>& ids) {
    Recipe next = current_;
    const auto count = std::erase_if(next.board.components, [&](const auto& c) {
        return std::find(ids.begin(), ids.end(), c.id) != ids.end();
    });
    if (count == 0) return false;
    std::erase_if(next.board.types, [&](const auto& t) {
        return std::none_of(next.board.components.begin(), next.board.components.end(), [&](const auto& c) { return c.typeId == t.id; });
    });
    commit(std::move(next)); return true;
}

bool SolderTeachingSession::undo() {
    if (!canUndo()) return false;
    std::swap(current_, history_[--cursor_]); ++revision_; return true;
}
bool SolderTeachingSession::redo() {
    if (!canRedo()) return false;
    std::swap(current_, history_[cursor_++]); ++revision_; return true;
}

}  // namespace aoi
