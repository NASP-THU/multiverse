import sys

if len(sys.argv) != 2:
    print("Usage: count_flow.py <filename>")
    sys.exit(1)

pattern = "Flow is finished: flow sender id:"
count = 0

with open(sys.argv[1], "r", encoding="utf-8", errors="ignore") as f:
    for line in f:
        count += line.count(pattern)
        print(count)