FROM python:3.12-slim

WORKDIR /app

COPY backend/requirements.txt /app/backend/requirements.txt
RUN pip install --no-cache-dir -r /app/backend/requirements.txt

COPY backend /app/backend
COPY frontend /app/frontend

ENV HOST=0.0.0.0
ENV PORT=8787
ENV DATA_DIR=/data

WORKDIR /app/backend
EXPOSE 8787

CMD ["python", "run.py"]
