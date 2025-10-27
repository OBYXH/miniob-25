from pathlib import Path

INPUT_FILE = Path(__file__).with_name("insert.txt")
OUTPUT_FILE = Path(__file__).with_name("result.txt")

def process_lines(lines):
    formatted = []
    for raw in lines:
        line = raw.rstrip("\n")
        if not line:
            continue
        formatted.append(line)
        formatted.append("SUCCESS")
    return "\n".join(formatted) + "\n"

def main():
    if not INPUT_FILE.exists():
        raise FileNotFoundError(f"Input file not found: {INPUT_FILE}")
    lines = INPUT_FILE.read_text(encoding="utf-8").splitlines()
    OUTPUT_FILE.write_text(process_lines(lines), encoding="utf-8")

if __name__ == "__main__":
    main()