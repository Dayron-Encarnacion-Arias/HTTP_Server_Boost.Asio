const health = document.querySelector('[data-health]');

if (health) {
    fetch('/api/health', { headers: { Accept: 'application/json' } })
        .then((response) => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        })
        .then((data) => { health.textContent = data.status === 'ok' ? 'ONLINE' : 'DEGRADED'; })
        .catch(() => { health.textContent = 'OFFLINE'; });
}
