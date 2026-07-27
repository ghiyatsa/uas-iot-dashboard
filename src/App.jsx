import { useState, useEffect } from "react";
import { useMQTT } from "./hooks/useMQTT";
import Header from "./components/Header";
import SensorCard from "./components/SensorCard";
import HistoryChart from "./components/HistoryChart";
import ControlPanel from "./components/ControlPanel";
import {
  ThermometerIcon,
  DropletIcon,
  GaugeIcon,
  CloudIcon,
  FlameIcon,
  CpuIcon,
  HeartIcon,
  SirenIcon,
} from "./components/Icons";
import "./App.css";

const SENSORS = [
  {
    key: "temperature",
    label: "Suhu (Temp)",
    unit: "°C",
    statusKey: "temp_status",
    icon: <ThermometerIcon />,
    description: "Suhu ruangan. Normal: 18°C - 30°C.",
  },
  {
    key: "humidity",
    label: "Kelembapan",
    unit: "%",
    statusKey: "humidity_status",
    icon: <DropletIcon />,
    description: "Kelembapan udara. Ideal: 40% - 60%.",
  },
  {
    key: "pressure",
    label: "Tekanan",
    unit: "hPa",
    statusKey: "pressure_status",
    icon: <GaugeIcon />,
    description: "Tekanan udara. Normal: ~1013 hPa.",
  },
  {
    key: "gas_adc",
    label: "Gas MQ-2",
    unit: "",
    statusKey: "gas_status",
    icon: <CloudIcon />,
    description: "Kadar gas & asap. Aman: < 400.",
  },
  {
    key: "flame_adc",
    label: "Flame Sensor",
    unit: "",
    statusKey: "flame_status",
    icon: <FlameIcon />,
    description: "Deteksi api. Nilai kecil = bahaya.",
  },
];

