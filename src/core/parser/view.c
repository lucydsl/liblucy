#include "internal.h"

#include <stdbool.h>
#include <string.h>

/*
 * Views: markup the compiler keeps in step with the machine.
 *
 *   view { <span>Name: {name}</span> <input on:input={input}> }
 *
 * in a machine's block is its view, and
 *
 *   view(status) { <p>Saving…</p> }
 *
 * in a state is a part of it, there only while the machine is in that
 * state, which goes where the view has {part(status)}.
 *
 * Markup is read a character at a time, not in tokens: elements, their
 * attributes, text, and {...}, which is a context key or a part. The view
 * ends at the } that closes its block.
 */

// The elements HTML gives no closing tag.
static const char* const void_elements[] = {
  "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr", NULL
};

static bool is_void(const char* tag) {
  for(size_t i = 0; void_elements[i] != NULL; i++) {
    if(strcmp(void_elements[i], tag) == 0) {
      return true;
    }
  }
  return false;
}

// The next character, which is not read yet.
static char peek(const Parser* parser) {
  return parser_peek(parser);
}

// Read the next character, keeping the line and column right.
static char take(Parser* parser) {
  char c = parser_next(parser);
  if(c == '\n') {
    parser_advance_line(parser);
  } else {
    parser_advance_column(parser);
  }
  return c;
}

static bool at_end(const Parser* parser) {
  return parser->index + 1 >= parser->source_len;
}

static void skip_space(Parser* parser) {
  while(!at_end(parser) && (peek(parser) == ' ' || peek(parser) == '\n' || peek(parser) == '\t' || peek(parser) == '\r')) {
    take(parser);
  }
}

static bool is_name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ':' || c == '.';
}

static bool is_identifier_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// The run of characters that pred accepts, from the next one, as an arena
// string. Empty if there are none.
static const char* take_while(Parser* parser, bool (*pred)(char)) {
  size_t start = parser->index + 1;
  while(!at_end(parser) && pred(peek(parser))) {
    take(parser);
  }
  return arena_strndup(parser->arena, parser->source + start, parser->index + 1 - start);
}

static ViewNode* new_node(Parser* parser, ViewNodeType type) {
  ViewNode* node = ARENA_NEW(parser->arena, ViewNode);
  node->type = type;
  return node;
}

// The text between tags, as JSX keeps it: whitespace at the start and end
// of each line goes, as do lines with nothing else on them, and the lines
// that are left are joined with a space. Whitespace within a line stays,
// so `Name: {name}` keeps its space. NULL if nothing is left.
static bool is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\r';
}

// The part of the line at raw[start, end) that is kept: trimmed at its
// start unless it is the first line, and at its end unless it is the last.
static void trim_line(const char* raw, size_t* start, size_t* end, bool first, bool last) {
  if(!first) {
    while(*start < *end && is_blank(raw[*start])) {
      (*start)++;
    }
  }
  if(!last) {
    while(*end > *start && is_blank(raw[*end - 1])) {
      (*end)--;
    }
  }
}

static const char* clean_text(Parser* parser, const char* raw, size_t length) {
  StrBuilder* text = str_builder_create();
  bool pending_space = false;
  size_t start = 0;
  for(size_t i = 0; i <= length; i++) {
    if(i < length && raw[i] != '\n') {
      continue;
    }
    size_t line_start = start;
    size_t line_end = i;
    trim_line(raw, &line_start, &line_end, start == 0, i == length);
    if(line_end > line_start) {
      if(pending_space) {
        str_builder_add_char(text, ' ');
      }
      for(size_t j = line_start; j < line_end; j++) {
        str_builder_add_char(text, raw[j]);
      }
      pending_space = true;
    }
    start = i + 1;
  }
  const char* result = str_builder_peek(text)[0] != '\0' ? arena_strdup(parser->arena, str_builder_peek(text)) : NULL;
  str_builder_destroy(text);
  return result;
}

// {key} or {part(name)}, from after the {.
static ParseStatus consume_value(Parser* parser, ViewNode** out) {
  skip_space(parser);
  const char* name = take_while(parser, is_identifier_char);
  if(name[0] == '\0') {
    parser_error(parser, "Expected a key of the context, as in {name}, or a part, as in {part(name)}.");
    return PARSE_FATAL;
  }
  ViewNode* node;
  if(strcmp(name, "part") == 0 && peek(parser) == '(') {
    take(parser);
    skip_space(parser);
    const char* part = take_while(parser, is_identifier_char);
    skip_space(parser);
    if(part[0] == '\0' || peek(parser) != ')') {
      parser_error(parser, "A part is named in parentheses, as in {part(status)}.");
      return PARSE_FATAL;
    }
    take(parser);
    node = new_node(parser, VIEW_PART);
    node->name = part;
  } else {
    node = new_node(parser, VIEW_VALUE);
    node->name = name;
  }
  skip_space(parser);
  if(peek(parser) != '}') {
    parser_error(parser, "Expected a } here. A view shows a key of the context as it is, as in {name}: there are no expressions.");
    return PARSE_FATAL;
  }
  take(parser);
  *out = node;
  return PARSE_OK;
}

