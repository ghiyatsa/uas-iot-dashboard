import { useState } from "react";
import { useMQTT } from "./hooks/useMQTT";
import Header from "./components/Header";
import SensorCard from "./components/SensorCard";
import HistoryChart from "./components/HistoryChart";
import AlertBanner from "./components/AlertBanner";
import {
  ThermometerIcon,
  DropletIcon,
  GaugeIcon,
  CloudIcon,
  HeartIcon,
} from "./components/Icons";
import "./App.css";

const SENSORS = [
  {
    key: "temperature",
    label: "Suhu",
    unit: "°C",
    statusKey: "temp_status",
    icon: <ThermometerIcon />,
  },
  {
    key: "humidity",
    label: "Kelembapan",
    unit: "%",
    statusKey: "humidity_status",
    icon: <DropletIcon />,
  },
  {
    key: "pressure",
    label: "Tekanan",
    unit: "hPa",
    statusKey: "pressure_status",
    icon: <GaugeIcon />,
  },
  {
    key: "gas_adc",
    label: "Gas (ADC)",
    unit: "",
    statusKey: "gas_status",
    icon: <CloudIcon />,
  },
];

export default function App() {
  const { connected, deviceOnline, sensorData, alert, history } = useMQTT();
  const overall = sensorData?.overall_status ?? "NORMAL";
  const [activeMetric, setActiveMetric] = useState("temperature");

  return (
    <div className={`app ${overall === "DANGER" ? "danger-mode" : ""}`}>
      <div className="wrap">
        <Header
          brokerConnected={connected}
          deviceOnline={deviceOnline}
          overallStatus={overall}
          lastData={sensorData}
        />

        <div className="hatched-divider" />

        {/* Alert banner — hanya muncul saat ada pesan DANGER dari topic alert */}
        {alert && (
          <>
            <AlertBanner alert={alert} />
            <div className="hatched-divider" />
          </>
        )}

        {/* 4 Sensor cards */}
        <div className="sensor-grid">
          {SENSORS.map((s) => (
            <SensorCard
              key={s.key}
              label={s.label}
              unit={s.unit}
              icon={s.icon}
              value={sensorData ? sensorData[s.key] : null}
              status={
                sensorData ? (sensorData[s.statusKey] ?? "NORMAL") : "NORMAL"
              }
              isLoading={!sensorData}
              onClick={() => setActiveMetric(s.key === "gas_adc" ? "gas" : s.key)}
            />
          ))}
        </div>

        <div className="hatched-divider" />

        {/* History chart */}
        <HistoryChart
          history={history}
          active={activeMetric}
          setActive={setActiveMetric}
        />

        <div className="hatched-divider" />

        <footer className="footer">
          Kelompok 2 &middot; Made with <HeartIcon /> &middot; 2026
        </footer>
      </div>
    </div>
  );
}
