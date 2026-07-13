import { CONFIG } from './config.js';

// ====================== STATE GLOBAL ======================
let sensorData = null;
let activeMetric = 'temperature';
let deviceOnline = false;
let heartbeatTimer = null;

const history = {
  labels: [],
  temperature: [],
  humidity: [],
  pressure: [],
  gas: []
};

const SENSORS = {
  temperature: { label: 'Suhu',       unit: '°C',  color: '#f87171', bgColorGrad: 'rgba(248,113,113,0.2)' },
  humidity:    { label: 'Kelembapan', unit: '%',   color: '#60a5fa', bgColorGrad: 'rgba(96,165,250,0.2)' },
  pressure:    { label: 'Tekanan',    unit: 'hPa', color: '#a78bfa', bgColorGrad: 'rgba(167,139,250,0.2)' },
  gas:         { label: 'Gas (ADC)',  unit: '',     color: '#34d399', bgColorGrad: 'rgba(52,211,153,0.2)' }
};

// ====================== DOM ELEMENTS (cached once) ======================
const appContainer = document.getElementById('app-container');
const mqttDot      = document.getElementById('mqtt-dot');
const mqttStatus   = document.getElementById('mqtt-status');
const deviceDot    = document.getElementById('device-dot');
const deviceStatus = document.getElementById('device-status');
const overallBadge = document.getElementById('overall-badge');
const lastUpdateEl = document.getElementById('last-update');
const alertModal   = document.getElementById('alert-modal');
const alertMessage = document.getElementById('alert-message');
const alertDismiss = document.getElementById('alert-dismiss');

const valElements = {
  temperature: document.getElementById('val-temperature'),
  humidity:    document.getElementById('val-humidity'),
  pressure:    document.getElementById('val-pressure'),
  gas:         document.getElementById('val-gas')
};
const badgeElements = {
  temperature: document.getElementById('badge-temperature'),
  humidity:    document.getElementById('badge-humidity'),
  pressure:    document.getElementById('badge-pressure'),
  gas:         document.getElementById('badge-gas')
};
const cardElements = {
  temperature: document.getElementById('card-temperature'),
  humidity:    document.getElementById('card-humidity'),
  pressure:    document.getElementById('card-pressure'),
  gas:         document.getElementById('card-gas')
};
const tabButtons = {
  temperature: document.getElementById('tab-temperature'),
  humidity:    document.getElementById('tab-humidity'),
  pressure:    document.getElementById('tab-pressure'),
  gas:         document.getElementById('tab-gas')
};

// ====================== CHART ======================
let chart = null;

function initChart() {
  const ctx = document.getElementById('historyChart').getContext('2d');

  // Literal font string — canvas cannot resolve CSS variables
  Chart.defaults.font.family = "'JetBrains Mono', 'SF Mono', Consolas, monospace";
  Chart.defaults.font.size   = 11;
  Chart.defaults.color       = '#5a7a63'; // --ink-dim literal

  chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: SENSORS[activeMetric].label,
        data: [],
        borderColor: SENSORS[activeMetric].color,
        backgroundColor: function(context) {
          const chartArea = context.chart.chartArea;
          if (!chartArea) return 'transparent';
          const g = context.chart.ctx.createLinearGradient(0, chartArea.top, 0, chartArea.bottom);
          g.addColorStop(0, SENSORS[activeMetric].bgColorGrad);
          g.addColorStop(1, 'rgba(0,0,0,0.01)');
          return g;
        },
        borderWidth: 2,
        fill: true,
        tension: 0.3,
        pointRadius: 0,
        pointHoverRadius: 4,
        pointHoverBackgroundColor: SENSORS[activeMetric].color,
        pointHoverBorderColor: '#080e0a', // --bg literal (CSS vars not readable by canvas)
        pointHoverBorderWidth: 2
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false, // disable all animation globally for max perf
      interaction: {
        mode: 'index',
        intersect: false
      },
      plugins: {
        legend: { display: false },
        tooltip: {
          enabled: true,
          backgroundColor: '#162019', // --panel-2 literal
          borderColor:     '#26402b', // --border-2 literal
          borderWidth:     1,
          cornerRadius:    0,
          titleColor:      '#5a7a63', // --ink-dim literal
          bodyColor:       '#dff0e5', // --ink literal
          bodyFont: { weight: 'bold', family: "'JetBrains Mono', monospace" },
          callbacks: {
            label: function(context) {
              const sensor = SENSORS[activeMetric];
              const val = context.dataset.data[context.dataIndex];
              if (val === null || val === undefined || isNaN(val)) {
                return ` ${sensor.label}: --`;
              }
              const decimals = sensor.unit === '' ? 0 : 1;
              return ` ${sensor.label}: ${Number(val).toFixed(decimals)}${sensor.unit ? ' ' + sensor.unit : ''}`;
            }
          }
        }
      },
      scales: {
        x: {
          grid: { color: 'rgba(255,255,255,0.04)', drawBorder: false },
          ticks: { maxRotation: 0, autoSkip: true, maxTicksLimit: 6, color: '#5a7a63' }
        },
        y: {
          grid: { color: 'rgba(255,255,255,0.04)', drawBorder: false },
          ticks: {
            color: '#5a7a63',
            callback: function(value) {
              const unit = SENSORS[activeMetric].unit;
              return unit ? value + ' ' + unit : value;
            }
          }
        }
      }
    }
  });
}

