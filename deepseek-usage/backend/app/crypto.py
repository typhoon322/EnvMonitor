from cryptography.fernet import Fernet, InvalidToken


class KeyCipher:
    def __init__(self, master_key: str) -> None:
        if not master_key:
            raise ValueError("MASTER_KEY is required to encrypt API keys at rest")
        self._fernet = Fernet(master_key.encode() if isinstance(master_key, str) else master_key)

    def encrypt(self, plaintext: str) -> str:
        return self._fernet.encrypt(plaintext.encode()).decode()

    def decrypt(self, ciphertext: str) -> str:
        try:
            return self._fernet.decrypt(ciphertext.encode()).decode()
        except InvalidToken as exc:
            raise ValueError("Failed to decrypt API key; check MASTER_KEY") from exc
