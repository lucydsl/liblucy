declare namespace LibLucy {
  interface CompileXStateOptions {
    /** Import XState from https://cdn.skypack.dev/xstate instead of 'xstate'. */
    useRemote?: boolean;
    /** Also produce TypeScript declarations (.d.ts) for the module. Experimental. */
    dts?: boolean;
  }

  interface CompileXStateResult {
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

  /** What compileXstate throws when the source doesn't compile. */
  interface CompileError extends Error {
    message: 'Compiler error';
    /** The errors and warnings, in the order the compiler reported them. */
    diagnostics: Diagnostic[];
  }

  /** Resolves when the module is loaded and compileXstate can be called. */
  export const ready: Promise<void>;

  /**
   * Compile Lucy source to a JavaScript module of XState machines.
   *
   * Throws a CompileError if the source doesn't compile, after printing
   * the errors to the console.
   */
  export function compileXstate(source: string, filename: string, options?: CompileXStateOptions): CompileXStateResult;
}

export = LibLucy;