function updateChartData() {
  if (!chart) return;
  const s = SENSORS[activeMetric];
  chart.data.labels                              = history.labels;
  chart.data.datasets[0].label                   = s.label;
  chart.data.datasets[0].data                    = history[activeMetric];
  chart.data.datasets[0].borderColor             = s.color;
  chart.data.datasets[0].pointHoverBackgroundColor = s.color;
  chart.update('none');
}

function selectMetric(metric) {
  if (activeMetric === metric) return;
  // Single-pass toggle — classList.toggle(name, force) is more efficient
  const keys = Object.keys(cardElements);
  for (let i = 0; i < keys.length; i++) {
    const isActive = keys[i] === metric;
    cardElements[keys[i]].classList.toggle('active', isActive);
    tabButtons[keys[i]].classList.toggle('active', isActive);
  }
  activeMetric = metric;
  updateChartData();
}

Object.keys(cardElements).forEach(k => cardElements[k].addEventListener('click', () => selectMetric(k)));
Object.keys(tabButtons).forEach(k => tabButtons[k].addEventListener('click', () => selectMetric(k)));

// ====================== HISTORY PUSH (hoisted — not recreated per message) ======================
function pushHistory(arr, val) {
  arr.push(val);
  if (arr.length > CONFIG.maxHistoryPoints) arr.shift();
}

// ====================== DATA NORMALIZATION ======================
function normalize(raw) {
  return {
    temperature:     raw.t  ?? raw.temperature  ?? null,
    humidity:        raw.h  ?? raw.humidity      ?? null,
    pressure:        raw.p  ?? raw.pressure      ?? null,
    gas:             raw.g  ?? raw.gas_adc       ?? null,
    temp_status:     raw.ts ?? raw.temp_status     ?? 'NORMAL',
    humidity_status: raw.hs ?? raw.humidity_status ?? 'NORMAL',
    pressure_status: raw.ps ?? raw.pressure_status ?? 'NORMAL',
    gas_status:      raw.gs ?? raw.gas_status      ?? 'NORMAL',
    overall_status:  raw.os ?? raw.overall_status  ?? 'NORMAL',
    timestamp:       raw.ms ?? raw.timestamp       ?? Date.now()
  };
}

// ====================== DEVICE ONLINE/OFFLINE ======================
function updateDeviceOnline(status) {
  if (deviceOnline === status) return; // Skip DOM write if unchanged
  deviceOnline = status;
  if (status) {
    deviceDot.className    = 'dot online';
    deviceStatus.textContent = 'ONLINE';
  } else {
    deviceDot.className    = 'dot offline';
    deviceStatus.textContent = 'OFFLINE';
    valElements.temperature.textContent = '--';
    valElements.humidity.textContent    = '--';
    valElements.pressure.textContent    = '--';
    valElements.gas.textContent         = '--';
  }
}

function markHeartbeat() {
  updateDeviceOnline(true);
  clearTimeout(heartbeatTimer);
  heartbeatTimer = setTimeout(() => updateDeviceOnline(false), CONFIG.heartbeatTimeout);
}

