import { useState, useEffect, useRef, useCallback } from 'react';
import mqtt from 'mqtt';

// ── HiveMQ Cloud config ─────────────────────────────────────────────────────
const BROKER_HOST = import.meta.env.VITE_MQTT_HOST;
const BROKER_PORT = Number(import.meta.env.VITE_MQTT_PORT) || 8884;
const MQTT_USER   = import.meta.env.VITE_MQTT_USER;
const MQTT_PASS   = import.meta.env.VITE_MQTT_PASS;

const TOPIC_DATA      = 'iot/room-safety/data';
const TOPIC_ALERT     = 'iot/room-safety/alert';
const TOPIC_HEARTBEAT = 'iot/room-safety/heartbeat';
const TOPIC_CMD_MUTE  = 'iot/room-safety/cmd/mute';
const TOPIC_CMD_LED   = 'iot/room-safety/cmd/test_led';
const TOPIC_CMD_TG    = 'iot/room-safety/cmd/telegram';

const HEARTBEAT_TIMEOUT = 30_000;
const MAX_POINTS        = 60;

if (!BROKER_HOST) throw new Error('[MQTT] VITE_MQTT_HOST is not set in .env');

// Normalize payload: dukung key pendek (firmware baru) dan key panjang
function normalize(raw) {
  return {
    temperature:     raw.t   ?? raw.temperature,
    humidity:        raw.h   ?? raw.humidity,
    pressure:        raw.p   ?? raw.pressure,
    gas_adc:         raw.g   ?? raw.gas_adc,
    flame_adc:       raw.f   ?? raw.flame_adc,
    temp_status:     raw.ts  ?? raw.temp_status     ?? 'NORMAL',
    humidity_status: raw.hs  ?? raw.humidity_status ?? 'NORMAL',
    pressure_status: raw.ps  ?? raw.pressure_status ?? 'NORMAL',
    gas_status:      raw.gs  ?? raw.gas_status      ?? 'NORMAL',
    flame_status:    raw.fs  ?? raw.flame_status    ?? 'NORMAL',
    overall_status:  raw.os  ?? raw.overall_status  ?? 'NORMAL',
    sensor_error:    raw.se  ?? raw.sensor_error    ?? false,
    buzzer_muted:    raw.bm  ?? raw.buzzer_muted    ?? false,
    heap_free:       raw.hf  ?? raw.heap_free,
    ts_unix:         raw.ts_unix ?? null,
    timestamp:       Date.now(),
  };
}

// Request browser notification permission
function requestNotifPermission() {
  if ('Notification' in window && Notification.permission === 'default') {
    Notification.requestPermission();
  }
}

function sendBrowserNotif(title, body) {
  if ('Notification' in window && Notification.permission === 'granted') {
    new Notification(title, {
      body,
      icon: '/icon-192.png',
      tag: 'room-safety-danger',   // replace agar tidak spam
      requireInteraction: true,
    });
  }
}

