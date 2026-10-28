#pragma once
// The Model's private state: the one engine-free IndexedDB Catalog.
#include "web_indexeddb/model.h"
#include "domain/indexed_database/catalog.h"
namespace ruyijs::web_indexeddb {
struct Model::Impl { domain::indexed_database::Catalog catalog; };
}
