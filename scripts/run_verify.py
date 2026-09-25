import os
import subprocess
import sys

os.environ["PATH"] = r"C:\Qt\6.8.2\msvc2022_64\bin;" + os.environ["PATH"]
cmd = [r".\build\windows\x64\release\weasel-fluent-cpp-qt.exe", "--verify"]
res = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
print("RETURNCODE:", res.returncode)
print("STDOUT:\n" + res.stdout)
if res.stderr:
    print("STDERR:\n" + res.stderr)
