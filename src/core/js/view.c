#include "printer.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../support/str_set.h"

/*
 * Views. A machine's markup is parsed once, the first time a machine of
 * its kind is made, into a <template>, whose fragment each machine clones.
 * The nodes that change or listen are found by walking from the top, with
 * firstChild and nextSibling. In the markup, <!> marks where a key's text,
 * a list of machines or a state's part goes.
 *
 * Nothing compares or re-renders. The compiler knows where each key is
 * assigned, so the text that shows it is changed right after the assign;
 * append and remove each add or take out one machine's nodes; and a
 * state's part goes in and out in its enter and exit functions.
 */

// Names a view's variables can't take: the output's own, and JavaScript's.
static const char* const taken_names[] = {
  "machine", "states", "context", "event", "listeners", "notify", "queue", "busy", "run", "parent",
  "dom", "nodes", "template", "data", "handler", "listener", "promise", "error", "child", "actions",
  "assigns", "delays", "guards", "services", "actor", "node", "part", "attr", "element", "name", "value",
  "var", "new", "delete", "in", "do", "if", "for", "class", "const", "let", "switch", "case",
  "default", "return", "this", "with", "void", "typeof", "function", NULL
};

// A name for a variable of the view, from base: base, or base2, base3, ...
static const char* view_name(Printer* printer, JSView* view, const char* base) {
  const char* name = base;
  for(unsigned n = 2; str_set_contains(&view->names, name) || printer_is_imported(printer, name); n++) {
    char suffix[16];
    snprintf(suffix, sizeof(suffix), "%u", n);
    name = printer_concat(printer, base, suffix);
  }
  str_set_add(&view->names, name);
  return name;
}

// A tag as a variable's name: my-list as myList.
static const char* tag_name(Printer* printer, const char* tag) {
  StrBuilder* sb = str_builder_create();
  bool upper = false;
  for(const char* c = tag; *c != '\0'; c++) {
    if(*c == '-' || *c == ':' || *c == '.') {
      upper = true;
      continue;
    }
    char ch = *c;
    if(upper && ch >= 'a' && ch <= 'z') {
      ch = (char)(ch - 'a' + 'A');
    } else if(!upper && ch >= 'A' && ch <= 'Z') {
      ch = (char)(ch - 'A' + 'a');
    }
    upper = false;
    str_builder_add_char(sb, ch);
  }
  const char* name = arena_strdup(printer->arena, str_builder_peek(sb));
  str_builder_destroy(sb);
  return name;
}

// The attributes a bound key sets as a property, as the element's state
// can move away from its attribute once someone types or clicks.
static bool is_property(const char* name) {
  return strcmp(name, "value") == 0 || strcmp(name, "checked") == 0 || strcmp(name, "selected") == 0 ||
         strcmp(name, "indeterminate") == 0;
}

// Note whether any attribute bound to a key is set with the attr helper.
static void find_bound_attributes(Printer* printer, ViewNode* node) {
  for(; node != NULL; node = node->next) {
    for(ViewAttribute* attribute = node->attributes; attribute != NULL; attribute = attribute->next) {
      if(attribute->key != NULL && !is_property(attribute->name)) {
        printer->needs_attr = true;
      }
    }
    find_bound_attributes(printer, node->child);
  }
}

static void find_slots(Printer* printer, JSView* view, ViewNode* node) {
  for(; node != NULL; node = node->next) {
    if(node->type == VIEW_PART) {
      if(str_map_get(view->slots, node->name) != NULL) {
        printer_warnf(printer, LUCY_STREAM_STDOUT, "The view has {part(%s)} more than once; only the first is used.\n", node->name);
      } else {
        // Its variable is named when the view is printed.
        str_map_set(view->slots, node->name, node);
      }
    }
    find_slots(printer, view, node->child);
  }
}

