#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "io/ZipStorage.h"

TEST(ZipWriterTest, create) {
    const std::string output = "../../test/created.zip";

    odr::ZipWriter writer(output);

    {
        const auto sink = writer.write("one.txt");
        sink->write("this is written at once", 23);
    }

    {
        const auto sink = writer.write("two.txt");
        sink->write("this is written ", 15);
        sink->write("at two stages", 13);
    }
}

TEST(ZipWriterTest, copy) {
    const std::string input = "/home/andreas/Desktop/odr/megatest.odt";
    const std::string output = "../../test/copied.zip";

    odr::ZipReader reader(input);
    odr::ZipWriter writer(output);

    reader.visit([&] (const auto &p) {
        writer.copy(reader, p);
    });
}

TEST(ZipReaderTest, exception) {
    try {
        odr::ZipReader reader("/");
    } catch (odr::NoZipFileException &e) {
        LOG(ERROR) << "not a zip file";
    }
}
