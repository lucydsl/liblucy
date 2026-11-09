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

  /** Resolves when the module is loaded and compileXstate can be called. */
  export const ready: Promise<void>;

  /**
   * Compile Lucy source to a JavaScript module of XState machines.
   *
   * Throws an Error with the message 'Compiler error' if the source doesn't
   * compile, after printing the errors to the console.
   */
  export function compileXstate(source: string, filename: string, options?: CompileXStateOptions): CompileXStateResult;
}

export = LibLucy;
