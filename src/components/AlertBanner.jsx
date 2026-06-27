import PropTypes from 'prop-types';
import { SirenIcon } from './Icons';

export default function AlertBanner({ alert }) {
  const msg = alert?.message ?? 'Kondisi DANGER terdeteksi! Segera periksa ruangan.';
  const type = alert?.alert_type ?? 'UNKNOWN';

  // Tentukan warna/gaya visual berdasarkan tipe alert
  let bannerClass = 'alert-banner';
  if (type === 'FIRE') {
    bannerClass += ' alert-fire';
  } else if (type === 'GAS') {
    bannerClass += ' alert-gas';
  } else if (type === 'BOTH') {
    bannerClass += ' alert-both';
  }

  return (
    <div className={bannerClass} role="alert" aria-live="assertive">
      <span className="alert-icon"><SirenIcon /></span>
      <div className="alert-body">
        <div className="alert-title">⚠ {type} DANGER ALERT</div>
        <div className="alert-message">{msg}</div>
      </div>
      {alert?.gas_adc != null && (
        <span style={{
          fontFamily: 'var(--mono)',
          fontSize: '0.72rem',
          color: 'inherit',
          borderLeft: '1px solid rgba(255,255,255,0.3)',
          paddingLeft: '12px',
          whiteSpace: 'nowrap',
        }}>
          Gas ADC<br />
          <strong style={{ fontSize: '1.1rem' }}>{alert.gas_adc}</strong>
        </span>
      )}
    </div>
  );
}

AlertBanner.propTypes = {
  alert: PropTypes.shape({
    message: PropTypes.string,
    alert_type: PropTypes.string,
    gas_adc: PropTypes.number,
  }),
};