// The attributes of an element, up to and including its > or />.
// *self_closing says which.
static ParseStatus consume_attributes(Parser* parser, ViewNode* element, bool* self_closing) {
  ViewAttribute** tail = &element->attributes;
  for(;;) {
    skip_space(parser);
    if(at_end(parser)) {
      parser_error_backf(parser, 0, "Expected a > to end the <%s> tag.", element->name);
      return PARSE_FATAL;
    }
    char c = peek(parser);
    if(c == '>') {
      take(parser);
      *self_closing = false;
      return PARSE_OK;
    }
    if(c == '/') {
      take(parser);
      if(peek(parser) != '>') {
        parser_error(parser, "Expected a > after the /.");
        return PARSE_FATAL;
      }
      take(parser);
      *self_closing = true;
      return PARSE_OK;
    }
    const char* name = take_while(parser, is_name_char);
    if(name[0] == '\0') {
      parser_error_backf(parser, 0, "Expected the name of an attribute of <%s>.", element->name);
      return PARSE_FATAL;
    }
    ViewAttribute* attribute = ARENA_NEW(parser->arena, ViewAttribute);
    attribute->name = name;
    bool event = strncmp(name, "on:", 3) == 0;
    if(event) {
      attribute->dom_event = name + 3;
      if(attribute->dom_event[0] == '\0') {
        parser_error(parser, "An event attribute names the DOM event after on:, as in on:click={remove}.");
        return PARSE_FATAL;
      }
    }
    skip_space(parser);
    if(peek(parser) == '=') {
      take(parser);
      skip_space(parser);
      char quote = peek(parser);
      if(quote == '{') {
        take(parser);
        skip_space(parser);
        const char* value = take_while(parser, is_identifier_char);
        skip_space(parser);
        if(value[0] == '\0' || peek(parser) != '}') {
          parser_error(parser, event ? "An event attribute sends a Lucy event, as in on:click={remove}." : "An attribute shows a key of the context as it is, as in value={name}: there are no expressions.");
          return PARSE_FATAL;
        }
        take(parser);
        if(event) {
          attribute->event = value;
        } else {
          attribute->key = value;
        }
      } else if(quote == '"' || quote == '\'') {
        if(event) {
          parser_error(parser, "An event attribute sends a Lucy event, named in braces: on:click={remove}.");
          return PARSE_FATAL;
        }
        size_t start = parser->index + 1;
        take(parser);
        while(!at_end(parser) && peek(parser) != quote) {
          take(parser);
        }
        if(at_end(parser)) {
          parser_error(parser, "This attribute's value has no closing quote.");
          return PARSE_FATAL;
        }
        take(parser);
        attribute->value = arena_strndup(parser->arena, parser->source + start, parser->index + 1 - start);
      } else {
        parser_error_backf(parser, 0, "Expected a quoted value for the attribute %s.", name);
        return PARSE_FATAL;
      }
    } else if(event) {
      parser_error(parser, "An event attribute sends a Lucy event, as in on:click={remove}.");
      return PARSE_FATAL;
    }
    *tail = attribute;
    tail = &attribute->next;
  }
}

static ParseStatus consume_nodes(Parser* parser, const char* closing, ViewNode** out);

// <!-- ... -->, from after the <!--.
static ParseStatus skip_comment(Parser* parser) {
  while(!at_end(parser)) {
    char c = take(parser);
    if(c == '-' && peek(parser) == '-') {
      take(parser);
      if(peek(parser) == '>') {
        take(parser);
        return PARSE_OK;
      }
    }
  }
  parser_error(parser, "This comment has no -->.");
  return PARSE_FATAL;
}

