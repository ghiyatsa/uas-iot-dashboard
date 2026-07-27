import PropTypes from "prop-types";
import { ShieldIcon } from "./Icons";

export default function Header({
  brokerConnected,
  deviceOnline,
  overallStatus,
  lastData,
  sensorError,
}) {
  // Gunakan NTP timestamp jika tersedia, fallback ke Date.now()
  const lastUpdate = lastData
    ? lastData.ts_unix
      ? new Date(lastData.ts_unix * 1000).toLocaleTimeString(undefined, { hour12: false })
      : new Date(lastData.timestamp).toLocaleTimeString(undefined, { hour12: false })
    : null;

  const displayStatus = {
    NORMAL: 'SAFE',
    WARNING: 'WARNING',
    DANGER: 'DANGER'
  };

  return (
    <header className="header">
      <div className="header-left">
        <h1>
          <ShieldIcon /> Safety Dashboard
        </h1>
        <div className="sub">
          Fire & Gas Detection &middot; MQTT &middot; HiveMQ Cloud
        </div>
      </div>

      <div className="header-right">
        {/* Broker connection */}
        <div className="pill">
          <span
            className={`dot ${brokerConnected ? "online" : "connecting"}`}
          />
          <span>MQTT: {brokerConnected ? "CONNECTED" : "CONNECTING..."}</span>
        </div>

        {/* Device status */}
        <div className="pill">
          <span className={`dot ${deviceOnline ? "online" : "offline"}`} />
          <span>ESP32: {deviceOnline ? "ONLINE" : "OFFLINE"}</span>
        </div>

        {/* Overall status */}
        <span className={`overall-badge ${overallStatus}`}>
          {displayStatus[overallStatus] || overallStatus}
        </span>

        {/* Sensor error */}
        {sensorError && (
          <span className="pill" style={{ borderColor: 'var(--warning)', color: 'var(--warning)' }}>
            SENSOR ERROR
          </span>
        )}
        {/* Last update */}
        {lastUpdate && <span className="last-update">{lastUpdate}</span>}
      </div>
    </header>
  );
}

Header.propTypes = {
  brokerConnected: PropTypes.bool.isRequired,
  deviceOnline:    PropTypes.bool.isRequired,
  overallStatus:   PropTypes.string.isRequired,
  lastData:        PropTypes.object,
  sensorError:     PropTypes.bool,
};
