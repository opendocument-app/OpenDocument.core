#ifndef ODR_SVM2SVG_H
#define ODR_SVM2SVG_H

#include <memory>
#include <iostream>

namespace odr {

class Svm2Svg final {
public:
    Svm2Svg();
    ~Svm2Svg();

    bool translate(std::istream &in, std::ostream &out) const;
};

}

#endif //ODR_SVM2SVG_H