// An element, from after its <.
static ParseStatus consume_element(Parser* parser, ViewNode** out) {
  const char* tag = take_while(parser, is_name_char);
  if(tag[0] == '\0') {
    parser_error(parser, "Expected the name of an element after the <.");
    return PARSE_FATAL;
  }
  ViewNode* element = new_node(parser, VIEW_ELEMENT);
  element->name = tag;
  bool self_closing = false;
  if(consume_attributes(parser, element, &self_closing) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  if(!self_closing && !is_void(tag)) {
    if(consume_nodes(parser, tag, &element->child) == PARSE_FATAL) {
      return PARSE_FATAL;
    }
  }
  *out = element;
  return PARSE_OK;
}

// The nodes up to </closing>, which is read too, or, for closing NULL, up
// to the } that ends the view, which is read too.
static ParseStatus consume_nodes(Parser* parser, const char* closing, ViewNode** out) {
  ViewNode** tail = out;
  for(;;) {
    if(at_end(parser)) {
      if(closing != NULL) {
        parser_error_backf(parser, 0, "Expected a </%s> to close the <%s>.", closing, closing);
      } else {
        parser_error(parser, "Expected a } to end the view.");
      }
      return PARSE_FATAL;
    }
    char c = peek(parser);
    ViewNode* node = NULL;
    if(c == '}') {
      if(closing != NULL) {
        parser_error_backf(parser, 0, "Expected a </%s> to close the <%s> before the end of the view.", closing, closing);
        return PARSE_FATAL;
      }
      take(parser);
      return PARSE_OK;
    } else if(c == '<') {
      take(parser);
      if(peek(parser) == '/') {
        take(parser);
        const char* tag = take_while(parser, is_name_char);
        skip_space(parser);
        if(peek(parser) != '>') {
          parser_error(parser, "Expected a > to end the closing tag.");
          return PARSE_FATAL;
        }
        take(parser);
        if(closing == NULL || strcmp(tag, closing) != 0) {
          if(closing == NULL) {
            parser_error_backf(parser, 0, "</%s> closes an element that isn't open.", tag);
          } else {
            parser_error_backf(parser, 0, "</%s> where </%s> was expected.", tag, closing);
          }
          return PARSE_FATAL;
        }
        return PARSE_OK;
      }
      if(peek(parser) == '!') {
        take(parser);
        if(take(parser) != '-' || take(parser) != '-') {
          parser_error(parser, "Expected a comment, <!-- like this -->.");
          return PARSE_FATAL;
        }
        if(skip_comment(parser) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        continue;
      }
      if(consume_element(parser, &node) == PARSE_FATAL) {
        return PARSE_FATAL;
      }
    } else if(c == '{') {
      take(parser);
      if(consume_value(parser, &node) == PARSE_FATAL) {
        return PARSE_FATAL;
      }
    } else {
      size_t start = parser->index + 1;
      while(!at_end(parser) && peek(parser) != '<' && peek(parser) != '{' && peek(parser) != '}') {
        take(parser);
      }
      const char* text = clean_text(parser, parser->source + start, parser->index + 1 - start);
      if(text == NULL) {
        continue;
      }
      node = new_node(parser, VIEW_TEXT);
      node->text = text;
    }
    *tail = node;
    tail = &node->next;
  }
}

// The { that starts a view's block, and the view up to its }.
static ParseStatus consume_block(Parser* parser, ViewNode** out) {
  skip_space(parser);
  if(peek(parser) != '{') {
    parser_error(parser, "Expected a { to start the view.");
    return PARSE_FATAL;
  }
  take(parser);
  return consume_nodes(parser, NULL, out);
}

ParseStatus parser_consume_view(Parser* parser) {
  Node* node = parser->node;
  if(node == NULL || node->type != NODE_MACHINE || !((MachineNode*)node)->has_name) {
    parser_error(parser, "A view goes inside a machine { } block.");
    return PARSE_FATAL;
  }
  if(parser->machine_depth > 1) {
    parser_error(parser, "A view goes in the outermost machine, not in a machine nested in a state.");
    return PARSE_FATAL;
  }
  MachineNode* machine = (MachineNode*)node;
  if(machine->has_view) {
    parser_error(parser, "This machine already has a view.");
    return PARSE_FATAL;
  }
  machine->has_view = true;
  return consume_block(parser, &machine->view);
}

ParseStatus parser_consume_part(Parser* parser) {
  Node* node = parser->node;
  if(node == NULL || node->type != NODE_STATE) {
    parser_error(parser, "A part of a view goes in a state.");
    return PARSE_FATAL;
  }
  StateNode* state = (StateNode*)node;
  // Over the (.
  take(parser);
  skip_space(parser);
  const char* name = take_while(parser, is_identifier_char);
  skip_space(parser);
  if(name[0] == '\0' || peek(parser) != ')') {
    parser_error(parser, "A part of a view is named in parentheses, as in view(status) { ... }.");
    return PARSE_FATAL;
  }
  take(parser);
  if(state->part != NULL || state->part_name != NULL) {
    parser_error(parser, "This state already has a part of the view.");
    return PARSE_FATAL;
  }
  state->part_name = name;
  return consume_block(parser, &state->part);
}
