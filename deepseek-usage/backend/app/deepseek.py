from dataclasses import dataclass
from typing import Any

import httpx


@dataclass
class BalanceInfo:
    currency: str
    total_balance: str
    granted_balance: str
    topped_up_balance: str


@dataclass
class BalanceResult:
    is_available: bool | None
    balances: list[BalanceInfo]
    raw: dict[str, Any]


class DeepSeekClient:
    def __init__(self, balance_url: str) -> None:
        self.balance_url = balance_url

    async def fetch_balance(self, api_key: str) -> BalanceResult:
        headers = {"Authorization": f"Bearer {api_key}"}
        async with httpx.AsyncClient(timeout=20.0) as client:
            response = await client.get(self.balance_url, headers=headers)
            response.raise_for_status()
            payload = response.json()

        balances: list[BalanceInfo] = []
        for item in payload.get("balance_infos", []):
            balances.append(
                BalanceInfo(
                    currency=str(item.get("currency", "")),
                    total_balance=str(item.get("total_balance", "0")),
                    granted_balance=str(item.get("granted_balance", "0")),
                    topped_up_balance=str(item.get("topped_up_balance", "0")),
                )
            )

        return BalanceResult(
            is_available=payload.get("is_available"),
            balances=balances,
            raw=payload,
        )