static void prepare_parts(Printer* printer, JSMachine* machine, JSState* state) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->part_name != NULL) {
      if(machine->view == NULL || str_map_get(machine->view->slots, node->part_name) == NULL) {
        printer_warnf(printer, LUCY_STREAM_STDOUT,
          "State [%s] has a part, view(%s), but the machine's view has no {part(%s)} to put it in.\n",
          state->path, node->part_name, node->part_name);
      } else {
        const char* name = printer_capitalize(printer, node->part_name);
        state->part_var = view_name(printer, machine->view, printer_concat(printer, state->var, name));
        state->part_make = view_name(printer, machine->view, printer_concat(printer, printer_concat(printer, "make", state->ident), name));
        find_bound_attributes(printer, node->part);
        state->part_template = printer_free_name(printer,
          printer_concat(printer, printer_concat(printer, printer_concat(printer, machine->name, state->ident), name), "View"));
      }
    }
    prepare_parts(printer, machine, state->child);
  }
}

void printer_prepare_views(Printer* printer) {
  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    MachineNode* node = machine->node;
    if(node->has_view) {
      JSView* view = ARENA_NEW(printer->arena, JSView);
      view->lists = str_map_create(printer->arena);
      view->slots = str_map_create(printer->arena);
      str_set_init(&view->names, printer->arena);
      for(size_t i = 0; taken_names[i] != NULL; i++) {
        str_set_add(&view->names, taken_names[i]);
      }
      view->template_var = printer_free_name(printer, printer_concat(printer, machine->name, "View"));
      find_slots(printer, view, node->view);
      find_bound_attributes(printer, node->view);
      machine->view = view;
    }
    prepare_parts(printer, machine, machine->states);
  }
}

static bool any_view(Printer* printer) {
  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    if(machine->view != NULL) {
      return true;
    }
  }
  return false;
}

void printer_print_view_helper(Printer* printer) {
  if(!any_view(printer)) {
    return;
  }
  JSBuilder* jsb = printer->jsb;
  js_builder_line(jsb, "// Markup, parsed once into a fragment for the machines to clone.");
  js_builder_open(jsb, "function %s(html%s) {", printer->template, printer->ts ? ": string" : "");
  js_builder_line(jsb, "const element = document.createElement('template');");
  js_builder_line(jsb, "element.innerHTML = html;");
  js_builder_line(jsb, "return element.content;");
  js_builder_close(jsb, "}");
  js_builder_blank(jsb);
  if(printer->needs_attr) {
    js_builder_line(jsb, "// An attribute bound to a key: there for true and other values, as");
    js_builder_line(jsb, "// written, and gone for false, null and undefined.");
    js_builder_open(jsb, "function %s(element%s, name%s, value%s) {", printer->attr,
      printer->ts ? ": Element" : "", printer->ts ? ": string" : "", printer->ts ? ": unknown" : "");
    js_builder_open(jsb, "if(value == null || value === false) {");
    js_builder_line(jsb, "element.removeAttribute(name);");
    js_builder_close(jsb, "} else {");
    jsb->depth++;
    js_builder_line(jsb, "element.setAttribute(name, value === true ? '' : String(value));");
    js_builder_close(jsb, "}");
    js_builder_close(jsb, "}");
    js_builder_blank(jsb);
  }
}

static void part_templates(Printer* printer, JSState* state) {
  for(; state != NULL; state = state->next) {
    if(state->part_var != NULL) {
      js_builder_line(printer->jsb, "let %s;", state->part_template);
    }
    part_templates(printer, state->child);
  }
}

void printer_print_view_templates(Printer* printer, JSMachine* machine) {
  if(machine->view == NULL) {
    return;
  }
  js_builder_line(printer->jsb, "let %s;", machine->view->template_var);
  part_templates(printer, machine->states);
  js_builder_blank(printer->jsb);
}

