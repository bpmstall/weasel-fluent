import pefile
import os

dll_path = r"C:\Program Files\Rime\weasel-0.17.4\rime.dll"
pe = pefile.PE(dll_path)
exports = []
if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if exp.name:
            name = exp.name.decode('utf-8')
            exports.append(name)

with open(r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\scripts\rime_exports.txt", "w", encoding="utf-8") as f:
    for name in exports:
        f.write(name + "\n")
print(f"Total exports: {len(exports)}")
