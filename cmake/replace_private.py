#!/usr/bin/env python3
import sys
from pathlib import Path

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <file_to_patch>")
    sys.exit(1)

file_path = Path(sys.argv[1])
if not file_path.is_file():
    print(f"File not found: {file_path}")
    sys.exit(1)

with file_path.open("r", encoding="utf-8") as f:
    content = f.read()

new_content = content.replace("private:", "protected:")

with file_path.open("w", encoding="utf-8") as f:
    f.write(new_content)

print(f"Replaced 'private:' with 'protected:' in {file_path}")
