from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from app.config import settings
from app.crypto import KeyCipher
from app.database import Database
from app.deepseek import DeepSeekClient
from app.schemas import ApiKeyCreate, ApiKeyOut, ApiKeyUpdate, HealthOut, KeyBalanceOut
from app.store import BalanceService, KeyStore

FRONTEND_DIR = Path(__file__).resolve().parents[2] / "frontend"


def create_app() -> FastAPI:
    db = Database(settings.db_path)
    cipher = KeyCipher(settings.master_key)
    key_store = KeyStore(db, cipher)
    balance_service = BalanceService(key_store, DeepSeekClient(settings.deepseek_balance_url))
    refresh_task: asyncio.Task | None = None

    async def refresh_loop() -> None:
        while True:
            await asyncio.sleep(settings.refresh_interval_sec)
            if key_store.list_keys():
                await balance_service.refresh_all()

    @asynccontextmanager
    async def lifespan(_: FastAPI):
        nonlocal refresh_task
        refresh_task = asyncio.create_task(refresh_loop())
        yield
        if refresh_task:
            refresh_task.cancel()
            try:
                await refresh_task
            except asyncio.CancelledError:
                pass

    app = FastAPI(title="DeepSeek Usage Monitor", lifespan=lifespan)
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_methods=["*"],
        allow_headers=["*"],
    )

    @app.get("/api/health", response_model=HealthOut)
    def health() -> HealthOut:
        return HealthOut(status="ok", refresh_interval_sec=settings.refresh_interval_sec)

    @app.get("/api/keys", response_model=list[ApiKeyOut])
    def list_keys() -> list[ApiKeyOut]:
        return [ApiKeyOut(**row) for row in key_store.list_keys()]

    @app.post("/api/keys", response_model=ApiKeyOut, status_code=201)
    async def create_key(body: ApiKeyCreate) -> ApiKeyOut:
        created = key_store.create_key(body.name.strip(), body.api_key.strip())
        await balance_service.refresh_one(created["id"])
        return ApiKeyOut(**created)

    @app.put("/api/keys/{key_id}", response_model=ApiKeyOut)
    def update_key(key_id: int, body: ApiKeyUpdate) -> ApiKeyOut:
        updated = key_store.update_key(
            key_id,
            body.name.strip() if body.name is not None else None,
            body.api_key.strip() if body.api_key is not None else None,
        )
        if updated is None:
            raise HTTPException(status_code=404, detail="Key not found")
        return ApiKeyOut(**updated)

    @app.delete("/api/keys/{key_id}", status_code=204)
    def delete_key(key_id: int) -> None:
        if not key_store.delete_key(key_id):
            raise HTTPException(status_code=404, detail="Key not found")

    @app.get("/api/balances", response_model=list[KeyBalanceOut])
    def list_balances() -> list[KeyBalanceOut]:
        return [KeyBalanceOut(**row) for row in balance_service.list_balances()]

    @app.post("/api/balances/refresh", response_model=list[KeyBalanceOut])
    async def refresh_balances() -> list[KeyBalanceOut]:
        rows = await balance_service.refresh_all()
        return [KeyBalanceOut(**row) for row in rows]

    @app.post("/api/balances/{key_id}/refresh", response_model=KeyBalanceOut)
    async def refresh_balance(key_id: int) -> KeyBalanceOut:
        row = await balance_service.refresh_one(key_id)
        if row is None:
            raise HTTPException(status_code=404, detail="Key not found")
        return KeyBalanceOut(**row)

    if FRONTEND_DIR.is_dir():
        app.mount("/assets", StaticFiles(directory=FRONTEND_DIR), name="assets")

        @app.get("/")
        def index() -> FileResponse:
            return FileResponse(FRONTEND_DIR / "index.html")

    return app


app = create_app()
