from __future__ import annotations

import asyncio
from typing import Any

from app.crypto import KeyCipher
from app.database import Database, utc_now_iso
from app.deepseek import DeepSeekClient


def mask_key_hint(api_key: str) -> str:
    if len(api_key) <= 8:
        return "****"
    return f"{api_key[:4]}...{api_key[-4:]}"


class KeyStore:
    def __init__(self, db: Database, cipher: KeyCipher) -> None:
        self.db = db
        self.cipher = cipher

    def list_keys(self) -> list[dict[str, Any]]:
        with self.db.connect() as conn:
            rows = conn.execute(
                "SELECT id, name, key_hint, created_at, updated_at FROM api_keys ORDER BY id"
            ).fetchall()
        return [dict(row) for row in rows]

    def create_key(self, name: str, api_key: str) -> dict[str, Any]:
        now = utc_now_iso()
        hint = mask_key_hint(api_key)
        cipher = self.cipher.encrypt(api_key)
        with self.db.connect() as conn:
            cur = conn.execute(
                """
                INSERT INTO api_keys (name, key_cipher, key_hint, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?)
                """,
                (name, cipher, hint, now, now),
            )
            key_id = cur.lastrowid
            row = conn.execute(
                "SELECT id, name, key_hint, created_at, updated_at FROM api_keys WHERE id = ?",
                (key_id,),
            ).fetchone()
        return dict(row)

    def update_key(self, key_id: int, name: str | None, api_key: str | None) -> dict[str, Any] | None:
        with self.db.connect() as conn:
            row = conn.execute("SELECT * FROM api_keys WHERE id = ?", (key_id,)).fetchone()
            if row is None:
                return None

            new_name = name if name is not None else row["name"]
            new_cipher = row["key_cipher"]
            new_hint = row["key_hint"]
            if api_key is not None:
                new_cipher = self.cipher.encrypt(api_key)
                new_hint = mask_key_hint(api_key)

            conn.execute(
                """
                UPDATE api_keys
                SET name = ?, key_cipher = ?, key_hint = ?, updated_at = ?
                WHERE id = ?
                """,
                (new_name, new_cipher, new_hint, utc_now_iso(), key_id),
            )
            updated = conn.execute(
                "SELECT id, name, key_hint, created_at, updated_at FROM api_keys WHERE id = ?",
                (key_id,),
            ).fetchone()
        return dict(updated)

    def delete_key(self, key_id: int) -> bool:
        with self.db.connect() as conn:
            cur = conn.execute("DELETE FROM api_keys WHERE id = ?", (key_id,))
        return cur.rowcount > 0

    def get_decrypted_key(self, key_id: int) -> tuple[dict[str, Any], str] | None:
        with self.db.connect() as conn:
            row = conn.execute("SELECT * FROM api_keys WHERE id = ?", (key_id,)).fetchone()
        if row is None:
            return None
        api_key = self.cipher.decrypt(row["key_cipher"])
        meta = {
            "id": row["id"],
            "name": row["name"],
            "key_hint": row["key_hint"],
            "created_at": row["created_at"],
            "updated_at": row["updated_at"],
        }
        return meta, api_key

    def save_balance_cache(
        self,
        key_id: int,
        *,
        is_available: bool | None,
        currency: str | None,
        total_balance: str | None,
        granted_balance: str | None,
        topped_up_balance: str | None,
        fetched_at: str,
        error: str | None,
    ) -> None:
        with self.db.connect() as conn:
            conn.execute(
                """
                INSERT INTO balance_cache (
                    key_id, is_available, currency, total_balance,
                    granted_balance, topped_up_balance, fetched_at, error
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(key_id) DO UPDATE SET
                    is_available = excluded.is_available,
                    currency = excluded.currency,
                    total_balance = excluded.total_balance,
                    granted_balance = excluded.granted_balance,
                    topped_up_balance = excluded.topped_up_balance,
                    fetched_at = excluded.fetched_at,
                    error = excluded.error
                """,
                (
                    key_id,
                    None if is_available is None else int(is_available),
                    currency,
                    total_balance,
                    granted_balance,
                    topped_up_balance,
                    fetched_at,
                    error,
                ),
            )

    def list_cached_balances(self) -> list[dict[str, Any]]:
        with self.db.connect() as conn:
            rows = conn.execute(
                """
                SELECT
                    k.id AS key_id,
                    k.name,
                    k.key_hint,
                    c.is_available,
                    c.currency,
                    c.total_balance,
                    c.granted_balance,
                    c.topped_up_balance,
                    c.fetched_at,
                    c.error
                FROM api_keys k
                LEFT JOIN balance_cache c ON c.key_id = k.id
                ORDER BY k.id
                """
            ).fetchall()
        return [dict(row) for row in rows]


class BalanceService:
    def __init__(self, store: KeyStore, client: DeepSeekClient) -> None:
        self.store = store
        self.client = client
        self._lock = asyncio.Lock()

    async def refresh_one(self, key_id: int) -> dict[str, Any] | None:
        item = self.store.get_decrypted_key(key_id)
        if item is None:
            return None
        meta, api_key = item
        fetched_at = utc_now_iso()
        try:
            result = await self.client.fetch_balance(api_key)
            primary = result.balances[0] if result.balances else None
            self.store.save_balance_cache(
                key_id,
                is_available=result.is_available,
                currency=primary.currency if primary else None,
                total_balance=primary.total_balance if primary else None,
                granted_balance=primary.granted_balance if primary else None,
                topped_up_balance=primary.topped_up_balance if primary else None,
                fetched_at=fetched_at,
                error=None,
            )
        except Exception as exc:  # noqa: BLE001 - surface API errors to UI
            self.store.save_balance_cache(
                key_id,
                is_available=None,
                currency=None,
                total_balance=None,
                granted_balance=None,
                topped_up_balance=None,
                fetched_at=fetched_at,
                error=str(exc),
            )
        return self._format_row(key_id)

    async def refresh_all(self) -> list[dict[str, Any]]:
        async with self._lock:
            keys = self.store.list_keys()
            tasks = [self.refresh_one(key["id"]) for key in keys]
            results = await asyncio.gather(*tasks)
        return [row for row in results if row is not None]

    def list_balances(self) -> list[dict[str, Any]]:
        rows = self.store.list_cached_balances()
        return [self._format_cached_row(row) for row in rows]

    def _format_row(self, key_id: int) -> dict[str, Any] | None:
        rows = self.store.list_cached_balances()
        for row in rows:
            if row["key_id"] == key_id:
                return self._format_cached_row(row)
        return None

    def _format_cached_row(self, row: dict[str, Any]) -> dict[str, Any]:
        balances = []
        if row.get("currency"):
            balances.append(
                {
                    "currency": row["currency"],
                    "total_balance": row.get("total_balance") or "0",
                    "granted_balance": row.get("granted_balance") or "0",
                    "topped_up_balance": row.get("topped_up_balance") or "0",
                }
            )
        is_available = row.get("is_available")
        return {
            "key_id": row["key_id"],
            "name": row["name"],
            "key_hint": row["key_hint"],
            "is_available": None if is_available is None else bool(is_available),
            "balances": balances,
            "fetched_at": row.get("fetched_at"),
            "error": row.get("error"),
        }
