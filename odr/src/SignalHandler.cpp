#include <SignalHandler.h>
#include <csignal>
#include <glog/logging.h>

namespace odr {

namespace {
typedef void (*signal_handler_t) (int);

volatile signal_handler_t previousSegvHandler{nullptr};
volatile signal_handler_t previousAbrtHandler{nullptr};

void signal_handler(const int signal) {
  std::signal(signal, SIG_DFL);
  LOG(ERROR) << "caught signal " << signal;
  if ((signal == SIGSEGV) && (previousSegvHandler != nullptr))
    previousSegvHandler(signal);
  else if ((signal == SIGABRT) && (previousAbrtHandler != nullptr))
    previousAbrtHandler(signal);
  else
    std::raise(SIGABRT);
}
}

void SignalHandler::sigsegv() {
  std::raise(SIGSEGV);
}

void SignalHandler::derefnullptr() {
  int *i = nullptr;
  *i = 0;
}

void SignalHandler::install() {
  signal_handler_t result;

  result = std::signal(SIGSEGV, &signal_handler);
  if (result == SIG_ERR)
    throw 1; // TODO
  previousSegvHandler = result;

  std::signal(SIGABRT, &signal_handler);
  if (result == SIG_ERR)
    throw 1; // TODO
  previousAbrtHandler = result;
}

void SignalHandler::uninstall() {
  signal_handler_t result;

  result = std::signal(SIGINT, previousSegvHandler);
  if (result == SIG_ERR)
    throw 1; // TODO
  previousSegvHandler = nullptr;

  result = std::signal(SIGABRT, previousAbrtHandler);
  if (result == SIG_ERR)
    throw 1; // TODO
  previousAbrtHandler = nullptr;
}

}
