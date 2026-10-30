#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "application/Result.h"
#include "application/ports/ISocketSampleSearcher.h"
#include "application/ports/ISocketCadGenerator.h"
#include "domain/recipe.h"

namespace aoi {

// Socket MVP 的幾何與復原規則；不改變既有樣本搜尋的 InspectionSession。
class SolderTeachingSession {
public:
    void openImage(std::string path, int width, int height, double umPerPixel);
    Result<void> load(Recipe recipe);
    const Recipe& recipe() const { return current_; }
    std::optional<Rect> box(int id) const;
    bool replaceRegion(const Rect& region, const std::vector<Rect>& boxes);
    bool addBox(const Rect& rect);
    bool addSample(const Rect& rect);
    std::vector<Rect> samples() const;
    bool replaceMatches(const std::vector<SocketSampleMatch>& matches);
    bool setMatchScore(double score);
    bool replaceCadRegion(const SocketCadResult& cad);
    std::optional<Size2D> cadFrameSize() const;
    bool move(const std::vector<int>& ids, Point2D delta);
    bool resize(int id, const Rect& rect);
    bool remove(const std::vector<int>& ids);
    bool undo();
    bool redo();
    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ < history_.size(); }
    bool dirty() const { return !savedCursor_ || *savedCursor_ != cursor_; }
    void markSaved() { savedCursor_ = cursor_; }
    std::uint64_t revision() const { return revision_; }

private:
    void commit(Recipe next);
    void resetHistory();
    bool validRect(const Rect& rect) const;
    static void appendBox(Recipe& recipe, const Rect& rect, ObjectOrigin origin);
    Recipe current_;
    std::vector<Recipe> history_;
    std::size_t cursor_ = 0;
    std::optional<std::size_t> savedCursor_ = 0;
    std::uint64_t revision_ = 0;
};

}  // namespace aoi
