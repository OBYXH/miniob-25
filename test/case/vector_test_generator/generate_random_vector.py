import argparse
import random
from pathlib import Path

def format_vector(values):
  return "[" + ",".join(f"{v:.2f}" for v in values) + "]"

def main():
  parser = argparse.ArgumentParser(description="Generate vector insert queries.")
  parser.add_argument("--count", type=int, required=True, help="number of queries to generate")
  parser.add_argument("--query-file", type=Path, default="insert.txt", help="output path for SQL queries")
  parser.add_argument("--vector-file", type=Path, default="vector.txt", help="output path for vectors with ids")
  parser.add_argument("--lower", type=float, default=0.0, help="lower bound for random values")
  parser.add_argument("--upper", type=float, default=5.0, help="upper bound for random values")
  parser.add_argument("--dimensions", type=int, default=10, help="vector dimensions")

  args = parser.parse_args()
  args.query_file.parent.mkdir(parents=True, exist_ok=True)
  args.vector_file.parent.mkdir(parents=True, exist_ok=True)

  queries = []
  vectors = []

  for idx in range(1, args.count + 1):
    vec = [round(random.uniform(args.lower, args.upper), 2) for _ in range(args.dimensions)]
    vec_str = format_vector(vec)
    queries.append(f"INSERT INTO TAB_VEC VALUES({idx}, {idx}, STRING_TO_VECTOR('{vec_str}'));")
    vectors.append(f"{idx}: {vec_str}")

  args.query_file.write_text("\n".join(queries) + "\n", encoding="utf-8")
  args.vector_file.write_text("\n".join(vectors) + "\n", encoding="utf-8")

if __name__ == "__main__":
  main()