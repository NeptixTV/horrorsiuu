/** Opens the system file dialog. */
export function pickFile(accept: string, onFile: (f: File) => void, multiple = false) {
  pickFiles(accept, (files) => files.forEach(onFile), multiple);
}

/** Like pickFile, but hands over all chosen files at once. */
export function pickFiles(accept: string, onFiles: (files: File[]) => void, multiple = true) {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = accept;
  input.multiple = multiple;
  input.onchange = () => onFiles(Array.from(input.files ?? []));
  input.click();
}

/** Lets the user choose a whole folder (e.g. a sample pack) – every file inside, sub folders included. */
export function pickFolder(onFiles: (files: File[]) => void) {
  const input = document.createElement('input');
  input.type = 'file';
  input.multiple = true;
  (input as HTMLInputElement & { webkitdirectory: boolean }).webkitdirectory = true;
  input.onchange = () => onFiles(Array.from(input.files ?? []));
  input.click();
}
