#pragma once

#include "../js_builder.h"
#include "../node.h"
#include "core.h"

void xs_enter_state(PrintState*, JSBuilder*, Node*);
void xs_exit_state(PrintState*, JSBuilder*, Node*);