# The test-data repositories and the revisions the suite is pinned to.
# Fetched by `cmake/test_data.cmake`; these were git submodules before.
#
# Advancing a pin is the replacement for `git add`-ing a submodule: after
# regenerating and pushing reference output, put the new commit here.

odr_test_data(
        PATH "input/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.git"
        REVISION "c70c296ebe08ba4e109312dcbf55227206b44eaa")

odr_test_data(
        PATH "input/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.git"
        REVISION "d0bedc89b67e73a2e4113f96f27d1d1cdf30c69f")

odr_test_data(
        PATH "reference-output/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.output.git"
        REVISION "deb25f76e313bd3c5242b57d1aca7de31f1f46ef")

odr_test_data(
        PATH "reference-output/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.output.git"
        REVISION "bcea37b997a045387b7069412ba668fa44a7e26b")
