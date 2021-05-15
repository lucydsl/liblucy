declare namespace LibLucy {
  interface CompileXStateOptions {
    useRemote: boolean;
  }

  export const ready: Promise<void>;

  export function compileXstate(source: string, filename: string, options?: CompileXStateOptions): string;
}

export = LibLucy;