export function useMQTT() {
  const [connected,    setConnected]    = useState(false);
  const [deviceOnline, setDeviceOnline] = useState(false);
  const [sensorData,   setSensorData]   = useState(null);
  const [alert,        setAlert]        = useState(null);
  // Flat array: [{ label, temperature, humidity, pressure, gas, flame }, ...]
  const [history, setHistory] = useState([]);
  const [dangerLogs, setDangerLogs] = useState(() => {
    try {
      return JSON.parse(localStorage.getItem('danger_logs') || '[]');
    } catch {
      return [];
    }
  });

  const clientRef      = useRef(null);
  const heartbeatTimer = useRef(null);
  const alertTimer     = useRef(null);
  const lastDangerNotif = useRef(0);

  const clearDangerLogs = useCallback(() => {
    localStorage.removeItem('danger_logs');
    setDangerLogs([]);
  }, []);

  const markHeartbeat = useCallback(() => {
    setDeviceOnline(true);
    clearTimeout(heartbeatTimer.current);
    heartbeatTimer.current = setTimeout(() => {
      console.log('[MQTT] Heartbeat timeout — device OFFLINE');
      setDeviceOnline(false);
    }, HEARTBEAT_TIMEOUT);
  }, []);

  // Publish command ke ESP32 via MQTT
  const publishCmd = useCallback((cmdType, value = '1') => {
    const client = clientRef.current;
    if (!client?.connected) {
      console.warn('[MQTT] Tidak bisa kirim cmd — client tidak terhubung');
      return false;
    }
    const topicMap = { mute: TOPIC_CMD_MUTE, test_led: TOPIC_CMD_LED, telegram: TOPIC_CMD_TG };
    const topic = topicMap[cmdType];
    if (!topic) return false;
    client.publish(topic, String(value), { qos: 1 });
    console.log(`[MQTT] → cmd: ${topic} = ${value}`);
    return true;
  }, []);

  useEffect(() => {
    requestNotifPermission();

    const brokerUrl = `wss://${BROKER_HOST}:${BROKER_PORT}/mqtt`;
    const clientId  = 'react-dash-' + Math.random().toString(16).slice(2, 10);

    console.log(`[MQTT] Connecting to ${brokerUrl} as ${clientId}`);

    const client = mqtt.connect(brokerUrl, {
      username:        MQTT_USER,
      password:        MQTT_PASS,
      clientId,
      clean:           true,
      protocolVersion: 4,
      reconnectPeriod: 3000,
      connectTimeout:  15_000,
      keepalive:       60,
    });

    clientRef.current = client;

    client.on('connect', (connack) => {
      console.log('[MQTT] Connected!', connack);
      setConnected(true);
      const topics = [TOPIC_DATA, TOPIC_ALERT, TOPIC_HEARTBEAT];
      client.subscribe(topics, { qos: 0 }, (err, granted) => {
        if (err) console.error('[MQTT] Subscribe error:', err);
        else console.log('[MQTT] Subscribed to:', granted.map(g => g.topic).join(', '));
      });
    });

    client.on('reconnect', () => { setConnected(false); });
    client.on('error',     (err) => { console.error('[MQTT] Error:', err.message ?? err); setConnected(false); });
    client.on('offline',   () => setConnected(false));
    client.on('close',     () => setConnected(false));

    client.on('message', (topic, payload, packet) => {
      const raw_str = payload.toString();

      let raw;
      try { raw = JSON.parse(raw_str); }
      catch (e) { console.error('[MQTT] JSON parse error:', e, raw_str); return; }

      if (topic === TOPIC_DATA) {
        markHeartbeat();
        const d = normalize(raw);
        setSensorData(d);

        // Browser notification saat DANGER (max 1× per 60 detik)
        if (d.overall_status === 'DANGER') {
          const now = Date.now();
          if (now - lastDangerNotif.current > 60_000) {
            lastDangerNotif.current = now;
            const flameD = d.flame_status === 'DANGER';
            const gasD   = d.gas_status   === 'DANGER';
            const what   = flameD && gasD ? 'KEBAKARAN & KEBOCORAN GAS TERDETEKSI' : flameD ? 'KEBAKARAN TERDETEKSI' : 'KEBOCORAN GAS TERDETEKSI';
            sendBrowserNotif('⚠️ Peringatan Keamanan Ruangan!', `${what} — Segera periksa ruangan!`);
          }
        }

        // Danger log
        if (d.overall_status === 'DANGER') {
          setDangerLogs(prev => {
            const now = new Date();
            const timeStr = now.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false });
            const dateStr = now.toLocaleDateString(undefined, { day: '2-digit', month: 'short' });

            if (prev.length > 0) {
              const lastLog = prev[0];
              const diff = Date.now() - new Date(lastLog.rawTimestamp).getTime();
              if (diff < 5000 && lastLog.gas_status === d.gas_status && lastLog.flame_status === d.flame_status) return prev;
            }

            const newLog = {
              id:           Math.random().toString(36).slice(2, 9),
              time:         timeStr,
              date:         dateStr,
              rawTimestamp: Date.now(),
              gas_status:   d.gas_status,
              flame_status: d.flame_status,
              temp:         d.temperature,
              gasVal:       d.gas_adc,
              flameVal:     d.flame_adc,
            };
            const updated = [newLog, ...prev].slice(0, 50);
            // async — jangan block main thread
            setTimeout(() => localStorage.setItem('danger_logs', JSON.stringify(updated)), 0);
            return updated;
          });
        }

        // History chart — flat array, 1 push per message
        const label = new Date().toLocaleTimeString(undefined, {
          hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false,
        });
        setHistory(prev => {
          const next = prev.length >= MAX_POINTS ? prev.slice(1) : prev;
          return [...next, {
            label,
            temperature: d.temperature,
            humidity:    d.humidity,
            pressure:    d.pressure,
            gas:         d.gas_adc,
            flame:       d.flame_adc,
            heap_free:   d.heap_free,
          }];
        });

      } else if (topic === TOPIC_ALERT) {
        if (packet?.retain) { console.log('[MQTT] Stale retained alert ignored'); return; }
        console.warn('[MQTT] DANGER ALERT:', raw);
        setAlert(raw);
        clearTimeout(alertTimer.current);
        alertTimer.current = setTimeout(() => setAlert(null), 20_000);

      } else if (topic === TOPIC_HEARTBEAT) {
        if (raw.status === 'offline') {
          console.log('[MQTT] Heartbeat indicates device OFFLINE');
          setDeviceOnline(false);
        } else {
          console.log('[MQTT] Heartbeat received, heap_free:', raw.heap_free);
          markHeartbeat();
          setSensorData(prev => prev ? { ...prev, heap_free: raw.heap_free } : { heap_free: raw.heap_free });
        }
      }
    });

    return () => {
      clearTimeout(heartbeatTimer.current);
      clearTimeout(alertTimer.current);
      client.end(true);
    };
  }, [markHeartbeat]);

  return { connected, deviceOnline, sensorData, alert, history, dangerLogs, clearDangerLogs, publishCmd };
}
