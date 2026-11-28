declare namespace LibLucy {
  interface CompileOptions {
    /**
     * What to compile to: 'js', the default, for a JavaScript module and
     * its declarations, or 'ts' for the two as one TypeScript module.
     */
    format?: 'js' | 'ts';
    /**
     * Where views run: 'dom', the default, for the browser, where a
     * machine's view is machine.dom; or 'ssr' for a server, where it is
     * the string machine.render() returns. Machines without a view are the
     * same for both.
     */
    target?: 'dom' | 'ssr';
    /**
     * Compile for hydration, on both targets: with 'ssr', render() marks
     * where each key's text, list and part is; with 'dom', a machine with
     * a view takes over those nodes, given as its hydrate option, rather
     * than making its own. A machine the module spawns makes its own.
     */
    hydrate?: boolean;
    /**
     * @deprecated The declarations are always produced. This does nothing.
     */
    dts?: boolean;
  }

  interface CompileResult {
    /** The compiled JavaScript module. */
    js: string;
    /** The module's TypeScript declarations (.d.ts). */
    dts: string;
  }

  interface TypeScriptCompileResult {
    /**
     * The compiled TypeScript module: the JavaScript with its types in it.
     * Its syntax is erasable, so Node can run it as it is.
     */
    ts: string;
  }

  /** An error or warning from the compiler. */
  interface Diagnostic {
    severity: 'error' | 'warning';
    /** The message alone, such as 'Unexpected identifier'. */
    message: string;
    /** The line (counting from 1) of an error. 0 for warnings. */
    line: number;
    /** The column of an error. 0 for warnings. */
    column: number;
    /** The diagnostic as lucyc prints it, with ANSI colours. */
    text: string;
  }

  /** What compile throws when the source doesn't compile. */
  interface CompileError extends Error {
    message: 'Compiler error';
    /** The errors and warnings, in the order the compiler reported them. */
    diagnostics: Diagnostic[];
  }

  /** Resolves when dist/lucy.wasm is loaded and compile can be called. */
  export const ready: Promise<void>;

  /**
   * Compile Lucy source to a JavaScript module of plain state machines: one
   * exported factory per machine, which returns a machine with `state`,
   * `context`, `send`, `subscribe`, `start` and `stop`.
   *
   * Prints the compile's errors and warnings to the console as lucyc does:
   * errors with console.error, warnings with console.log, one call each.
   * Throws a CompileError if the source doesn't compile, after printing
   * them.
   */
  export function compile(source: string, filename: string, options?: CompileOptions & { format?: 'js' }): CompileResult;
  /** The same, to TypeScript. */
  export function compile(source: string, filename: string, options: CompileOptions & { format: 'ts' }): TypeScriptCompileResult;
}

export = LibLucy;
