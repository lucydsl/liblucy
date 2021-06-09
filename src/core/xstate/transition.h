#pragma once

#include "../js_builder.h"
#include "../node.h"
#include "core.h"

void xs_compile_event_transition(PrintState*, JSBuilder*, TransitionNode*);
void xs_compile_transition_action(PrintState*, JSBuilder*, TransitionAction*, const char*);
void xs_compile_transition_key(PrintState*, JSBuilder*, Node*, char*);
void xs_compile_inner_transition(PrintState*, JSBuilder*, TransitionNode*);