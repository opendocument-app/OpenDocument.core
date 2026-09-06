# The test-data repositories and the revisions the suite is pinned to.
# Fetched by `cmake/test_data.cmake`; these were git submodules before.
#
# Advancing a pin is the replacement for `git add`-ing a submodule: after
# regenerating and pushing reference output, put the new commit here.

odr_test_data(
        PATH "input/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.git"
        REVISION "2a69662064eee0b4fb368bf99a875d77ee4e74b1")

odr_test_data(
        PATH "input/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.git"
        REVISION "d0bedc89b67e73a2e4113f96f27d1d1cdf30c69f")

odr_test_data(
        PATH "reference-output/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.output.git"
        REVISION "19392dfc4e3f4c5507dae231fe96a92fe0ca29f4")

odr_test_data(
        PATH "reference-output/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.output.git"
        REVISION "ee3077eaf35b36e084f14b130c4e9236fc9be0f1")
