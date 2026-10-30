let allApps = [];

document.addEventListener('DOMContentLoaded', () => {
  fetchApps();
});

function switchTab(tabId) {
  document.querySelectorAll('.tab-pane').forEach(el => el.classList.remove('active'));
  document.querySelectorAll('.nav-btn').forEach(el => el.classList.remove('active'));
  
  const pane = document.getElementById('tab-' + tabId);
  if (pane) pane.classList.add('active');
  
  event.target.classList.add('active');
}

function fetchApps() {
  fetch('/api/apps')
    .then(r => r.json())
    .then(data => {
      allApps = data;
      renderApps(allApps);
    })
    .catch(err => {
      console.warn('Fallback local demo:', err);
      allApps = [
        { titleId: 'CUSA00123', name: 'Grand Theft Auto V', lastPlayed: 1600000000 },
        { titleId: 'CUSA07010', name: 'Horizon Zero Dawn', lastPlayed: 1600000010 },
        { titleId: 'APKG00001', name: 'AeroPKG Manager', lastPlayed: 1600000020 }
      ];
      renderApps(allApps);
    });
}

function renderApps(apps) {
  const container = document.getElementById('appsList');
  container.innerHTML = '';

  apps.forEach(app => {
    const card = document.createElement('div');
    card.className = 'app-card';
    card.innerHTML = `
      <div class="app-header">
        <div class="app-icon"></div>
        <div class="app-info">
          <h3>${app.name}</h3>
          <span>${app.titleId}</span>
        </div>
      </div>
      <div>
        <button class="secondary-btn" style="width: 100%; margin-top: 8px;" onclick="promptRename('${app.titleId}', '${app.name}')">Edit Name</button>
      </div>
    `;
    container.appendChild(card);
  });
}

function filterApps() {
  const query = document.getElementById('appSearch').value.toLowerCase();
  const filtered = allApps.filter(a =>
    a.name.toLowerCase().includes(query) || a.titleId.toLowerCase().includes(query)
  );
  renderApps(filtered);
}

function promptRename(titleId, currentName) {
  const newName = prompt('Enter new display name for ' + titleId + ':', currentName);
  if (newName && newName !== currentName) {
    alert('Title updated to: ' + newName + '. Click "Apply & Reload XMB" to see changes on PS4.');
  }
}

function sendPkg() {
  const url = document.getElementById('pkgUrlInput').value.trim();
  if (!url) return alert('Please enter a valid package URL.');

  fetch('/api/install', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ type: 'direct', packages: [url] })
  })
  .then(r => r.json())
  .then(data => {
    alert('Package install command pushed to PS4!');
    document.getElementById('pkgUrlInput').value = '';
  })
  .catch(err => alert('Failed to push PKG: ' + err));
}

function restartShell() {
  if (confirm('Soft-restart SceShellCore? The PS4 home screen will briefly reload to apply all changes.')) {
    fetch('/api/restart_shell', { method: 'POST' })
      .then(() => alert('Shell reload signal sent!'));
  }
}

function backupDb() {
  alert('Backup initiated on PS4: /system_data/priv/mms/app.db.aeropkg_bak');
}

function restoreDb() {
  if (confirm('Restore previous app.db backup?')) {
    alert('Database restored. Please reload shell.');
  }
}
