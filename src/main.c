#include "attacks.h"
#include "zobrist.h"
#include "uci.h"

int main(void) {
    attacks_init();
    zobrist_init();
    uci_run();
    return 0;
}
