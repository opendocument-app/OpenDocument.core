#ifndef ODR_SIGNALHANDLER_H
#define ODR_SIGNALHANDLER_H

namespace odr {

class SignalHandler {
public:
  // TODO remove
  static void sigsegv();
  static void derefnullptr();

  static void install();
  static void uninstall();
};

}

#endif // ODR_SIGNALHANDLER_H
