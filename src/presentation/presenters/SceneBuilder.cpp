#include "presentation/presenters/SceneBuilder.h"

#include <algorithm>

#include "domain/box_expander.h"

namespace aoi::ui {

namespace {

// 低於「門檻＋這個值」的找到的框，算信心偏低（S01 原型的 LOWCONF）。
constexpr double kLowConfidenceMargin = 0.10;

OverlayState toOverlay(ObjectState s) {
    switch (s) {
        case ObjectState::Sample: return OverlayState::Sample;
        case ObjectState::Found: return OverlayState::Found;
        case ObjectState::Inferred: return OverlayState::Inferred;
        case ObjectState::Rejected: return OverlayState::Rejected;
    }
    return OverlayState::Found;
}

BoxKind toBoxKind(BoxGroupKind k) {
    switch (k) {
        case BoxGroupKind::Body: return BoxKind::Body;
        case BoxGroupKind::Pin: return BoxKind::Pin;
        case BoxGroupKind::Window: return BoxKind::Window;
    }
    return BoxKind::Body;
}

QRectF toQRect(const RectObj& r) {
    const Rect box = boundingRect(r);  // 畫布目前只畫軸對齊的框；旋轉的框用外接矩形
    return QRectF(box.x, box.y, box.w, box.h);
}

QString scoreText(double score) { return QString::number(score, 'f', 2); }

// 一個群組有幾個框（Window 與父群組一樣多）。
int groupBoxCount(const ComponentType& type, const BoxGroup& group) {
    auto countOf = [](const BoxGroup& g) {
        if (g.kind == BoxGroupKind::Body) return 1;
        if (g.kind != BoxGroupKind::Pin) return 0;
        int n = g.matrix.rows * g.matrix.cols;
        for (const MatrixCell& c : g.matrix.skipped) {
            if (c.row >= 0 && c.row < g.matrix.rows && c.col >= 0 && c.col < g.matrix.cols) --n;
        }
        return std::max(0, n);
    };
    if (group.kind != BoxGroupKind::Window) return countOf(group);
    for (const BoxGroup& parent : type.groups) {
        if (parent.id == group.window.parentGroupId && parent.kind != BoxGroupKind::Window) return countOf(parent);
    }
    return 0;
}

void addToCounts(SceneCounts& counts, OverlayState state) {
    switch (state) {
        case OverlayState::Sample:
        case OverlayState::Found: ++counts.found; break;
        case OverlayState::Inferred: ++counts.inferred; break;
        case OverlayState::Rejected: ++counts.rejected; break;
    }
}

}  // namespace

Scene buildScene(const InspectionSession& session, const SceneLabels& labels) {
    const InspectionDocument& document = session.document();
    const Board& board = document.board();
    const ProposalSession* proposal = session.proposal();

    auto snapshot = std::make_shared<ComponentsSnapshot>();
    snapshot->components.reserve(board.components.size() + (proposal ? proposal->proposal().candidates.size() : 0));

    // ---- 文件裡的元件 ----
    auto docRows = std::make_shared<ObjectRows>();
    docRows->boardTitle = labels.board;
    SceneCounts docCounts;
    docRows->types.reserve(board.types.size());
    for (const ComponentType& t : board.types) {
        ObjectTypeRow row;
        row.label = QString::fromStdString(t.partNumber);
        for (const BoxGroup& g : t.groups) row.groups.push_back({g.kind, groupBoxCount(t, g)});
        docRows->types.push_back(std::move(row));
    }
    docRows->rows.reserve(board.components.size());

    for (const ComponentInstance& c : board.components) {
        std::size_t typeIndex = board.types.size();
        for (std::size_t i = 0; i < board.types.size(); ++i) {
            if (board.types[i].id == c.typeId) {
                typeIndex = i;
                break;
            }
        }
        const OverlayState state = toOverlay(c.state);
        ComponentView view;
        view.id = c.id;
        view.state = state;
        if (typeIndex < board.types.size()) {
            const std::vector<ExpandedBox> boxes = expandBoxes(board.types[typeIndex], c.pose, board.umPerPixel);
            view.boxes.reserve(boxes.size());
            QRectF outline;
            for (const ExpandedBox& b : boxes) {
                const QRectF r = toQRect(b.rect);
                view.boxes.push_back({r, toBoxKind(b.kind)});
                outline = outline.isNull() ? r : outline.united(r);
            }
            view.outline = outline;
        }
        if (c.origin == ObjectOrigin::Manual) view.label = QString::fromStdString(c.name) + QLatin1Char(' ') + labels.manual;
        else if (c.score) view.label = QString::fromStdString(c.name) + QLatin1Char(' ') + scoreText(*c.score);
        else view.label = QString::fromStdString(c.name);
        docCounts.boxes += view.boxes.size();
        ++docCounts.components;
        addToCounts(docCounts, state);
        if (state == OverlayState::Inferred) ++docCounts.lowConfidence;

        ObjectRow row;
        row.id = c.id;
        row.name = QString::fromStdString(c.name);
        row.state = state;
        row.score = c.score ? *c.score : -1.0;
        row.manual = c.origin == ObjectOrigin::Manual;
        row.typeIndex = typeIndex < board.types.size() ? static_cast<int>(typeIndex) : -1;
        docRows->rows.push_back(std::move(row));
        ++docRows->stateCounts[static_cast<std::size_t>(stateIndex(state))];
        snapshot->components.push_back(std::move(view));
    }

    Scene scene;
    if (proposal == nullptr) {
        scene.snapshot = std::move(snapshot);
        scene.rows = std::move(docRows);
        scene.counts = docCounts;
        return scene;
    }

    // ---- 目前的提案（尚未接受）----
    const ProposalObjects objects = proposal->objects();
    auto propRows = std::make_shared<ObjectRows>();
    propRows->boardTitle = labels.board;
    propRows->proposal = true;
    propRows->rows.reserve(objects.objects.size());
    propRows->types.push_back(ObjectTypeRow{QString(), {ObjectGroupRow{BoxGroupKind::Body, 1}}});
    SceneCounts counts;
    counts.fromProposal = true;
    const double lowBelow = proposal->threshold() + kLowConfidenceMargin;

    for (const InspectionObject& o : objects.objects) {
        const OverlayState state = toOverlay(o.state);
        const int sceneId = kProposalIdBase + o.id;
        const QRectF rect = toQRect(o.rect);

        ComponentView view;
        view.id = sceneId;
        view.state = state;
        view.outline = rect;
        view.boxes.push_back({rect, BoxKind::Body});
        if (o.origin == ObjectOrigin::Manual) view.label = labels.manual;
        else if (o.state == ObjectState::Sample) view.label = labels.sample;
        else if (o.state == ObjectState::Inferred) view.label = labels.inferred;
        else if (o.score) view.label = scoreText(*o.score);
        snapshot->components.push_back(std::move(view));

        ObjectRow row;
        row.id = sceneId;
        row.name = QStringLiteral("#%1").arg(o.id);
        row.state = state;
        row.score = o.score ? *o.score : -1.0;
        row.manual = o.origin == ObjectOrigin::Manual;
        row.typeIndex = 0;
        propRows->rows.push_back(std::move(row));
        ++propRows->stateCounts[static_cast<std::size_t>(stateIndex(state))];

        ++counts.components;
        ++counts.boxes;
        addToCounts(counts, state);
        const bool lowFound = o.state == ObjectState::Found && o.origin == ObjectOrigin::Auto && o.score && *o.score < lowBelow;
        if (o.state == ObjectState::Inferred || lowFound) ++counts.lowConfidence;
    }

    scene.snapshot = std::move(snapshot);
    scene.rows = std::move(propRows);
    scene.counts = counts;
    return scene;
}

}  // namespace aoi::ui
