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
        REVISION "c4efe97d67c9a12aa08965d696a0cb7a3dc4d025")

odr_test_data(
        PATH "reference-output/odr-public"
        URL "https://github.com/opendocument-app/OpenDocument.test.output.git"
        REVISION "a213baa014e4fb4328360602345e964f360e1445")

odr_test_data(
        PATH "reference-output/odr-private"
        URL "https://github.com/opendocument-app/OpenDocument.test-private.output.git"
        REVISION "80172fcc670d29c4879cf97748546641309033bb")
