import argparse
import math
from pathlib import Path

TARGET_VECTOR = [1.23, 2.34, 3.45, 4.56, 5.67, 1.11, 2.22, 3.33, 4.44, 5.55]

def parse_vector_line(line: str):
  line = line.strip()
  if not line or ":" not in line:
    return None
  idx_part, vec_part = line.split(":", 1)
  idx = int(idx_part.strip())
  vec_str = vec_part.strip().strip("[]")
  values = [float(x) for x in vec_str.split(",") if x.strip()]
  return idx, values

def euclidean_distance(vec1, vec2):
  if len(vec1) != len(vec2):
    raise ValueError("Vector dimension mismatch.")
  return math.sqrt(sum((a - b) ** 2 for a, b in zip(vec1, vec2)))

def main():
  parser = argparse.ArgumentParser(description="Compute Euclidean distance to target vector.")
  parser.add_argument("--input", type=Path, default="vector.txt", help="path to input vector file")
  parser.add_argument("--output", type=Path, default="cmpResult.txt", help="path to output result file")
  parser.add_argument("--precision", type=int, default=4, help="decimal places for distances")
  args = parser.parse_args()

  lines = args.input.read_text(encoding="utf-8").splitlines()
  entries = []
  for line in lines:
    parsed = parse_vector_line(line)
    if not parsed:
      continue
    idx, vec = parsed
    distance = euclidean_distance(vec, TARGET_VECTOR)
    entries.append((idx, distance))

  entries.sort(key=lambda item: item[1])

  results = [
      f"{rank},{idx},{distance:.{args.precision}f}"
      for rank, (idx, distance) in enumerate(entries, start=1)
  ]

  args.output.parent.mkdir(parents=True, exist_ok=True)
  args.output.write_text("\n".join(results) + "\n", encoding="utf-8")

if __name__ == "__main__":
  main()