// The markup of nodes, for the template: as written, but with no event
// attributes, and <!> where a key, a list or a part goes.
static void add_markup(StrBuilder* sb, ViewNode* node) {
  for(; node != NULL; node = node->next) {
    switch(node->type) {
      case VIEW_ELEMENT: {
        str_builder_add_format(sb, "<%s", node->name);
        for(ViewAttribute* attribute = node->attributes; attribute != NULL; attribute = attribute->next) {
          if(attribute->event != NULL || attribute->key != NULL) {
            continue;
          }
          str_builder_add_format(sb, " %s", attribute->name);
          if(attribute->value != NULL) {
            str_builder_add_format(sb, "=%s", attribute->value);
          }
        }
        str_builder_add_str(sb, ">");
        static const char* const void_elements[] = {
          "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr", NULL
        };
        bool is_void = false;
        for(size_t i = 0; void_elements[i] != NULL; i++) {
          if(strcmp(void_elements[i], node->name) == 0) {
            is_void = true;
          }
        }
        if(!is_void) {
          add_markup(sb, node->child);
          str_builder_add_format(sb, "</%s>", node->name);
        }
        break;
      }
      case VIEW_TEXT: {
        str_builder_add_str(sb, node->text);
        break;
      }
      case VIEW_VALUE:
      case VIEW_PART: {
        str_builder_add_str(sb, "<!>");
        break;
      }
    }
  }
}

// The template's markup as a JavaScript string, in single quotes.
static const char* markup_string(Printer* printer, ViewNode* nodes) {
  StrBuilder* markup = str_builder_create();
  add_markup(markup, nodes);
  StrBuilder* sb = str_builder_create();
  str_builder_add_char(sb, '\'');
  for(const char* c = str_builder_peek(markup); *c != '\0'; c++) {
    if(*c == '\\' || *c == '\'') {
      str_builder_add_char(sb, '\\');
    }
    str_builder_add_char(sb, *c);
  }
  str_builder_add_char(sb, '\'');
  const char* result = arena_strdup(printer->arena, str_builder_peek(sb));
  str_builder_destroy(markup);
  str_builder_destroy(sb);
  return result;
}

// What a walk of a view is for: the factory's own view, or a state's part,
// made later, when the context is the machine's.
typedef struct Walk {
  Printer* printer;
  JSMachine* machine;
  JSView* view;
  bool in_part;
  // The keys the machine assigns, whose text changes.
  StrSet* assigned;
  // The keys it appends to, which are lists of machines.
  StrSet* lists;
} Walk;

// Whether the element listens, or has an attribute bound to a key.
static bool has_event(ViewNode* node) {
  for(ViewAttribute* attribute = node->attributes; attribute != NULL; attribute = attribute->next) {
    if(attribute->event != NULL || attribute->key != NULL) {
      return true;
    }
  }
  return false;
}

// Whether the output has to find node: it changes or listens, or is on the
// way to something that does.
static bool needs_var(ViewNode* node) {
  if(node->type == VIEW_VALUE || node->type == VIEW_PART) {
    return true;
  }
  if(node->type != VIEW_ELEMENT) {
    return false;
  }
  if(has_event(node)) {
    return true;
  }
  for(ViewNode* child = node->child; child != NULL; child = child->next) {
    if(needs_var(child)) {
      return true;
    }
  }
  return false;
}

static const char* node_var_base(Walk* walk, ViewNode* node) {
  Printer* printer = walk->printer;
  switch(node->type) {
    case VIEW_ELEMENT:
      return tag_name(printer, node->name);
    case VIEW_VALUE:
      return printer_concat(printer, node->name, str_set_contains(walk->lists, node->name) ? "End" : "Marker");
    case VIEW_PART:
      return printer_concat(printer, node->name, "Slot");
    case VIEW_TEXT:
      break;
  }
  return "text";
}

