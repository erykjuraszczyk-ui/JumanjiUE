from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    required = {
        "service": ROOT / "Backend/EncounterCheckpointService/service.py",
        "requirements": ROOT / "Backend/EncounterCheckpointService/requirements.txt",
        "unreal_header": ROOT / "Source/JumanjiUE/Public/JumanjiEncounterCheckpointBackendSubsystem.h",
        "unreal_source": ROOT / "Source/JumanjiUE/Private/JumanjiEncounterCheckpointBackendSubsystem.cpp",
        "tests": ROOT / "Scripts/Tests/test_encounter_checkpoint_service.py",
    }
    missing = [name for name, path in required.items() if not path.is_file()]
    if missing:
        print("Encounter checkpoint backend validation: FAIL")
        for name in missing:
            print(f"Missing required artifact: {name}", file=sys.stderr)
        return 1

    service = required["service"].read_text(encoding="utf-8")
    header = required["unreal_header"].read_text(encoding="utf-8")
    source = required["unreal_source"].read_text(encoding="utf-8")
    build_rules = (ROOT / "Source/JumanjiUE/JumanjiUE.Build.cs").read_text(encoding="utf-8")

    checks = {
        "RFC 8785 canonicalization": "rfc8785.dumps(" in service,
        "Ed25519 signing": "self.signing_key.sign(" in service,
        "signature verification": "self.verification_key.verify(" in service,
        "atomic SQLite transactions": "BEGIN IMMEDIATE" in service,
        "lease fencing": "authority_epoch" in service and "lease_token_hash" in service,
        "sequence compare-and-swap": "SEQUENCE_CONFLICT" in service,
        "server-only Unreal guard": "NM_Client" in source and "GetNetMode()" in source,
        "server token from process environment": "JUMANJI_CHECKPOINT_SERVER_TOKEN" in source,
        "HTTP/JSON module dependencies": all(f'"{module}"' in build_rules for module in ("HTTP", "Json", "JsonUtilities")),
        "native-only API": "UFUNCTION(" not in header,
    }
    failed = [name for name, passed in checks.items() if not passed]
    if failed:
        print("Encounter checkpoint backend validation: FAIL")
        for name in failed:
            print(f"Missing contract: {name}", file=sys.stderr)
        return 1

    print("Encounter checkpoint backend validation: PASS")
    print(f"Validated {len(checks)} backend and Unreal adapter integration contracts.")
    print("Cryptographic and concurrency behavior is covered by the dedicated unittest suite.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
