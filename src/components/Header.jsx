import PropTypes from "prop-types";
import { ShieldIcon } from "./Icons";

export default function Header({
  brokerConnected,
  deviceOnline,
  overallStatus,
  lastData,
}) {
  const lastUpdate = lastData
    ? new Date(lastData.timestamp).toLocaleTimeString(undefined, { hour12: false })
    : null;

  return (
    <header className="header">
      <div className="header-left">
        <h1>
          <ShieldIcon /> Room Safety Dashboard
        </h1>
        <div className="sub">
          IoT Environment Monitoring &middot; MQTT Push &middot; HiveMQ Cloud
        </div>
      </div>

      <div className="header-right">
        {/* Broker connection */}
        <div className="pill">
          <span
            className={`dot ${brokerConnected ? "online" : "connecting"}`}
          />
          <span>Broker: {brokerConnected ? "ONLINE" : "Connecting..."}</span>
        </div>

        {/* Device status */}
        <div className="pill">
          <span className={`dot ${deviceOnline ? "online" : "offline"}`} />
          <span>Device: {deviceOnline ? "ONLINE" : "OFFLINE"}</span>
        </div>

        {/* Overall status */}
        <span className={`overall-badge ${overallStatus}`}>
          {overallStatus}
        </span>

        {/* Last update */}
        {lastUpdate && <span className="last-update">{lastUpdate}</span>}
      </div>
    </header>
  );
}

Header.propTypes = {
  brokerConnected: PropTypes.bool.isRequired,
  deviceOnline: PropTypes.bool.isRequired,
  overallStatus: PropTypes.string.isRequired,
  lastData: PropTypes.object,
};
