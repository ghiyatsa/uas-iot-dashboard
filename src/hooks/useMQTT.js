import { useState, useEffect, useRef, useCallback } from 'react';
import mqtt from 'mqtt';

// ── HiveMQ Cloud config ─────────────────────────────────────────────────────
// WebSocket over TLS: port 8884, path /mqtt
// Pastikan kredensial ini sama persis dengan yang ada di config.h firmware
const BROKER_HOST = import.meta.env.VITE_MQTT_HOST;
const BROKER_PORT = Number(import.meta.env.VITE_MQTT_PORT) || 8884;
const MQTT_USER   = import.meta.env.VITE_MQTT_USER;
const MQTT_PASS   = import.meta.env.VITE_MQTT_PASS;

const TOPIC_DATA      = 'iot/room-safety/data';
const TOPIC_ALERT     = 'iot/room-safety/alert';
const TOPIC_HEARTBEAT = 'iot/room-safety/heartbeat';

const HEARTBEAT_TIMEOUT = 30_000; // 30s tanpa data → device OFFLINE
const MAX_POINTS        = 60;     // simpan 60 titik terakhir di chart

// Normalize payload: dukung key pendek (firmware v2: t,h,p,...) dan key panjang (v1)
function normalize(raw) {
  return {
    temperature:     raw.t  ?? raw.temperature,
    humidity:        raw.h  ?? raw.humidity,
    pressure:        raw.p  ?? raw.pressure,
    gas_adc:         raw.g  ?? raw.gas_adc,
    gas_digital:     raw.gd ?? raw.gas_digital,
    flame_detected:  raw.fd ?? raw.flame_detected ?? false,
    temp_status:     raw.ts ?? raw.temp_status     ?? 'NORMAL',
    humidity_status: raw.hs ?? raw.humidity_status ?? 'NORMAL',
    pressure_status: raw.ps ?? raw.pressure_status ?? 'NORMAL',
    gas_status:      raw.gs ?? raw.gas_status      ?? 'NORMAL',
    flame_status:    raw.fs ?? raw.flame_status    ?? 'NORMAL',
    overall_status:  raw.os ?? raw.overall_status  ?? 'NORMAL',
    timestamp:       Date.now(),
  };
}