// The consts that find the nodes the output needs, under parent: each the
// node before it, or its parent, and a step.
static void print_walk(Walk* walk, const char* parent, ViewNode* children) {
  JSBuilder* jsb = walk->printer->jsb;
  ViewNode* last = NULL;
  for(ViewNode* child = children; child != NULL; child = child->next) {
    if(needs_var(child)) {
      last = child;
    }
  }
  if(last == NULL) {
    return;
  }
  const char* path = printer_concat(walk->printer, parent, ".firstChild");
  for(ViewNode* child = children; child != NULL; child = child->next) {
    if(needs_var(child)) {
      if(child->var != NULL) {
        // Declared already, for an assign to reach it.
        js_builder_line(jsb, "%s = %s;", child->var, path);
      } else {
        child->var = view_name(walk->printer, walk->view, node_var_base(walk, child));
        js_builder_line(jsb, "const %s = %s;", child->var, path);
      }
      path = child->var;
    }
    if(child == last) {
      break;
    }
    path = printer_concat(walk->printer, path, ".nextSibling");
  }
  for(ViewNode* child = children; child != NULL; child = child->next) {
    if(child->type == VIEW_ELEMENT && child->var != NULL) {
      print_walk(walk, child->var, child->child);
    }
    if(child == last) {
      break;
    }
  }
}

// Note that var, a text node or, with attribute, an element, shows key,
// so that an assign of the key changes it.
static void add_binding(Walk* walk, const char* key, const char* var, const char* attribute) {
  JSViewText* entry = ARENA_NEW(walk->printer->arena, JSViewText);
  entry->key = key;
  entry->var = var;
  entry->attribute = attribute;
  entry->in_part = walk->in_part;
  // In the order they are in the view, which is the order they change in.
  JSViewText** tail = &walk->view->texts;
  while(*tail != NULL) {
    tail = &(*tail)->next;
  }
  *tail = entry;
}

// Name the text node that shows key, which an assign of it changes.
static const char* add_text(Walk* walk, const char* key) {
  const char* text = view_name(walk->printer, walk->view, printer_concat(walk->printer, key, "Text"));
  add_binding(walk, key, text, NULL);
  return text;
}

// Whether an element of a part has an attribute bound to a key the
// machine assigns, so that its variable has to be the factory's.
static bool has_assigned_attribute(Walk* walk, ViewNode* node) {
  for(ViewAttribute* attribute = node->attributes; attribute != NULL; attribute = attribute->next) {
    if(attribute->key != NULL && str_set_contains(walk->assigned, attribute->key)) {
      return true;
    }
  }
  return false;
}

// A part's text nodes that show assigned keys are the factory's variables,
// so an assign can reach them, declared before the part is made. Each one's
// name is kept in its node's text.
static void declare_part_texts(Walk* walk, ViewNode* node) {
  for(; node != NULL; node = node->next) {
    if(node->type == VIEW_VALUE && !str_set_contains(walk->lists, node->name) &&
       str_set_contains(walk->assigned, node->name)) {
      node->text = add_text(walk, node->name);
      js_builder_line(walk->printer->jsb, "let %s;", node->text);
    } else if(node->type == VIEW_ELEMENT && has_assigned_attribute(walk, node)) {
      node->var = view_name(walk->printer, walk->view, tag_name(walk->printer, node->name));
      js_builder_line(walk->printer->jsb, "let %s;", node->var);
    }
    declare_part_texts(walk, node->child);
  }
}

// The context a key's first value comes from: the factory's option, or,
// in a part made later, the machine's.
static const char* context_of(Walk* walk) {
  return walk->in_part ? "machine.context" : "context";
}

// Set the attribute of element bound to a key to value, a JavaScript
// expression, on the current line.
static void print_set(Printer* printer, const char* element, const char* attribute, const char* value) {
  JSBuilder* jsb = printer->jsb;
  if(strcmp(attribute, "value") == 0) {
    js_builder_line(jsb, "%s.value = %s ?? '';", element, value);
  } else if(is_property(attribute)) {
    js_builder_line(jsb, "%s.%s = !!%s;", element, attribute, value);
  } else {
    js_builder_line(jsb, "%s(%s, '%s', %s);", printer->attr, element, attribute, value);
  }
}

