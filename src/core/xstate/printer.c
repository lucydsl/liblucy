#include "printer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*set_ref)(Printer*, Ref*);

// Forget the guard, action and delay refs at the end of a machine. They
// are arena memory, so there is nothing to free. The service list is not
// reset here, as it never has been; see the services leaking into a later
// machine's options in the report on this change.
void printer_warn(Printer* printer, LucyStream stream, const char* text) {
  diagnostic_list_add_warning(printer->diagnostics, stream, text);
}

void printer_warnf(Printer* printer, LucyStream stream, const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list args_copy;
  va_copy(args_copy, args);
  int len = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  // len is negative only for an encoding error; print nothing then.
  size_t size = len > 0 ? (size_t)len + 1 : 1;
  char* text = arena_alloc(printer->arena, size);
  if(len > 0) {
    vsnprintf(text, size, format, args);
  }
  va_end(args);
  printer_warn(printer, stream, text);
}

void printer_clear_refs(Printer* printer) {
  printer->guard_refs = NULL;
  printer->action_refs = NULL;
  printer->delay_refs = NULL;
}

static void add_ref(Printer* printer, char* key, Expression* value, Ref* head, StrSet* set, set_ref setter) {
  // If already added, don't do so again.
  if(str_set_contains(set, key)) {
    return;
  }

  Ref *ref = ARENA_NEW(printer->arena, Ref);
  ref->key = key;
  ref->value = value;

  if(head == NULL) {
    setter(printer, ref);
  } else {
    Ref* cur = head; 
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = ref;
  }
  str_set_add(set, key);
}

static void set_action_ref(Printer* printer, Ref* ref) {
  printer->action_refs = ref;
}

void printer_add_action_ref(Printer* printer, char* key, Expression* value) {
  add_ref(printer, key, value, printer->action_refs, &printer->action_names, *set_action_ref);
}

static void set_guard_ref(Printer* printer, Ref* ref) {
  printer->guard_refs = ref;
}

void printer_add_guard_ref(Printer* printer, char* key, Expression* value) {
  add_ref(printer, key, value, printer->guard_refs, &printer->guard_names, *set_guard_ref);
}

static void set_delay_ref(Printer* printer, Ref* ref) {
  printer->delay_refs = ref;
}

void printer_add_delay_ref(Printer* printer, char* key, Expression* value) {
  add_ref(printer, key, value, printer->delay_refs, &printer->delay_names, *set_delay_ref);
}

static void set_service_ref(Printer* printer, Ref* ref) {
  printer->service_refs = ref;
}

void printer_add_service_ref(Printer* printer, char* key, Expression* value) {
  add_ref(printer, key, value, printer->service_refs, &printer->service_names, *set_service_ref);
}

void printer_start_assign_call(JSBuilder* jsb, AssignExpression* assign_expression) {
  js_builder_start_call(jsb, "assign");
  js_builder_start_object(jsb);
  js_builder_start_prop(jsb, assign_expression->key);
}

void printer_end_assign_call(JSBuilder* jsb) {
  js_builder_end_object(jsb);
  js_builder_end_call(jsb);
}

bool printer_add_top_level_machine_name(Printer* printer, JSBuilder* jsb, char* matching_name) {
  size_t matching_len = strlen(matching_name);
  Node* cur = printer->program->body;
  while(cur != NULL) {
    if(cur->type == NODE_MACHINE) {
      MachineNode* cur_machine = (MachineNode*)cur;
      if(cur_machine->has_name) {
        size_t len = cur_machine->name.end - cur_machine->name.start;
        if(len == matching_len &&
          memcmp(printer->source + cur_machine->name.start, matching_name, len) == 0) {
          printer_add_machine_binding_name(printer, jsb, cur_machine);
          return true;
        }
      }
    }
    cur = cur->next;
  }
  return false;
}

void printer_add_spawn_call(Printer* printer, JSBuilder* jsb, SpawnExpression* spawn_expression) {
  js_builder_add_str(jsb, "() => ");
  js_builder_start_call(jsb, "spawn");
  Expression* target = spawn_expression->target;
  char* name = NULL;
  if(target->type == EXPRESSION_IDENTIFIER) {
    name = ((IdentifierExpression*)target)->name;
    if(!printer_add_top_level_machine_name(printer, jsb, name)) {
      js_builder_add_str(jsb, name);
    }
  } else {
    name = ((SymbolExpression*)target)->name;
    js_builder_add_str(jsb, "services.");
    js_builder_add_str(jsb, name);
  }
  js_builder_add_str(jsb, ", ");
  js_builder_add_string(jsb, name);
  js_builder_end_call(jsb);
}

void printer_add_send_call(JSBuilder* jsb, SendExpression* send_expression) {
  js_builder_start_call(jsb, "send");
  js_builder_add_str(jsb, "(ctx, ev) => ({ type: ");
  js_builder_add_string(jsb, send_expression->event);
  js_builder_add_str(jsb, ", data: ev.data })");
  js_builder_add_str(jsb, ", ");
  js_builder_start_object(jsb);
  js_builder_start_prop(jsb, "to");
  js_builder_add_str(jsb, "(context) => context.");
  js_builder_add_str(jsb, send_expression->actor);
  js_builder_end_object(jsb);
  js_builder_end_call(jsb);
}