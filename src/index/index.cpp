#include "nx/index/index.hpp"
#include "nx/index/sqlite_index.hpp"
#include "nx/index/ripgrep_index.hpp"

namespace nx::index {

std::unique_ptr<Index> IndexFactory::createSqliteIndex(const std::filesystem::path& db_path) {
  return std::make_unique<SqliteIndex>(db_path);
}

std::unique_ptr<Index> IndexFactory::createRipgrepIndex(const std::filesystem::path& notes_dir) {
  return std::make_unique<RipgrepIndex>(notes_dir);
}

}  // namespace nx::index