// ====================== UPDATE UI ======================
function updateUI(data, timeLabel) {
  const overall = data.overall_status;

  // Only update badge if changed (avoid unnecessary reflow)
  if (overallBadge.textContent !== overall) {
    overallBadge.textContent = overall;
    overallBadge.className   = `overall-badge ${overall}`;
  }

  appContainer.classList.toggle('danger-mode', overall === 'DANGER');

  // Sensor values — direct property access, no loop
  valElements.temperature.textContent = typeof data.temperature === 'number' ? data.temperature.toFixed(1) : '--';
  valElements.humidity.textContent    = typeof data.humidity    === 'number' ? data.humidity.toFixed(1)    : '--';
  valElements.pressure.textContent    = typeof data.pressure    === 'number' ? Math.round(data.pressure)   : '--';
  valElements.gas.textContent         = typeof data.gas         === 'number' ? data.gas                    : '--';

  // Card + badge status — for..in is faster than forEach for plain objects
  const statMap = {
    temperature: data.temp_status     || 'NORMAL',
    humidity:    data.humidity_status || 'NORMAL',
    pressure:    data.pressure_status || 'NORMAL',
    gas:         data.gas_status      || 'NORMAL'
  };
  for (const k in statMap) {
    const st = statMap[k];
    cardElements[k].className   = `card ${st}${activeMetric === k ? ' active' : ''}`;
    badgeElements[k].className  = `card-badge ${st}`;
    badgeElements[k].textContent = st;
  }

  lastUpdateEl.textContent = 'Aktual: ' + timeLabel;

  // Reactive alert: tampil jika DANGER, hilang segera jika sudah aman
  // Hanya tampil ulang jika bukan sedang di-dismiss manual oleh user
  if (overall === 'DANGER') {
    if (!alertModal.dataset.dismissed) {
      if (!alertMessage.dataset.custom) {
        alertMessage.textContent = 'Status DANGER terdeteksi! Periksa ruangan segera.';
      }
      alertModal.classList.add('visible');
    }
  } else {
    // Kondisi sudah aman — reset dismiss flag dan tutup modal
    delete alertModal.dataset.dismissed;
    delete alertMessage.dataset.custom;
    alertModal.classList.remove('visible');
  }
}

// ====================== MQTT ======================
function connectMQTT() {
  const brokerUrl = `wss://${CONFIG.brokerHost}:${CONFIG.brokerPort}/mqtt`;
  const clientId  = 'dash-vanilla-' + Math.random().toString(16).substring(2, 10);

  mqttDot.className      = 'dot connecting';
  mqttStatus.textContent = 'CONNECTING...';

  const client = mqtt.connect(brokerUrl, {
    username:        CONFIG.mqttUser,
    password:        CONFIG.mqttPass,
    clientId,
    clean:           true,
    protocolVersion: 4,
    reconnectPeriod: 3000,
    connectTimeout:  15000,
    keepalive:       60
  });

  client.on('connect', () => {
    mqttDot.className      = 'dot online';
    mqttStatus.textContent = 'ONLINE';
    client.subscribe([CONFIG.topicData, CONFIG.topicAlert, CONFIG.topicHeartbeat], err => {
      if (err) console.error('[MQTT] Subscribe error:', err);
    });
  });

  client.on('reconnect', () => {
    mqttDot.className      = 'dot connecting';
    mqttStatus.textContent = 'RECONNECTING...';
  });

  const setOffline = () => {
    mqttDot.className      = 'dot offline';
    mqttStatus.textContent = 'OFFLINE';
  };
  client.on('offline', setOffline);
  client.on('close',   setOffline);

  client.on('message', (topic, payload, packet) => {
    let raw;
    try { raw = JSON.parse(payload.toString()); }
    catch { return; }

    if (topic === CONFIG.topicData) {
      markHeartbeat();
      sensorData = normalize(raw);

      const timeLabel = new Date().toLocaleTimeString('id-ID', {
        hour: '2-digit', minute: '2-digit', second: '2-digit'
      });

      updateUI(sensorData, timeLabel);

      pushHistory(history.labels,      timeLabel);
      pushHistory(history.temperature, sensorData.temperature);
      pushHistory(history.humidity,    sensorData.humidity);
      pushHistory(history.pressure,    sensorData.pressure);
      pushHistory(history.gas,         sensorData.gas);

      updateChartData();

    } else if (topic === CONFIG.topicAlert) {
      if (packet?.retain) return; // Ignore stale retained messages
      // Tampilkan pesan dari ESP32 jika ada, dan tandai sebagai custom
      const msg = raw.message || 'Kondisi DANGER terdeteksi! Periksa ruangan segera.';
      alertMessage.textContent = msg;
      alertMessage.dataset.custom = '1';
      delete alertModal.dataset.dismissed;
      alertModal.classList.add('visible');

    } else if (topic === CONFIG.topicHeartbeat) {
      markHeartbeat();
    }
  });
}

// ====================== ENTRY POINT ======================
document.addEventListener('DOMContentLoaded', () => {
  initChart();
  connectMQTT();

  // Dismiss button: tandai dismissed agar modal tidak muncul lagi
  // sampai kondisi DANGER benar-benar selesai (reset otomatis saat status kembali aman)
  alertDismiss.addEventListener('click', () => {
    alertModal.dataset.dismissed = '1';
    alertModal.classList.remove('visible');
  });
});
