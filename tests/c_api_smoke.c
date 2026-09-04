// SPDX-License-Identifier: MIT

#include "jr800/wasm/api.h"

int main(void) {
    jr800_machine* machine = jr800_machine_create();
    if (machine == 0) {
        return 1;
    }

    jr800_machine_state state = {0};
    const jr800_status result = jr800_machine_get_state(machine, &state);
    jr800_machine_destroy(machine);

    if (result != JR800_STATUS_OK
        || state.abi_version != JR800_WASM_ABI_VERSION) {
        return 1;
    }
    return 0;
}
