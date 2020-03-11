#include <access/FileUtil.h>
#include <fstream>
#include <streambuf>

namespace odr {

std::string FileUtil::read(const std::string &path) {
    std::ifstream in(path);
    std::string result;

    in.seekg(0, std::ios::end);
    result.reserve(in.tellg());
    in.seekg(0, std::ios::beg);

    result.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    return result;
}

}
