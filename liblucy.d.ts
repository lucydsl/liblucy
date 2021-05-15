declare namespace LibLucy {
  interface CompileXStateOptions {
    useRemote: boolean;
  }

  export function compileXstate(source: string, filename: string, options?: CompileXStateOptions): string;
}

export = LibLucy;
