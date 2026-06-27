import PropTypes from 'prop-types';
import { useRef, useEffect } from 'react';

function formatValue(value, unit) {
  if (value == null) return '--';
  if (unit === '' || unit == null) return String(Math.round(value));
  if (unit === '°C' || unit === '%') return value.toFixed(1);
  if (unit === 'hPa') return value.toFixed(1);
  return String(value);
}

export default function SensorCard({ label, value, unit, status, icon, isLoading, onClick }) {
  const valueRef = useRef(null);
  const prevStatus = useRef(status);

  // Brief flash animation when status changes
  useEffect(() => {
    if (prevStatus.current !== status && valueRef.current) {
      valueRef.current.animate(
        [{ opacity: 0.3, transform: 'scale(0.95)' }, { opacity: 1, transform: 'scale(1)' }],
        { duration: 300, easing: 'ease-out' }
      );
    }
    prevStatus.current = status;
  }, [status]);

  return (
    <div 
      className={`card ${isLoading ? 'NORMAL' : status} ${isLoading ? 'card-skeleton' : ''}`}
      onClick={isLoading ? undefined : onClick}
    >
      <span className="card-icon">{icon}</span>
      <div className="card-label">{label}</div>

      <div ref={valueRef} className={`card-value ${isLoading ? '' : status}`}>
        {isLoading
          ? <span className="skeleton" style={{ width: 80, height: 36, display: 'block', borderRadius: 0 }} />
          : <>
              {formatValue(value, unit)}
              {unit && <span className="card-unit">{unit}</span>}
            </>
        }
      </div>

      {isLoading
        ? <span className="skeleton" style={{ width: 60, height: 20, display: 'block', marginTop: 12, borderRadius: 0 }} />
        : <span className={`card-badge ${status}`}>{status}</span>
      }
    </div>
  );
}

SensorCard.propTypes = {
  label:     PropTypes.string.isRequired,
  value:     PropTypes.number,
  unit:      PropTypes.string,
  status:    PropTypes.oneOf(['NORMAL', 'WARNING', 'DANGER']),
  icon:      PropTypes.node,
  isLoading: PropTypes.bool,
  onClick:   PropTypes.func,
};

SensorCard.defaultProps = {
  status:    'NORMAL',
  isLoading: false,
};
