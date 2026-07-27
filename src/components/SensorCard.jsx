import PropTypes from 'prop-types';
import { useRef, useEffect, memo } from 'react';

function formatValue(value, unit) {
  if (value == null) return '--';
  if (unit === '' || unit == null) return String(Math.round(value));
  if (unit === '°C' || unit === '%') return value.toFixed(1);
  if (unit === 'hPa') return String(Math.round(value));
  return String(value);
}

function SensorCard({ label, value, unit, status, icon, isLoading, isActive, description, onClick }) {
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

  const displayStatus = {
    NORMAL: 'AMAN',
    WARNING: 'WASPADA',
    DANGER: 'BAHAYA'
  };

  const handleKeyDown = (e) => {
    if (isLoading || !onClick) return;
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      onClick();
    }
  };

  return (
    <div 
      className={`card ${isLoading ? 'NORMAL' : status} ${isLoading ? 'card-skeleton' : ''} ${isActive ? 'active' : ''}`}
      onClick={isLoading ? undefined : onClick}
      onKeyDown={handleKeyDown}
      tabIndex={isLoading ? -1 : 0}
      role={onClick ? 'button' : undefined}
      aria-selected={onClick ? isActive : undefined}
      title={isLoading ? undefined : description}
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
        : <span className={`card-badge ${status}`}>{displayStatus[status] || status}</span>
      }
    </div>
  );
}

export default memo(SensorCard);

SensorCard.propTypes = {
  label:       PropTypes.string.isRequired,
  value:       PropTypes.number,
  unit:        PropTypes.string,
  status:      PropTypes.oneOf(['NORMAL', 'WARNING', 'DANGER']),
  icon:        PropTypes.node,
  isLoading:   PropTypes.bool,
  isActive:    PropTypes.bool,
  description: PropTypes.string,
  onClick:     PropTypes.func,
};

SensorCard.defaultProps = {
  status:      'NORMAL',
  isLoading:   false,
  isActive:    false,
  description: '',
};
