#pragma once

#include "../js_builder.h"
#include "../node.h"
#include "core.h"
#include "ts_printer.h"

void xs_add_machine_binding_name(JSBuilder*, MachineNode*);
void xs_enter_machine(PrintState*, JSBuilder*, Node*);
void xs_exit_machine(PrintState*, JSBuilder*, Node*);