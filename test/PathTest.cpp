#include <unordered_map>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "io/Path.h"

namespace odr {

TEST(Path, empty) {
    Path a;
    Path b = "a";
    LOG(INFO) << a.hash();
    LOG(INFO) << b.hash();
    LOG(INFO) << (a == b);
    LOG(INFO) << (a != b);
    LOG(INFO) << (a < b);
}

TEST(Path, unordered_map) {
    std::unordered_map<Path, int> map;

    map["a"] = 1;
    map["b"] = 2;
    map[""] = 3;
    map["c/d"] = 4;
    map["a"] = 5;

    for (const auto &i : map) {
        LOG(INFO) << i.first << " " << i.second;
    }
}

TEST(Path, nestedAssign) {
    struct Entry {
        Path path;
        std::size_t sizeReal;
        std::size_t sizeUncompressed;
        std::string mediaType;
        bool encrypted;
    };

    std::unordered_map<Path, Entry> map;

    map["asadf"] = {};

    for (const auto &i : map) LOG(INFO) << i.first;
}

}
