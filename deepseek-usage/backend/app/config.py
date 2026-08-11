from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict

# deepseek-usage/ (two levels up from backend/app/)
PROJECT_ROOT = Path(__file__).resolve().parents[2]
ENV_FILE = PROJECT_ROOT / ".env"


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=str(ENV_FILE) if ENV_FILE.is_file() else None,
        env_file_encoding="utf-8",
        extra="ignore",
    )

    master_key: str = ""
    host: str = "0.0.0.0"
    port: int = 8787
    data_dir: Path = PROJECT_ROOT / "data"
    refresh_interval_sec: int = 300
    deepseek_balance_url: str = "https://api.deepseek.com/user/balance"

    @property
    def db_path(self) -> Path:
        return self.data_dir / "deepseek_usage.db"


settings = Settings()
