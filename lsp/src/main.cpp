#include "debug.h"
#include "server.h"
#include "transport.h"

int main() {
  // Silence all skald library logging: stdout is the LSP protocol channel and
  // any stray text corrupts the JSON-RPC stream.
  Skald::log_level = Skald::SkaldLogLevel::OFF;
  dbg_out_on = false;
  dbg_always_cout = false;
  dbg_sink = [](const std::string &) {};

  SkaldLsp::Server server;

  while (!server.should_exit()) {
    auto msg = Transport::read_message();
    if (!msg) {
      // stdin closed or read error
      break;
    }
    server.handle_message(*msg);
  }

  return 0;
}
