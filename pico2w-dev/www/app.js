/* ============================================================================
   Pico 2W Dashboard — Client-side JavaScript
   Handles GPS auto-refresh, LED control commands, pixel grid, and track plot.
   ============================================================================ */

// ─── GPS Auto-Refresh ───────────────────────────────────────────────────────

let trackData = [];

function refreshGpsData() {
  fetch('/gps/data')
    .then(r => r.text())
    .then(html => {
      // Parse the SSI-populated data attributes from the fragment
      const parser = new DOMParser();
      const doc = parser.parseFromString(html, 'text/html');
      const el = doc.getElementById('gpsJsonData');
      if (!el) return;

      const d = el.dataset;

      // Update GPS fields
      const setText = (id, val) => {
        const e = document.getElementById(id);
        if (e) e.textContent = val;
      };

      setText('gpsLat', d.lat);
      setText('gpsLon', d.lon);
      setText('gpsAlt', d.alt);
      setText('gpsSpd', d.spd);
      setText('gpsCrs', d.crs);
      setText('gpsSat', d.sat);
      setText('gpsHdop', d.hdop);
      setText('gpsTime', d.time);
      setText('gpsDate', d.date);
      setText('gpsSentences', d.sentences);
      setText('activeEffect', d.effect || 'none');

      // Update fix badge
      const fixBadge = document.getElementById('gpsFixBadge');
      if (fixBadge) {
        const valid = d.valid === 'true';
        const fix = parseInt(d.fix) || 0;
        const sats = parseInt(d.sat) || 0;

        if (!valid || fix === 0) {
          fixBadge.innerHTML = '<span class="fix-badge no-fix">No Fix</span>';
        } else if (sats < 6) {
          fixBadge.innerHTML = '<span class="fix-badge weak-fix">Fix (' + sats + ' sats)</span>';
        } else {
          fixBadge.innerHTML = '<span class="fix-badge has-fix">Fix (' + sats + ' sats)</span>';
        }
      }

      // Update status bar
      const dot = document.getElementById('statusDot');
      const statusText = document.getElementById('statusText');
      if (dot && statusText) {
        dot.style.background = '#22c55e';
        statusText.textContent = 'Connected';
      }

      // Parse track data and redraw
      if (d.track) {
        try {
          trackData = JSON.parse(d.track);
          drawTrack();
        } catch (e) { /* ignore parse errors */ }
      }
    })
    .catch(() => {
      const dot = document.getElementById('statusDot');
      const statusText = document.getElementById('statusText');
      if (dot) dot.style.background = '#ef4444';
      if (statusText) statusText.textContent = 'Disconnected';
    });
}

// Refresh every 2 seconds
setInterval(refreshGpsData, 2000);
refreshGpsData();

// ─── LED Control ────────────────────────────────────────────────────────────

function hexToRgb(hex) {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return { r, g, b };
}

function sendBrightness(val) {
  fetch('/led/brightness?val=' + val);
}

function sendSpeed(val) {
  fetch('/led/speed?val=' + val);
}

function setPreset(color) {
  document.getElementById('colorPicker').value = color;
}

function setColor() {
  const color = hexToRgb(document.getElementById('colorPicker').value);
  const bright = document.getElementById('brightness').value;
  fetch('/led/set?r=' + color.r + '&g=' + color.g + '&b=' + color.b + '&bright=' + bright);
}

function setEffect(type) {
  const color = hexToRgb(document.getElementById('colorPicker').value);
  let url = '/led/effect?type=' + type;

  if (type === 'breathe' || type === 'strobe') {
    url += '&r=' + color.r + '&g=' + color.g + '&b=' + color.b;
  }

  // Also send brightness and speed for non-off effects
  if (type !== 'off') {
    const bright = document.getElementById('brightness').value;
    const speed = document.getElementById('speed').value;
    fetch('/led/brightness?val=' + bright);
    fetch('/led/speed?val=' + speed);
  }

  fetch(url);
}

// ─── 8x8 Pixel Grid ────────────────────────────────────────────────────────

