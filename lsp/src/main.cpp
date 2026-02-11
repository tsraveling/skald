#include "debug.h"
#include "server.h"
#include "transport.h"

int main() {
    // Disable debug output from skald library
    Skald::log_level = Skald::SkaldLogLevel::SPARSE;
    dbg_out_on = false;

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
