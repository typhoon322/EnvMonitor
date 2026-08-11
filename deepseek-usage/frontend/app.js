const cardsEl = document.getElementById("cards");
const emptyStateEl = document.getElementById("emptyState");
const keyCountEl = document.getElementById("keyCount");
const refreshHintEl = document.getElementById("refreshHint");
const refreshAllBtn = document.getElementById("refreshAllBtn");
const addKeyForm = document.getElementById("addKeyForm");
const editDialog = document.getElementById("editDialog");
const editForm = document.getElementById("editForm");
const cancelEditBtn = document.getElementById("cancelEdit");
const toastEl = document.getElementById("toast");

let refreshIntervalSec = 300;

function showToast(message, isError = false) {
  toastEl.textContent = message;
  toastEl.style.borderColor = isError ? "rgba(255,107,107,0.5)" : "var(--border)";
  toastEl.classList.remove("hidden");
  clearTimeout(showToast._timer);
  showToast._timer = setTimeout(() => toastEl.classList.add("hidden"), 3200);
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options,
  });
  if (response.status === 204) return null;
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.detail || `请求失败 (${response.status})`);
  }
  return data;
}

function formatTime(iso) {
  if (!iso) return "尚未刷新";
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return iso;
  return date.toLocaleString("zh-CN");
}

function statusBadge(isAvailable) {
  if (isAvailable === true) return '<span class="status ok">可用</span>';
  if (isAvailable === false) return '<span class="status bad">不可用</span>';
  return '<span class="status unknown">未知</span>';
}

function renderCards(balances) {
  keyCountEl.textContent = String(balances.length);
  if (!balances.length) {
    cardsEl.innerHTML = "";
    emptyStateEl.classList.remove("hidden");
    return;
  }
  emptyStateEl.classList.add("hidden");
  cardsEl.innerHTML = balances
    .map((item) => {
      const balance = item.balances[0];
      const errorHtml = item.error
        ? `<div class="error-box">${escapeHtml(item.error)}</div>`
        : "";
      const metrics = balance
        ? `
          <div class="metrics">
            <div class="metric">
              <span class="metric-label">总余额 (${balance.currency})</span>
              <span class="metric-value">${balance.total_balance}</span>
            </div>
            <div class="metric">
              <span class="metric-label">赠送余额</span>
              <span class="metric-value">${balance.granted_balance}</span>
            </div>
            <div class="metric">
              <span class="metric-label">充值余额</span>
              <span class="metric-value">${balance.topped_up_balance}</span>
            </div>
          </div>`
        : `<p class="meta">暂无余额数据，请点击刷新。</p>`;

      return `
        <article class="card" data-id="${item.key_id}">
          <div class="card-head">
            <div>
              <h3 class="card-title">${escapeHtml(item.name)}</h3>
              <p class="card-hint">${escapeHtml(item.key_hint)}</p>
            </div>
            ${statusBadge(item.is_available)}
          </div>
          ${errorHtml}
          ${metrics}
          <p class="meta">上次刷新：${formatTime(item.fetched_at)}</p>
          <div class="card-actions">
            <button class="btn" data-action="refresh">刷新</button>
            <button class="btn" data-action="edit">编辑</button>
            <button class="btn danger" data-action="delete">删除</button>
          </div>
        </article>`;
    })
    .join("");
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

async function loadDashboard() {
  const [health, balances] = await Promise.all([api("/api/health"), api("/api/balances")]);
  refreshIntervalSec = health.refresh_interval_sec;
  refreshHintEl.textContent = `自动刷新间隔 ${refreshIntervalSec}s`;
  renderCards(balances);
}

async function refreshAll() {
  refreshAllBtn.disabled = true;
  refreshAllBtn.textContent = "刷新中...";
  try {
    const balances = await api("/api/balances/refresh", { method: "POST" });
    renderCards(balances);
    showToast("已全部刷新");
  } catch (err) {
    showToast(err.message, true);
  } finally {
    refreshAllBtn.disabled = false;
    refreshAllBtn.textContent = "刷新全部";
  }
}

cardsEl.addEventListener("click", async (event) => {
  const button = event.target.closest("button[data-action]");
  if (!button) return;
  const card = button.closest(".card");
  const keyId = Number(card.dataset.id);
  const action = button.dataset.action;

  if (action === "refresh") {
    button.disabled = true;
    try {
      const item = await api(`/api/balances/${keyId}/refresh`, { method: "POST" });
      const balances = await api("/api/balances");
      renderCards(balances);
      showToast(`${item.name} 已刷新`);
    } catch (err) {
      showToast(err.message, true);
    } finally {
      button.disabled = false;
    }
    return;
  }

  if (action === "edit") {
    const balances = await api("/api/balances");
    const item = balances.find((row) => row.key_id === keyId);
    if (!item) return;
    document.getElementById("editId").value = String(keyId);
    document.getElementById("editName").value = item.name;
    document.getElementById("editKey").value = "";
    editDialog.showModal();
    return;
  }

  if (action === "delete") {
    if (!confirm("确定删除这个 Key？")) return;
    try {
      await api(`/api/keys/${keyId}`, { method: "DELETE" });
      await loadDashboard();
      showToast("已删除");
    } catch (err) {
      showToast(err.message, true);
    }
  }
});

addKeyForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const name = document.getElementById("addName").value.trim();
  const apiKey = document.getElementById("addKey").value.trim();
  try {
    await api("/api/keys", {
      method: "POST",
      body: JSON.stringify({ name, api_key: apiKey }),
    });
    addKeyForm.reset();
    await loadDashboard();
    showToast("Key 已添加");
  } catch (err) {
    showToast(err.message, true);
  }
});

editForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const keyId = Number(document.getElementById("editId").value);
  const name = document.getElementById("editName").value.trim();
  const apiKey = document.getElementById("editKey").value.trim();
  const payload = { name };
  if (apiKey) payload.api_key = apiKey;
  try {
    await api(`/api/keys/${keyId}`, {
      method: "PUT",
      body: JSON.stringify(payload),
    });
    editDialog.close();
    await loadDashboard();
    showToast("已保存");
  } catch (err) {
    showToast(err.message, true);
  }
});

cancelEditBtn.addEventListener("click", () => editDialog.close());
refreshAllBtn.addEventListener("click", refreshAll);

loadDashboard().catch((err) => showToast(err.message, true));
setInterval(() => {
  loadDashboard().catch(() => {});
}, Math.max(refreshIntervalSec, 30) * 1000);
