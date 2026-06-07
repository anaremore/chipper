import argparse
from pathlib import Path


def parse_hex_bytes(text: str) -> bytes:
    values = []
    for token in text.replace(",", " ").split():
        token = token.strip()
        if not token:
            continue
        values.append(int(token, 16) & 0xFF)
    return bytes(values)


def main() -> int:
    parser = argparse.ArgumentParser(description="Write a generated binary fixture for renderer tests.")
    parser.add_argument("path")
    parser.add_argument("hex_bytes")
    args = parser.parse_args()

    path = Path(args.path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(parse_hex_bytes(args.hex_bytes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
