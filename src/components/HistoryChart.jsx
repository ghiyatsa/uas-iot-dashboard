import { useMemo } from 'react';
import PropTypes from 'prop-types';
import {
  AreaChart, Area, XAxis, YAxis, CartesianGrid,
  Tooltip, ResponsiveContainer,
} from 'recharts';
import { ActivityIcon, RadioIcon } from './Icons';

const METRICS = [
  { key: 'temperature', label: 'Suhu',       unit: '°C',  color: '#f87171', histKey: 'temperature' },
  { key: 'humidity',    label: 'Kelembapan', unit: '%',   color: '#60a5fa', histKey: 'humidity'    },
  { key: 'pressure',   label: 'Tekanan',    unit: 'hPa', color: '#a78bfa', histKey: 'pressure'    },
  { key: 'gas',        label: 'Gas ADC',    unit: '',    color: '#34d399', histKey: 'gas'         },
];

// Custom tooltip component
function CustomTooltip({ active, payload, label, unit }) {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      background: 'var(--panel-2)',
      border: '1px solid var(--border-2)',
      borderRadius: 0,
      padding: '8px 12px',
      fontFamily: 'var(--mono)',
      fontSize: 12,
    }}>
      <div style={{ color: 'var(--ink-dim)', marginBottom: 4 }}>{label}</div>
      <div style={{ color: payload[0].color, fontWeight: 600 }}>
        {typeof payload[0].value === 'number'
          ? payload[0].value.toFixed(unit === '' ? 0 : 1)
          : payload[0].value}
        {unit && <span style={{ color: 'var(--ink-dim)', marginLeft: 3 }}>{unit}</span>}
      </div>
    </div>
  );
}

export default function HistoryChart({ history, active, setActive }) {
  const metric = METRICS.find(m => m.key === active);

  // Build recharts-compatible data array from parallel arrays
  const chartData = useMemo(() => {
    return history.labels.map((label, i) => ({
      label,
      value: history[metric.histKey]?.[i] ?? null,
    }));
  }, [history, metric]);

  const hasData = chartData.length > 0;

  // Compute Y-axis domain with some padding
  const domain = useMemo(() => {
    if (!hasData) return ['auto', 'auto'];
    const vals = chartData.map(d => d.value).filter(v => v != null);
    if (!vals.length) return ['auto', 'auto'];
    const min = Math.min(...vals);
    const max = Math.max(...vals);
    const pad = Math.max((max - min) * 0.15, 1);
    return [Math.floor(min - pad), Math.ceil(max + pad)];
  }, [chartData, hasData]);

  return (
    <div className="chart-panel">
      <div className="chart-header">
        <div className="chart-title">
          <ActivityIcon /> Riwayat Sensor (60 data terakhir)
        </div>
        <div className="chart-tabs">
          {METRICS.map(m => (
            <button
              key={m.key}
              className={`chart-tab ${active === m.key ? 'active' : ''}`}
              onClick={() => setActive(m.key)}
            >
              {m.label}
            </button>
          ))}
        </div>
      </div>

      {!hasData ? (
        <div className="chart-empty">
          <span className="chart-empty-icon"><RadioIcon /></span>
          <span>Menunggu data dari device...</span>
          <span style={{ fontSize: '0.65rem', color: 'var(--ink-dim)', marginTop: 2 }}>
            Data akan muncul setelah ESP32 terhubung dan mengirim payload
          </span>
        </div>
      ) : (
        <div className="chart-wrap">
          <ResponsiveContainer width="100%" height="100%">
            <AreaChart data={chartData} margin={{ top: 4, right: 8, bottom: 0, left: 0 }}>
              <defs>
                <linearGradient id={`grad-${metric.key}`} x1="0" y1="0" x2="0" y2="1">
                  <stop offset="5%"  stopColor={metric.color} stopOpacity={0.25} />
                  <stop offset="95%" stopColor={metric.color} stopOpacity={0.02} />
                </linearGradient>
              </defs>
              <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.04)" />
              <XAxis
                dataKey="label"
                tick={{ fill: 'var(--ink-dim)', fontSize: 10, fontFamily: 'var(--mono)' }}
                tickLine={false}
                axisLine={{ stroke: 'var(--border)' }}
                interval="preserveEnd"
                minTickGap={60}
              />
              <YAxis
                domain={domain}
                tick={{ fill: 'var(--ink-dim)', fontSize: 10, fontFamily: 'var(--mono)' }}
                tickLine={false}
                axisLine={false}
                width={60}
                tickFormatter={v => metric.unit === '' ? v : v.toFixed(0)}
              />
              <Tooltip content={<CustomTooltip unit={metric.unit} />} />
              <Area
                type="monotone"
                dataKey="value"
                stroke={metric.color}
                strokeWidth={2}
                fill={`url(#grad-${metric.key})`}
                dot={false}
                activeDot={{ r: 4, fill: metric.color, stroke: 'var(--bg)', strokeWidth: 2 }}
                isAnimationActive={false}
              />
            </AreaChart>
          </ResponsiveContainer>
        </div>
      )}
    </div>
  );
}

HistoryChart.propTypes = {
  history: PropTypes.shape({
    labels:      PropTypes.arrayOf(PropTypes.string).isRequired,
    temperature: PropTypes.arrayOf(PropTypes.number).isRequired,
    humidity:    PropTypes.arrayOf(PropTypes.number).isRequired,
    pressure:    PropTypes.arrayOf(PropTypes.number).isRequired,
    gas:         PropTypes.arrayOf(PropTypes.number).isRequired,
  }).isRequired,
  active:    PropTypes.string.isRequired,
  setActive: PropTypes.func.isRequired,
};