function initGrid() {
  const grid = document.getElementById('matrixGrid');
  if (!grid) return;

  for (let y = 0; y < 8; y++) {
    for (let x = 0; x < 8; x++) {
      const pixel = document.createElement('div');
      pixel.className = 'matrix-pixel';
      pixel.dataset.x = x;
      pixel.dataset.y = y;

      pixel.addEventListener('click', () => {
        const color = hexToRgb(document.getElementById('colorPicker').value);
        pixel.style.background = document.getElementById('colorPicker').value;
        fetch('/led/pixel?x=' + x + '&y=' + y + '&r=' + color.r + '&g=' + color.g + '&b=' + color.b);
      });

      // Drag painting
      pixel.addEventListener('mouseenter', (e) => {
        if (e.buttons === 1) {
          const color = hexToRgb(document.getElementById('colorPicker').value);
          pixel.style.background = document.getElementById('colorPicker').value;
          fetch('/led/pixel?x=' + x + '&y=' + y + '&r=' + color.r + '&g=' + color.g + '&b=' + color.b);
        }
      });

      grid.appendChild(pixel);
    }
  }
}

initGrid();

// ─── GPS Track Plotter ──────────────────────────────────────────────────────

function drawTrack() {
  const canvas = document.getElementById('trackCanvas');
  if (!canvas || trackData.length < 2) return;

  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  // Clear
  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, w, h);

  // Find bounding box
  let minLat = 999, maxLat = -999, minLon = 999, maxLon = -999;
  for (const pt of trackData) {
    if (pt[0] < minLat) minLat = pt[0];
    if (pt[0] > maxLat) maxLat = pt[0];
    if (pt[1] < minLon) minLon = pt[1];
    if (pt[1] > maxLon) maxLon = pt[1];
  }

  // Add padding
  const latRange = Math.max(maxLat - minLat, 0.0001);
  const lonRange = Math.max(maxLon - minLon, 0.0001);
  const pad = 0.1;
  minLat -= latRange * pad;
  maxLat += latRange * pad;
  minLon -= lonRange * pad;
  maxLon += lonRange * pad;

  const scaleX = (w - 40) / (maxLon - minLon);
  const scaleY = (h - 40) / (maxLat - minLat);

  const toX = lon => 20 + (lon - minLon) * scaleX;
  const toY = lat => h - 20 - (lat - minLat) * scaleY;

  // Draw grid
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 0.5;
  for (let i = 0; i <= 4; i++) {
    const y = 20 + i * (h - 40) / 4;
    ctx.beginPath(); ctx.moveTo(20, y); ctx.lineTo(w - 20, y); ctx.stroke();
    const x = 20 + i * (w - 40) / 4;
    ctx.beginPath(); ctx.moveTo(x, 20); ctx.lineTo(x, h - 20); ctx.stroke();
  }

  // Draw track line
  ctx.strokeStyle = '#3b82f6';
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  ctx.beginPath();
  ctx.moveTo(toX(trackData[0][1]), toY(trackData[0][0]));
  for (let i = 1; i < trackData.length; i++) {
    ctx.lineTo(toX(trackData[i][1]), toY(trackData[i][0]));
  }
  ctx.stroke();

  // Draw points
  for (let i = 0; i < trackData.length; i++) {
    const x = toX(trackData[i][1]);
    const y = toY(trackData[i][0]);
    const speed = trackData[i][2] || 0;

    // Color by speed: blue (slow) → green → red (fast)
    const hue = Math.max(0, 240 - speed * 8);
    ctx.fillStyle = 'hsl(' + hue + ', 80%, 55%)';
    ctx.beginPath();
    ctx.arc(x, y, i === trackData.length - 1 ? 5 : 3, 0, Math.PI * 2);
    ctx.fill();
  }

  // Current position label
  if (trackData.length > 0) {
    const last = trackData[trackData.length - 1];
    const x = toX(last[1]);
    const y = toY(last[0]);
    ctx.fillStyle = '#e2e8f0';
    ctx.font = '11px Inter, sans-serif';
    ctx.fillText(last[0].toFixed(4) + ', ' + last[1].toFixed(4), x + 8, y - 8);
  }

  // Legend
  ctx.fillStyle = '#64748b';
  ctx.font = '10px Inter, sans-serif';
  ctx.fillText(trackData.length + ' points', w - 80, h - 6);
}
