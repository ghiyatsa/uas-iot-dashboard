import { useMemo, useCallback } from 'react';
import PropTypes from 'prop-types';
import {
  AreaChart, Area, XAxis, YAxis, CartesianGrid,
  Tooltip, ResponsiveContainer,
} from 'recharts';
import { ActivityIcon, RadioIcon, DownloadIcon } from './Icons';

const METRICS = [
  { key: 'temperature', label: 'Suhu',       unit: '°C',  color: '#f87171', histKey: 'temperature' },
  { key: 'humidity',    label: 'Kelembapan', unit: '%',   color: '#60a5fa', histKey: 'humidity'    },
  { key: 'pressure',    label: 'Tekanan',    unit: 'hPa', color: '#a78bfa', histKey: 'pressure'    },
  { key: 'gas',         label: 'Gas ADC',    unit: '',    color: '#34d399', histKey: 'gas'         },
  { key: 'flame',       label: 'Flame ADC',  unit: '',    color: '#fb923c', histKey: 'flame'       },
  { key: 'heap_free',   label: 'Sisa RAM',   unit: ' KB', color: '#06b6d4', histKey: 'heap_free'   },
  { key: 'danger_log',  label: 'Log Bahaya', unit: '',    color: '#ef4444', histKey: 'danger'      },
];

function CustomTooltip({ active, payload, label, unit }) {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      background: 'var(--panel-2)',
      backdropFilter: 'blur(8px)',
      border: '1px solid var(--border-2)',
      borderRadius: 'var(--radius)',
      padding: '10px 14px',
      fontFamily: 'var(--mono)',
      fontSize: 'var(--text-xs)',
      boxShadow: '0 10px 30px rgba(0,0,0,0.3)',
    }}>
      <div style={{ color: 'var(--ink-dim)', marginBottom: 4 }}>{label}</div>
      <div style={{ color: payload[0].color, fontWeight: 600 }}>
        {typeof payload[0].value === 'number'
          ? payload[0].value.toFixed(unit === '' ? 0 : 0)
          : payload[0].value}
        {unit && <span style={{ color: 'var(--ink-dim)', marginLeft: 3 }}>{unit}</span>}
      </div>
    </div>
  );
}

