/** Opens the native file picker. */
export function pickFile(accept: string, onFile: (f: File) => void, multiple = false) {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = accept;
  input.multiple = multiple;
  input.onchange = () => {
    for (const f of Array.from(input.files ?? [])) onFile(f);
  };
  input.click();
}
