# The test-data repositories and the revisions the suite is pinned to.
# Fetched by `cmake/test_data.cmake`; these were git submodules before.
#
# Advancing a pin is the replacement for `git add`-ing a submodule: after
# regenerating and pushing reference output, put the new commit here.

odr_test_data(
        PATH "input/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.git"
        REVISION "3ebef9a334ca840b95315bcf726d8ae3edf8c4f6")

odr_test_data(
        PATH "input/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.git"
        REVISION "260c3af64bbc4e39959e1c385fa15927b3d710f9")

odr_test_data(
        PATH "reference-output/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.output.git"
        REVISION "872852bca3c0c6094e2def8fa95d0d26419b8d12")

odr_test_data(
        PATH "reference-output/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.output.git"
        REVISION "a57bb006d1a38712e4a72486afbcebadce59c8fa")
