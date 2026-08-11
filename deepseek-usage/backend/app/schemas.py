from pydantic import BaseModel, Field


class ApiKeyCreate(BaseModel):
    name: str = Field(min_length=1, max_length=64)
    api_key: str = Field(min_length=8, max_length=256)


class ApiKeyUpdate(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=64)
    api_key: str | None = Field(default=None, min_length=8, max_length=256)


class ApiKeyOut(BaseModel):
    id: int
    name: str
    key_hint: str
    created_at: str
    updated_at: str


class BalanceInfoOut(BaseModel):
    currency: str
    total_balance: str
    granted_balance: str
    topped_up_balance: str


class KeyBalanceOut(BaseModel):
    key_id: int
    name: str
    key_hint: str
    is_available: bool | None = None
    balances: list[BalanceInfoOut] = []
    fetched_at: str | None = None
    error: str | None = None


class HealthOut(BaseModel):
    status: str
    refresh_interval_sec: int
