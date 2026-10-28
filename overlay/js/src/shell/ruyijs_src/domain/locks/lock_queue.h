#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ruyijs::domain::locks {

/// Web Locks request queues for one storage key, ported from donor
/// dom/locks/LockManagerParent.cpp (ProcessRequestQueue / IsGrantableRequest /
/// RecvPLockRequestConstructor / RecvQuery). Requests are identified by an
/// opaque id the adapter owns; granted ids are returned for the adapter to
/// settle. Pure: no engine, JSON or transport dependency.
enum class LockMode { Exclusive, Shared };

struct LockRequest {
    uint64_t id = 0;
    std::u16string name;
    LockMode mode = LockMode::Exclusive;
    std::string client;
};

struct LockInfo {
    std::u16string name;
    LockMode mode;
    std::string client;
};

class LockQueue {
public:
    /// Outcome of Enqueue: `granted` lists every request granted by this step
    /// (in grant order), `unavailable` is set when an ifAvailable request was
    /// answered with no lock, and `stolen` lists held requests that were released.
    struct Step {
        std::vector<LockRequest> granted;
        std::vector<LockRequest> stolen;
        bool unavailable = false;
    };

    Step Enqueue(const LockRequest& request, bool if_available, bool steal) {
        Step step;
        auto& queue = queues_[request.name];
        if (steal) {
            for (auto it = held_.begin(); it != held_.end();) {
                if (it->name == request.name) {
                    step.stolen.push_back(*it);
                    it = held_.erase(it);
                } else {
                    ++it;
                }
            }
            queue.insert(queue.begin(), request);
        } else if (if_available && (!queue.empty() || !Grantable(request))) {
            step.unavailable = true;
            return step;
        } else {
            queue.push_back(request);
        }
        Process(request.name, &step.granted);
        return step;
    }

    /// Releases a held lock or drops a pending request, then grants waiters.
    std::vector<LockRequest> Remove(uint64_t id) {
        std::vector<LockRequest> granted;
        std::u16string name;
        bool found = false;
        for (auto it = held_.begin(); it != held_.end(); ++it) {
            if (it->id == id) { name = it->name; held_.erase(it); found = true; break; }
        }
        if (!found) {
            for (auto& [key, queue] : queues_) {
                for (auto it = queue.begin(); it != queue.end(); ++it) {
                    if (it->id == id) { name = key; queue.erase(it); found = true; break; }
                }
                if (found) break;
            }
        }
        if (found) Process(name, &granted);
        return granted;
    }

    /// Drops every held lock and pending request of `client` (a realm going away).
    std::vector<LockRequest> RemoveClient(const std::string& client) {
        std::vector<std::u16string> names;
        for (auto it = held_.begin(); it != held_.end();) {
            if (it->client == client) { names.push_back(it->name); it = held_.erase(it); }
            else ++it;
        }
        for (auto& [key, queue] : queues_) {
            for (auto it = queue.begin(); it != queue.end();) {
                if (it->client == client) { names.push_back(key); it = queue.erase(it); }
                else ++it;
            }
        }
        std::vector<LockRequest> granted;
        for (const auto& name : names) Process(name, &granted);
        return granted;
    }

    /// RecvQuery: pending in queue-map order, then held in grant order.
    void Query(std::vector<LockInfo>* held, std::vector<LockInfo>* pending) const {
        for (const auto& [name, queue] : queues_) {
            for (const auto& request : queue) pending->push_back({request.name, request.mode, request.client});
        }
        for (const auto& request : held_) held->push_back({request.name, request.mode, request.client});
    }

private:
    bool Grantable(const LockRequest& request) const {
        for (const auto& held : held_) {
            if (held.name != request.name) continue;
            if (request.mode == LockMode::Exclusive || held.mode == LockMode::Exclusive) return false;
        }
        return true;
    }
    void Process(const std::u16string& name, std::vector<LockRequest>* granted) {
        auto found = queues_.find(name);
        if (found == queues_.end()) return;
        auto& queue = found->second;
        while (!queue.empty() && Grantable(queue.front())) {
            held_.push_back(queue.front());
            granted->push_back(queue.front());
            queue.erase(queue.begin());
        }
        if (queue.empty()) queues_.erase(found);
    }

    std::map<std::u16string, std::vector<LockRequest>> queues_;
    std::vector<LockRequest> held_;
};

}  // namespace ruyijs::domain::locks
