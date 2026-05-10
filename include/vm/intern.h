#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "value.h"

namespace Synapse {

class StringInterner {
public:
    static StringInterner& instance() {
        static StringInterner instance;
        return instance;
    }

    ObjString* intern(const std::string& str) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = internStrings.find(str);
        if (it != internStrings.end()) {
            return it->second;
        }
        
        auto* obj = new ObjString(str);
        obj->refCount = -1; // Immortal
        obj->isInterned = true;
        internStrings[str] = obj;
        return obj;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& pair : internStrings) {
            delete pair.second;
        }
        internStrings.clear();
    }

private:
    StringInterner() = default;
    ~StringInterner() { clear(); }

    std::unordered_map<std::string, ObjString*> internStrings;
    std::mutex mutex;
};

} // namespace Synapse