function downloadCSV(csv, filename) {
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement('a');
  a.href     = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export default function HistoryChart({ history, active, setActive, dangerLogs = [], clearDangerLogs }) {
  const metric = METRICS.find(m => m.key === active);

  const chartData = useMemo(() => {
    if (active === 'danger_log') return [];
    return history.map((d) => {
      let val = d[metric.histKey] ?? null;
      if (active === 'heap_free' && typeof val === 'number') {
        val = Math.round(val / 1024);
      }
      return {
        label: d.label,
        value: val,
      };
    });
  }, [history, metric, active]);

  const hasData = active === 'danger_log' ? true : chartData.length > 0;

  const domain = useMemo(() => {
    if (active === 'danger_log' || !hasData) return ['auto', 'auto'];
    const vals = chartData.map(d => d.value).filter(v => v != null);
    if (!vals.length) return ['auto', 'auto'];
    const min = vals.reduce((a, b) => Math.min(a, b), Infinity);
    const max = vals.reduce((a, b) => Math.max(a, b), -Infinity);
    const pad = Math.max((max - min) * 0.15, 1);
    return [Math.floor(min - pad), Math.ceil(max + pad)];
  }, [chartData, hasData, active]);

  // Export history chart data sebagai CSV
  const exportCSV = useCallback(() => {
    if (active === 'danger_log') {
      // Export danger log
      const header = 'Tanggal,Waktu,Gas Status,Flame Status,Gas ADC,Flame ADC,Suhu';
      const rows = dangerLogs.map(l =>
        `${l.date},${l.time},${l.gas_status ?? ''},${l.flame_status ?? ''},${l.gasVal ?? ''},${l.flameVal ?? ''},${l.temp?.toFixed(1) ?? ''}`
      );
      const csv = [header, ...rows].join('\n');
      downloadCSV(csv, 'danger_log.csv');
    } else {
      const header = `Waktu,${metric.label} (${metric.unit || 'ADC'})`;
      const rows = chartData.map(d => `${d.label},${d.value ?? ''}`);
      const csv = [header, ...rows].join('\n');
      downloadCSV(csv, `history_${metric.key}.csv`);
    }
  }, [active, chartData, dangerLogs, metric]);
  return (
    <div className="chart-panel">
      <div className="chart-header">
        <div className="chart-title">
          <ActivityIcon /> {active === 'danger_log' ? 'Log Riwayat Bahaya' : 'Riwayat Sensor (60 data terakhir)'}
        </div>
        <div className="chart-controls">
          <button className="btn-export" onClick={exportCSV} title="Export CSV">
            <DownloadIcon /> CSV
          </button>
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
      </div>

      {active === 'danger_log' ? (
        <div style={{ maxHeight: '260px', overflowY: 'auto', fontFamily: 'var(--mono)', fontSize: '0.8rem' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
            <span style={{ color: 'var(--ink-dim)' }}>Maks. 50 riwayat bahaya terakhir</span>
            {dangerLogs.length > 0 && (
              <button
                onClick={clearDangerLogs}
                style={{ background: 'none', border: '1px solid var(--border)', color: 'var(--danger)', padding: '2px 8px', cursor: 'pointer' }}
              >
                Hapus Log
              </button>
            )}
          </div>
          {dangerLogs.length === 0 ? (
            <div className="chart-empty">
              <span>Tidak ada riwayat bahaya tercatat.</span>
            </div>
          ) : (
            <div className="table-responsive">
              <table style={{ width: '100%', borderCollapse: 'collapse', textAlign: 'left', minWidth: '550px' }}>
                <thead>
                  <tr style={{ borderBottom: '1px solid var(--border-2)', color: 'var(--ink-dim)' }}>
                    <th style={{ padding: '8px' }}>Tanggal</th>
                    <th style={{ padding: '8px' }}>Waktu</th>
                    <th style={{ padding: '8px' }}>Gas</th>
                    <th style={{ padding: '8px' }}>Flame</th>
                    <th style={{ padding: '8px' }}>Gas ADC</th>
                    <th style={{ padding: '8px' }}>Flame ADC</th>
                    <th style={{ padding: '8px' }}>Suhu</th>
                  </tr>
                </thead>
                <tbody>
                  {dangerLogs.map(log => (
                    <tr key={log.id} style={{ borderBottom: '1px solid var(--border)', color: 'var(--ink)' }}>
                      <td style={{ padding: '8px' }}>{log.date}</td>
                      <td style={{ padding: '8px' }}>{log.time}</td>
                      <td style={{ padding: '8px', color: log.gas_status === 'DANGER' ? 'var(--danger)' : log.gas_status === 'WARNING' ? 'var(--warning)' : 'var(--normal)' }}>
                        {log.gas_status === 'DANGER' ? 'BAHAYA' : log.gas_status === 'WARNING' ? 'WASPADA' : 'AMAN'}
                      </td>
                      <td style={{ padding: '8px', color: log.flame_status === 'DANGER' ? 'var(--danger)' : log.flame_status === 'WARNING' ? 'var(--warning)' : 'var(--normal)' }}>
                        {log.flame_status === 'DANGER' ? 'BAHAYA' : log.flame_status === 'WARNING' ? 'WASPADA' : 'AMAN'}
                      </td>
                      <td style={{ padding: '8px' }}>{log.gasVal ?? '-'}</td>
                      <td style={{ padding: '8px' }}>{log.flameVal ?? '-'}</td>
                      <td style={{ padding: '8px' }}>{log.temp?.toFixed(1)}°C</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      ) : !hasData ? (
        <div className="chart-empty">
          <span className="chart-empty-icon"><RadioIcon /></span>
          <span>Menunggu data dari perangkat...</span>
          <span style={{ fontSize: '0.65rem', color: 'var(--ink-dim)', marginTop: 2 }}>
            Data akan muncul setelah ESP32 terhubung dan mengirim data
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
  history: PropTypes.arrayOf(
    PropTypes.shape({
      label:       PropTypes.string.isRequired,
      temperature: PropTypes.number.isRequired,
      humidity:    PropTypes.number.isRequired,
      pressure:    PropTypes.number.isRequired,
      gas:         PropTypes.number.isRequired,
      flame:       PropTypes.number.isRequired,
      heap_free:   PropTypes.number,
    })
  ).isRequired,
  active:          PropTypes.string.isRequired,
  setActive:       PropTypes.func.isRequired,
  dangerLogs:      PropTypes.array,
  clearDangerLogs: PropTypes.func,
};