// What follows the walk: each key's text put where its marker is, the
// listeners, lists filled and parts' places noted.
static void print_wiring(Walk* walk, ViewNode* node) {
  Printer* printer = walk->printer;
  JSBuilder* jsb = printer->jsb;
  for(; node != NULL; node = node->next) {
    switch(node->type) {
      case VIEW_ELEMENT: {
        for(ViewAttribute* attribute = node->attributes; attribute != NULL; attribute = attribute->next) {
          if(attribute->event != NULL) {
            js_builder_line(jsb, "%s.addEventListener('%s', data => machine.send({ type: '%s', data }));",
              node->var, attribute->dom_event, attribute->event);
          } else if(attribute->key != NULL) {
            if(str_set_contains(walk->assigned, attribute->key)) {
              add_binding(walk, attribute->key, node->var, attribute->name);
            }
            print_set(printer, node->var, attribute->name,
              printer_concat(printer, printer_concat(printer, context_of(walk), "."), attribute->key));
          }
        }
        print_wiring(walk, node->child);
        break;
      }
      case VIEW_VALUE: {
        const char* key = node->name;
        if(str_set_contains(walk->lists, key)) {
          if(walk->in_part) {
            printer_warnf(printer, LUCY_STREAM_STDOUT, "A part of a view can't show the list [%s] yet.\n", key);
            break;
          }
          str_map_set(walk->view->lists, key, node);
          js_builder_open(jsb, "for(const %s of %s.%s ?? []) {", printer->actor, context_of(walk), key);
          js_builder_line(jsb, "%s.before(...%s.%s);", node->var, printer->actor, printer->nodes);
          js_builder_close(jsb, "}");
        } else if(str_set_contains(walk->assigned, key)) {
          // Assigned, so its text node is kept, to change. A part's was
          // declared, and named, before the function that makes the part.
          const char* text = walk->in_part ? node->text : add_text(walk, key);
          js_builder_line(jsb, "%s%s = document.createTextNode(%s.%s ?? '');", walk->in_part ? "" : "const ", text, context_of(walk), key);
          js_builder_line(jsb, "%s.replaceWith(%s);", node->var, text);
        } else {
          // Nothing assigns it, so its text is put in once.
          js_builder_line(jsb, "%s.replaceWith(%s.%s ?? '');", node->var, context_of(walk), key);
        }
        break;
      }
      case VIEW_PART: {
        if(walk->in_part) {
          printer_warnf(printer, LUCY_STREAM_STDOUT, "A part of a view can't have parts of its own.\n");
          break;
        }
        break;
      }
      case VIEW_TEXT:
        break;
    }
  }
}

// Collect the keys a machine's actions assign, and the ones they append to.
static void collect_actions(Printer* printer, TransitionAction* action, StrSet* lists) {
  for(; action != NULL; action = action->next) {
    AssignExpression* assign = printer_action_spawn(printer, action);
    if(assign != NULL && assign->append) {
      str_set_add(lists, assign->key);
    }
  }
}

static void collect_transitions(Printer* printer, TransitionNode* transition, StrSet* lists) {
  for(; transition != NULL; transition = transition->next) {
    collect_actions(printer, transition->action, lists);
  }
}

static void collect_lists(Printer* printer, JSState* state, StrSet* lists) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      collect_actions(printer, node->entry->action, lists);
    }
    if(node->exit != NULL) {
      collect_actions(printer, node->exit->action, lists);
    }
    collect_transitions(printer, node->event_transition, lists);
    collect_transitions(printer, node->immediate_transition, lists);
    collect_transitions(printer, node->delay_transition, lists);
    if(node->invoke != NULL) {
      collect_transitions(printer, node->invoke->event_transition, lists);
    }
    collect_lists(printer, state->child, lists);
  }
}

// A state's part: the let for its nodes, and the function that makes them.
static void print_part_makers(Walk* walk, JSState* state) {
  Printer* printer = walk->printer;
  JSBuilder* jsb = printer->jsb;
  for(; state != NULL; state = state->next) {
    if(state->part_var != NULL) {
      ViewNode* nodes = state->node->part;
      walk->in_part = true;
      declare_part_texts(walk, nodes);
      js_builder_line(jsb, "let %s;", state->part_var);
      js_builder_open(jsb, "function %s() {", state->part_make);
      js_builder_line(jsb, "%s ?\?= %s(%s);", state->part_template, printer->template, markup_string(printer, nodes));
      js_builder_line(jsb, "const part = %s.cloneNode(true);", state->part_template);
      print_walk(walk, "part", nodes);
      print_wiring(walk, nodes);
      js_builder_line(jsb, "return [...part.childNodes];");
      js_builder_close(jsb, "}");
      walk->in_part = false;
    }
    print_part_makers(walk, state->child);
  }
}