export function useMQTT() {
  const [connected,    setConnected]    = useState(false);
  const [deviceOnline, setDeviceOnline] = useState(false);
  const [sensorData,   setSensorData]   = useState(null);
  const [alert,        setAlert]        = useState(null);
  const [history,      setHistory]      = useState({
    labels: [], temperature: [], humidity: [], pressure: [], gas: [],
  });
  const [dangerLogs,   setDangerLogs]   = useState(() => {
    try {
      return JSON.parse(localStorage.getItem('danger_logs') || '[]');
    } catch {
      return [];
    }
  });

  const clientRef      = useRef(null);
  const heartbeatTimer = useRef(null);
  const alertTimer     = useRef(null);

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

  useEffect(() => {
    if (!BROKER_HOST) {
      console.error(
        '[MQTT] VITE_MQTT_HOST is undefined. If you are deploying to Netlify, please configure VITE_MQTT_HOST, VITE_MQTT_PORT, VITE_MQTT_USER, and VITE_MQTT_PASS in Netlify Site Settings > Environment variables, and trigger a redeploy.'
      );
      return;
    }

    // MQTT.js v5: gunakan format URL lengkap wss://host:port/path
    const brokerUrl = `wss://${BROKER_HOST}:${BROKER_PORT}/mqtt`;
    const clientId  = 'react-dash-' + Math.random().toString(16).slice(2, 10);

    console.log(`[MQTT] Connecting to ${brokerUrl} as ${clientId}`);

    const client = mqtt.connect(brokerUrl, {
      username:        MQTT_USER,
      password:        MQTT_PASS,
      clientId,
      clean:           true,
      protocolVersion: 4,         // MQTT 3.1.1 — sama dengan PubSubClient ESP32
      reconnectPeriod: 3000,      // retry setiap 3 detik jika putus
      connectTimeout:  15_000,    // timeout 15 detik
      keepalive:       60,        // kirim PINGREQ setiap 60 detik
    });

    clientRef.current = client;

    client.on('connect', (connack) => {
      console.log('[MQTT] Connected!', connack);
      setConnected(true);

      // Subscribe ke semua topic yang diperlukan
      const topics = [TOPIC_DATA, TOPIC_ALERT, TOPIC_HEARTBEAT];
      client.subscribe(topics, { qos: 0 }, (err, granted) => {
        if (err) {
          console.error('[MQTT] Subscribe error:', err);
        } else {
          console.log('[MQTT] Subscribed to:', granted.map(g => g.topic).join(', '));
        }
      });
    });

    client.on('reconnect', () => {
      console.log('[MQTT] Reconnecting...');
      setConnected(false);
    });

    client.on('error', (err) => {
      console.error('[MQTT] Error:', err.message ?? err);
      setConnected(false);
    });

    client.on('offline', () => {
      console.warn('[MQTT] Client offline');
      setConnected(false);
    });

    client.on('close', () => {
      console.warn('[MQTT] Connection closed');
      setConnected(false);
    });

    client.on('message', (topic, payload, packet) => {
      const raw_str = payload.toString();
      console.log(`[MQTT] ← ${topic}: ${raw_str}`);

      let raw;
      try {
        raw = JSON.parse(raw_str);
      } catch (e) {
        console.error('[MQTT] JSON parse error:', e, raw_str);
        return;
      }

      if (topic === TOPIC_DATA) {
        markHeartbeat();
        const d = normalize(raw);
        setSensorData(d);

        // Jika status overall adalah DANGER, simpan log kejadian
        if (d.overall_status === 'DANGER') {
          setDangerLogs(prev => {
            const now = new Date();
            const timeStr = now.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false });
            const dateStr = now.toLocaleDateString(undefined, { day: '2-digit', month: 'short' });
            
            // Cegah spam log berturut-turut jika log terakhir memiliki tipe bahaya yang sama dalam 5 detik terakhir
            if (prev.length > 0) {
              const lastLog = prev[0];
              const diff = Date.now() - new Date(lastLog.rawTimestamp).getTime();
              if (diff < 5000 && lastLog.gas === d.gas_digital && lastLog.flame === d.flame_detected) {
                return prev;
              }
            }

            const newLog = {
              id: Math.random().toString(36).slice(2, 9),
              time: timeStr,
              date: dateStr,
              rawTimestamp: Date.now(),
              gas: d.gas_digital,
              flame: d.flame_detected,
              temp: d.temperature,
              gasVal: d.gas_adc,
            };
            const updated = [newLog, ...prev].slice(0, 50); // Simpan max 50 logs (terbaru di atas)
            localStorage.setItem('danger_logs', JSON.stringify(updated));
            return updated;
          });
        }

        const label = new Date().toLocaleTimeString(undefined, {
          hour: '2-digit', minute: '2-digit', second: '2-digit',
          hour12: false
        });
        setHistory(prev => {
          const push = (arr, val) => [...arr.slice(-(MAX_POINTS - 1)), val];
          return {
            labels:      push(prev.labels,      label),
            temperature: push(prev.temperature, d.temperature),
            humidity:    push(prev.humidity,    d.humidity),
            pressure:    push(prev.pressure,    d.pressure),
            gas:         push(prev.gas,         d.gas_adc),
          };
        });

      } else if (topic === TOPIC_ALERT) {
        // Abaikan jika ini adalah pesan lama (retained) yang tersimpan di broker saat subscribe
        if (packet && packet.retain) {
          console.log('[MQTT] Stale retained alert ignored');
          return;
        }
        console.warn('[MQTT] DANGER ALERT:', raw);
        setAlert(raw);
        clearTimeout(alertTimer.current);
        alertTimer.current = setTimeout(() => setAlert(null), 20_000);

      } else if (topic === TOPIC_HEARTBEAT) {
        console.log('[MQTT] Heartbeat received');
        markHeartbeat();
      }
    });

    return () => {
      console.log('[MQTT] Cleanup — closing client');
      clearTimeout(heartbeatTimer.current);
      clearTimeout(alertTimer.current);
      client.end(true);
    };
  }, [markHeartbeat]);

  return { connected, deviceOnline, sensorData, alert, history, dangerLogs, clearDangerLogs };
}

