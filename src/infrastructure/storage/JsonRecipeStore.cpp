#include "infrastructure/storage/JsonRecipeStore.h"

#include <QJsonObject>
#include <QString>
#include <new>
#include <string>
#include <utility>

#include "infrastructure/storage/JsonCodec.h"

namespace aoi::infra {

std::vector<json::Migration> JsonRecipeStore::builtInMigrations() { return {}; }  // v1 是第一版

JsonRecipeStore::JsonRecipeStore() : JsonRecipeStore(kSchemaVersion, builtInMigrations()) {}

JsonRecipeStore::JsonRecipeStore(int current, std::vector<json::Migration> migrations)
    : current_(current), migrations_(std::move(migrations)) {}

Result<void> JsonRecipeStore::save(const Recipe& recipe, std::string_view path) {
    try {
        Result<QJsonObject> board = json::encodeBoard(recipe.board);
        if (!board) return board.error();
        Result<QJsonObject> params = json::encodeSearchParams(recipe.params);
        if (!params) return params.error();

        QJsonObject root;
        root.insert(QStringLiteral("format"), QString::fromLatin1(kFormat));
        root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
        root.insert(QStringLiteral("board"), board.value());
        root.insert(QStringLiteral("params"), params.value());
        return json::writeDocument(path, root);
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to save the recipe.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while saving the recipe.");
    }
}

Result<Recipe> JsonRecipeStore::load(std::string_view path) {
    try {
        Result<QJsonObject> root = json::readDocument(path, kFormat);
        if (!root) return root.error();
        QJsonObject& r = root.value();
        const Result<int> version = json::migrate(r, current_, migrations_);
        if (!version) return version.error();

        Result<Board> board = json::decodeBoard(r.value(QStringLiteral("board")), "board");
        if (!board) return board.error();
        Result<SearchParams> params = json::decodeSearchParams(r.value(QStringLiteral("params")), "params");
        if (!params) return params.error();

        Recipe recipe;
        recipe.schemaVersion = version.value();
        recipe.board = std::move(board.value());
        recipe.params = params.value();
        return recipe;
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to load the recipe.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while loading the recipe.");
    }
}

}  // namespace aoi::infra
