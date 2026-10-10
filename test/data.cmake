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
        REVISION "260c3af64bbc4e39959e1c385fa15927b3d710f9")

odr_test_data(
        PATH "reference-output/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.output.git"
        REVISION "965c28de8cbb8da9cd2699a3617d41753e06a1d7")

odr_test_data(
        PATH "reference-output/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.output.git"
        REVISION "138824611354e8fc9bd9f4505d190460428b2657")