void printer_print_view_setup(Printer* printer, JSMachine* machine) {
  JSView* view = machine->view;
  if(view == NULL) {
    return;
  }
  JSBuilder* jsb = printer->jsb;
  StrSet lists;
  str_set_init(&lists, printer->arena);
  collect_lists(printer, machine->states, &lists);
  Walk walk = {
    .printer = printer,
    .machine = machine,
    .view = view,
    .assigned = &machine->interface->key_set,
    .lists = &lists,
  };
  ViewNode* nodes = machine->node->view;

  js_builder_line(jsb, "%s ?\?= %s(%s);", view->template_var, printer->template, markup_string(printer, nodes));
  js_builder_line(jsb, "const %s = %s.cloneNode(true);", printer->dom, view->template_var);
  js_builder_line(jsb, "const %s = [...%s.childNodes];", printer->nodes, printer->dom);
  print_walk(&walk, printer->dom, nodes);
  print_wiring(&walk, nodes);

  print_part_makers(&walk, machine->states);
  js_builder_blank(jsb);
}

void printer_print_view_assign(Printer* printer, const char* key, bool append) {
  JSView* view = printer->machine != NULL ? printer->machine->view : NULL;
  if(view == NULL || key == NULL) {
    return;
  }
  JSBuilder* jsb = printer->jsb;
  if(append) {
    ViewNode* end = str_map_get(view->lists, key);
    if(end != NULL) {
      // One machine was added, at the end, so its view goes at the end.
      js_builder_line(jsb, "%s.before(...machine.context.%s.at(-1).%s);", end->var, key, printer->nodes);
    }
    return;
  }
  // The bindings of a part are checked for, as the part may not be made
  // yet: once for each node that has them.
  const char* guard = NULL;
  for(JSViewText* text = view->texts; text != NULL; text = text->next) {
    if(strcmp(text->key, key) != 0) {
      continue;
    }
    const char* needs = text->in_part ? text->var : NULL;
    if(guard != NULL && (needs == NULL || strcmp(needs, guard) != 0)) {
      js_builder_close(jsb, "}");
      guard = NULL;
    }
    if(needs != NULL && guard == NULL) {
      js_builder_open(jsb, "if(%s) {", needs);
      guard = needs;
    }
    if(text->attribute != NULL) {
      print_set(printer, text->var, text->attribute, printer_concat(printer, "machine.context.", key));
    } else {
      js_builder_line(jsb, "%s.data = machine.context.%s ?? '';", text->var, key);
    }
  }
  if(guard != NULL) {
    js_builder_close(jsb, "}");
  }
}

bool printer_view_has_list(Printer* printer, const char* key) {
  JSView* view = printer->machine != NULL ? printer->machine->view : NULL;
  return view != NULL && str_map_get(view->lists, key) != NULL;
}

void printer_print_part_enter(Printer* printer, JSState* state) {
  if(state->part_var == NULL) {
    return;
  }
  ViewNode* slot = str_map_get(state->machine->view->slots, state->node->part_name);
  js_builder_line(printer->jsb, "%s ?\?= %s();", state->part_var, state->part_make);
  js_builder_line(printer->jsb, "%s.before(...%s);", slot->var, state->part_var);
}

void printer_print_part_exit(Printer* printer, JSState* state) {
  if(state->part_var == NULL) {
    return;
  }
  JSBuilder* jsb = printer->jsb;
  js_builder_open(jsb, "for(const node of %s) {", state->part_var);
  js_builder_line(jsb, "node.remove();");
  js_builder_close(jsb, "}");
}
