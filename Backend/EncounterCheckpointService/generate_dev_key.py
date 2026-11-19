from __future__ import annotations

import argparse
import os
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, NoEncryption, PrivateFormat


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a local TEST-ONLY Ed25519 key for the checkpoint prototype.")
    parser.add_argument("--output", type=Path, default=Path(".secrets/checkpoint-ed25519-dev.pem"))
    parser.add_argument("--force", action="store_true", help="Replace an existing development key.")
    args = parser.parse_args()

    output = args.output.resolve()
    if output.exists() and not args.force:
        parser.error(f"{output} already exists; use --force only when rotating a local development key")

    output.parent.mkdir(parents=True, exist_ok=True)
    private_key = Ed25519PrivateKey.generate()
    pem = private_key.private_bytes(Encoding.PEM, PrivateFormat.PKCS8, NoEncryption())
    output.write_bytes(pem)
    if os.name == "posix":
        output.chmod(0o600)

    print(f"Created local development key: {output}")
    print("Key material is not printed. Do not commit this file or use it in production.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
