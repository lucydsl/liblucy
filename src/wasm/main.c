// liblucy keeps no global state, so there is nothing to set up. main
// still exists because Emscripten runs it when the module is created and
// it is part of the exported API.
int main(void) {
  return 0;
}
