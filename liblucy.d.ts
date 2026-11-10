declare namespace LibLucy {
  interface CompileOptions {
    /** Also produce TypeScript declarations (.d.ts) for the module. Experimental. */
    dts?: boolean;
  }

  interface CompileResult {
    /** The compiled JavaScript module. */
    js: string;
    /** The module's TypeScript declarations, when the `dts` option is set. */
    dts?: string;
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
  export function compile(source: string, filename: string, options?: CompileOptions): CompileResult;
}

export = LibLucy;
