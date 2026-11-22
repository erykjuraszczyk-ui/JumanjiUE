FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1

WORKDIR /app

COPY Backend/EncounterCheckpointService/requirements.txt /app/requirements.txt
RUN python -m pip install --no-cache-dir --requirement /app/requirements.txt \
    && groupadd --gid 10001 checkpoint \
    && useradd --uid 10001 --gid 10001 --no-create-home --shell /usr/sbin/nologin checkpoint

COPY Backend/EncounterCheckpointService /app/Backend/EncounterCheckpointService
COPY Scripts/validate_encounter_checkpoint_contract.py /app/Scripts/validate_encounter_checkpoint_contract.py
COPY Data/Encounter/EncounterCheckpoint.schema.json /app/Data/Encounter/EncounterCheckpoint.schema.json

USER 10001:10001

EXPOSE 8787

ENTRYPOINT ["python", "Backend/EncounterCheckpointService/service.py"]