export default function App() {
  const {
    connected,
    deviceOnline,
    sensorData,
    history,
    dangerLogs,
    clearDangerLogs,
    publishCmd,
  } = useMQTT();

  const overall       = deviceOnline ? (sensorData?.overall_status ?? "NORMAL") : "NORMAL";
  const buzzerMuted   = deviceOnline ? (sensorData?.buzzer_muted   ?? false) : false;
  const sensorError   = deviceOnline ? (sensorData?.sensor_error   ?? false) : false;
  const [activeMetric, setActiveMetric] = useState("temperature");
  const [retryBusy, setRetryBusy] = useState(false);

  const handleSensorRetry = () => {
    if (retryBusy) return;
    setRetryBusy(true);
    publishCmd('test_led', '1');
    setTimeout(() => {
      setRetryBusy(false);
    }, 2000);
  };

  // Keyboard shortcuts (Keys 1-6 to switch active metrics)
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (document.activeElement.tagName === 'INPUT' || document.activeElement.tagName === 'TEXTAREA') return;

      const keyMap = {
        '1': 'temperature',
        '2': 'humidity',
        '3': 'pressure',
        '4': 'gas',
        '5': 'flame',
        '6': 'heap_free',
      };

      const metric = keyMap[e.key];
      if (metric) {
        e.preventDefault();
        setActiveMetric(metric);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const isFlameDanger = sensorData?.flame_status === "DANGER";
  const isGasDanger   = sensorData?.gas_status === "DANGER";
  const isTempDanger  = sensorData?.temp_status === "DANGER";

  const alertClass = isFlameDanger && isGasDanger
    ? "alert-both"
    : isFlameDanger
    ? "alert-fire"
    : isGasDanger
    ? "alert-gas"
    : isTempDanger
    ? "alert-temp"
    : "";

  return (
    <div className={`app ${overall === "DANGER" ? "danger-mode" : ""}`}>
      <div className="wrap">
        <Header
          brokerConnected={connected}
          deviceOnline={deviceOnline}
          overallStatus={overall}
          lastData={sensorData}
          sensorError={sensorError}
        />

        {/* Alert Banner */}
        {overall === "DANGER" && alertClass && (
          <div className={`alert-banner ${alertClass}`}>
            <span className="alert-icon"><SirenIcon /></span>
            <div className="alert-body">
              <div className="alert-title">
                {alertClass === "alert-both"
                  ? "KEBAKARAN & KEBOCORAN GAS TERDETEKSI"
                  : alertClass === "alert-fire"
                  ? "KEBAKARAN TERDETEKSI"
                  : alertClass === "alert-gas"
                  ? "KEBOCORAN GAS TERDETEKSI"
                  : "SUHU EKSTREM TERDETEKSI"}
              </div>
              <div className="alert-message">
                {alertClass === "alert-both"
                  ? "Segera evakuasi diri dan buka semua ventilasi udara!"
                  : alertClass === "alert-fire"
                  ? "Adanya indikasi api terdeteksi di ruangan!"
                  : alertClass === "alert-gas"
                  ? "Konsentrasi gas berbahaya terdeteksi melebihi batas aman!"
                  : "Suhu ruangan berada di tingkat bahaya!"}
              </div>
            </div>
          </div>
        )}

        {/* Sensor error notice */}
        {sensorError && (
          <div className="sensor-error-banner">
            <span>Sensor I2C gagal baca — periksa koneksi AHT20 / BMP280</span>
            <button
              onClick={handleSensorRetry}
              disabled={retryBusy || !deviceOnline}
              className="btn-sensor-retry"
              title={!deviceOnline ? 'Perangkat offline' : 'Kirim perintah uji sensor ke ESP32'}
            >
              {retryBusy ? "Menguji..." : "UJI SENSOR"}
            </button>
          </div>
        )}

        <div className="hatched-divider" />

        {/* 5 Sensor cards */}
        <div className="sensor-grid">
          {SENSORS.map((s) => {
            const metricKey = s.key === "gas_adc" ? "gas" : s.key === "flame_adc" ? "flame" : s.key;
            let val = deviceOnline && sensorData ? sensorData[s.key] : null;
            if (typeof val === 'number') {
              if (s.key === 'pressure') {
                val = Math.round(val);
              } else if (s.key === 'temperature' || s.key === 'humidity') {
                val = Number(val.toFixed(1));
              }
            }
            return (
              <SensorCard
                key={s.key}
                label={s.label}
                unit={s.unit}
                icon={s.icon}
                value={val}
                status={deviceOnline && sensorData ? (sensorData[s.statusKey] ?? "NORMAL") : "NORMAL"}
                isLoading={!deviceOnline || !sensorData}
                isActive={activeMetric === metricKey}
                description={s.description}
                onClick={() => setActiveMetric(metricKey)}
              />
            );
          })}
          {/* Card ke-6: Sisa RAM ESP32 */}
          <SensorCard
            key="heap_free"
            label="Sisa RAM (Heap)"
            unit=" KB"
            icon={<CpuIcon />}
            value={deviceOnline && sensorData?.heap_free != null ? Math.round(sensorData.heap_free / 1024) : null}
            status={
              !deviceOnline || sensorData?.heap_free == null
                ? "NORMAL"
                : sensorData.heap_free < 20000
                ? "DANGER"
                : sensorData.heap_free < 40000
                ? "WARNING"
                : "NORMAL"
            }
            isLoading={!deviceOnline || !sensorData}
            isActive={activeMetric === "heap_free"}
            description="Free RAM ESP32. Normal: > 20KB."
            onClick={() => setActiveMetric("heap_free")}
          />
        </div>

        <div className="hatched-divider" />

        {/* History chart */}
        <HistoryChart
          history={history}
          active={activeMetric}
          setActive={setActiveMetric}
          dangerLogs={dangerLogs}
          clearDangerLogs={clearDangerLogs}
        />

        <div className="hatched-divider" />

        {/* Control Panel */}
        <ControlPanel
          publishCmd={publishCmd}
          deviceOnline={deviceOnline}
          buzzerMuted={buzzerMuted}
        />

        <div className="hatched-divider" />

        <footer className="footer">
          Kelompok 2 &middot; Dibuat dengan <HeartIcon /> &middot; 2026
        </footer>
      </div>
    </div>
  );
}
