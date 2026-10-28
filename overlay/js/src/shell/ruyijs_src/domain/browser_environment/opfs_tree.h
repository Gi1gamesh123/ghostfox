#pragma once
// Origin private file system tree (WHATWG File System): directories with named
// children and files holding bytes. Handles keep a node alive through
// shared_ptr; a removed node is detached (parent cleared), so a stale handle
// sees NotFound, as in Firefox. Pure domain: no engine types.
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ruyijs::domain::browser_environment {
enum class OpfsStatus { Ok, NotFound, TypeMismatch, InvalidName, NotEmpty };
struct OpfsNode {
  bool directory = false;
  std::string name;
  OpfsNode* parent = nullptr;
  bool attached = true;
  std::map<std::string, std::shared_ptr<OpfsNode>> children;
  std::string data;
  double lastModified = 0;
  // Held by an open FileSystemSyncAccessHandle; createWritable() and a second
  // createSyncAccessHandle() fail with NoModificationAllowedError meanwhile.
  bool exclusiveLock = false;
};
class OpfsTree {
 public:
  OpfsTree() : root_(std::make_shared<OpfsNode>()) { root_->directory = true; }
  const std::shared_ptr<OpfsNode>& root() const { return root_; }
  // A valid entry name: non-empty, not "." or "..", no path separator or NUL.
  static bool ValidName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (char c : name) if (c == '/' || c == '\\' || c == '\0') return false;
    return true;
  }
  static bool Live(const OpfsNode& node) {
    for (const OpfsNode* cursor = &node; cursor; cursor = cursor->parent)
      if (!cursor->attached) return false;
    return true;
  }
  // getFileHandle / getDirectoryHandle: finds or (with create) makes the child.
  OpfsStatus Child(const std::shared_ptr<OpfsNode>& dir, const std::string& name, bool wantDirectory,
                   bool create, double now, std::shared_ptr<OpfsNode>* out) {
    if (!ValidName(name)) return OpfsStatus::InvalidName;
    if (!Live(*dir)) return OpfsStatus::NotFound;
    auto found = dir->children.find(name);
    if (found != dir->children.end()) {
      if (found->second->directory != wantDirectory) return OpfsStatus::TypeMismatch;
      *out = found->second;
      return OpfsStatus::Ok;
    }
    if (!create) return OpfsStatus::NotFound;
    auto node = std::make_shared<OpfsNode>();
    node->directory = wantDirectory;
    node->name = name;
    node->parent = dir.get();
    node->lastModified = now;
    dir->children.emplace(name, node);
    *out = node;
    return OpfsStatus::Ok;
  }
  OpfsStatus Remove(const std::shared_ptr<OpfsNode>& dir, const std::string& name, bool recursive) {
    if (!ValidName(name)) return OpfsStatus::InvalidName;
    if (!Live(*dir)) return OpfsStatus::NotFound;
    auto found = dir->children.find(name);
    if (found == dir->children.end()) return OpfsStatus::NotFound;
    if (found->second->directory && !found->second->children.empty() && !recursive)
      return OpfsStatus::NotEmpty;
    found->second->attached = false;
    found->second->parent = nullptr;
    dir->children.erase(found);
    return OpfsStatus::Ok;
  }
  // Renames `node` inside its parent (FileSystemHandle.move(newName)).
  OpfsStatus Rename(const std::shared_ptr<OpfsNode>& node, const std::string& name) {
    if (!ValidName(name)) return OpfsStatus::InvalidName;
    if (!Live(*node) || !node->parent) return OpfsStatus::NotFound;
    OpfsNode* parent = node->parent;
    if (name == node->name) return OpfsStatus::Ok;
    if (parent->children.count(name)) return OpfsStatus::NotEmpty;
    parent->children.erase(node->name);
    node->name = name;
    parent->children.emplace(name, node);
    return OpfsStatus::Ok;
  }
  // Path components from `ancestor` down to `node`; false when not a descendant.
  static bool Resolve(const OpfsNode& ancestor, const OpfsNode& node, std::vector<std::string>* path) {
    std::vector<std::string> reversed;
    for (const OpfsNode* cursor = &node; cursor; cursor = cursor->parent) {
      if (cursor == &ancestor) {
        path->assign(reversed.rbegin(), reversed.rend());
        return true;
      }
      reversed.push_back(cursor->name);
    }
    return false;
  }

 private:
  std::shared_ptr<OpfsNode> root_;
};
}  // namespace ruyijs::domain::browser_environment
