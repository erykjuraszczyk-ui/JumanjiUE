
@unittest.skipUnless(
    os.environ.get("JUMANJI_TEST_POSTGRES_URL"),
    "Set JUMANJI_TEST_POSTGRES_URL to run PostgreSQL provider integration tests",
)
class PostgreSQLCheckpointProviderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.key = Ed25519PrivateKey.generate()
        self.database_url = os.environ["JUMANJI_TEST_POSTGRES_URL"]
        self.backend = EncounterCheckpointBackend(
            None,
            self.key,
            "POSTGRES-TEST-KEY-01",
            database_url=self.database_url,
            allow_insecure_database=os.environ.get("JUMANJI_CHECKPOINT_ALLOW_INSECURE_DATABASE") == "true",
        )
        self.identity = {"serverInstanceId": SERVER_ID, "shardId": SHARD_ID}
        self.encounter_id = f"ENCOUNTER_PG_{uuid4().hex[:12].upper()}"
        self.lease = self.backend.acquire_lease(
            self.encounter_id,
            {**self.identity, "ttlSeconds": 45},
        )

    def make_payload(self, sequence: int) -> dict:
        fixture_path = ROOT / "Data/Encounter/EncounterCheckpointExample.json"
        payload = copy.deepcopy(json.loads(fixture_path.read_text(encoding="utf-8"))["payload"])
        now = datetime.now(timezone.utc).replace(microsecond=0)
        payload["checkpointId"] = str(uuid4())
        payload["sessionId"] = str(uuid4())
        payload["encounterId"] = self.encounter_id
        payload["checkpointSequence"] = sequence
        payload["createdAtUtc"] = now.isoformat().replace("+00:00", "Z")
        payload["expiresAtUtc"] = (now + timedelta(minutes=20)).isoformat().replace("+00:00", "Z")
        payload["authority"]["serverInstanceId"] = SERVER_ID
        payload["authority"]["shardId"] = SHARD_ID
        payload["authority"]["authorityEpoch"] = self.lease["authorityEpoch"]
        return payload

    def make_request(self, payload: dict, expected_sequence: int) -> dict:
        return {
            **self.identity,
            "leaseToken": self.lease["leaseToken"],
            "authorityEpoch": self.lease["authorityEpoch"],
            "expectedSequence": expected_sequence,
            "payload": payload,
        }

    def test_postgres_persists_signed_checkpoint_and_cas(self) -> None:
        payload = self.make_payload(1)
        receipt = self.backend.write_checkpoint(
            self.encounter_id,
            self.make_request(payload, 0),
        )
        self.assertEqual(receipt["checkpointSequence"], 1)
        response = self.backend.read_checkpoint(
            self.encounter_id,
            {
                **self.identity,
                "leaseToken": self.lease["leaseToken"],
                "authorityEpoch": self.lease["authorityEpoch"],
            },
        )
        self.assertEqual(response["envelope"]["payload"]["checkpointId"], payload["checkpointId"])

        with self.assertRaises(CheckpointServiceError) as conflict:
            self.backend.write_checkpoint(
                self.encounter_id,
                self.make_request(self.make_payload(2), 0),
            )
        self.assertEqual(conflict.exception.code, "SEQUENCE_CONFLICT")

    def test_postgres_fences_old_lease_after_reacquire(self) -> None:
        old = self.lease
        self.backend.release_lease(
            self.encounter_id,
            {
                **self.identity,
                "leaseToken": old["leaseToken"],
                "authorityEpoch": old["authorityEpoch"],
            },
        )
        newer = self.backend.acquire_lease(
            self.encounter_id,
            {**self.identity, "ttlSeconds": 45},
        )
        self.assertEqual(newer["authorityEpoch"], old["authorityEpoch"] + 1)
        with self.assertRaises(CheckpointServiceError) as stale:
            self.backend.read_checkpoint(
                self.encounter_id,
                {
                    **self.identity,
                    "leaseToken": old["leaseToken"],
                    "authorityEpoch": old["authorityEpoch"],
                },
            )
        self.assertEqual(stale.exception.code, "LEASE_INVALID")


