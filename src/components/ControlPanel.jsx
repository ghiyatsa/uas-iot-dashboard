import { useState, useCallback } from 'react';
import PropTypes from 'prop-types';
import { BellIcon, BellOffIcon, ZapIcon, SendIcon } from './Icons';

/**
 * ControlPanel Component
 * Menyediakan antarmuka tombol kontrol interaktif untuk mengirim perintah (commands)
 * ke perangkat hardware ESP32 secara remote menggunakan broker MQTT.
 * 
 * @param {Function} publishCmd - Fungsi untuk menerbitkan/publish topik MQTT cmd
 * @param {Boolean} deviceOnline - Status online/offline dari ESP32 liveness check
 * @param {Boolean} buzzerMuted - Status mute buzzer aktif saat ini di ESP32
 */
export default function ControlPanel({ publishCmd, deviceOnline, buzzerMuted = false }) {
  // State untuk melacak status aktifasi perintah guna mencegah klik ganda (spam)
  const [ledBusy, setLedBusy] = useState(false);
  const [tgBusy,  setTgBusy]  = useState(false);

  /**
   * Mengirim perintah toggle status mute buzzer (1 = Mute, 0 = Unmute)
   */
  const handleMuteToggle = useCallback(() => {
    publishCmd('mute', buzzerMuted ? '0' : '1');
  }, [publishCmd, buzzerMuted]);

  /**
   * Mengirim perintah test sequence RGB LED.
   * Tombol dikunci (disabled) selama 2.5 detik selama pengujian berlangsung.
   */
  const handleTestLed = useCallback(() => {
    if (ledBusy) return;
    setLedBusy(true);
    publishCmd('test_led', '1');
    setTimeout(() => setLedBusy(false), 2500); // Sequence LED di ESP32 butuh ~2.5s
  }, [publishCmd, ledBusy]);

  /**
   * Mengirim perintah trigger notifikasi Telegram manual.
   * Tombol dikunci (disabled) selama 5 detik untuk mencegah spam rate limit Telegram.
   */
  const handleTelegramAlert = useCallback(() => {
    if (tgBusy) return;
    setTgBusy(true);
    publishCmd('telegram', '1');
    setTimeout(() => setTgBusy(false), 5000); // Cooldown rate limit
  }, [publishCmd, tgBusy]);

  // Tombol dinonaktifkan jika perangkat terdeteksi offline
  const disabled = !deviceOnline;

  return (
    <div className="control-panel">
      {/* Judul & Subtitle Panel */}
      <div className="control-header">
        <span className="control-title">Kontrol Perangkat</span>
        <span className="control-subtitle">Kirim instruksi ke modul ESP32 via broker MQTT</span>
      </div>

      <div className="control-grid">
        {/* Card 1: Kontrol Alarm Buzzer */}
        <div className="control-card">
          <div className="control-card-icon" style={{ color: buzzerMuted ? 'var(--warning)' : 'var(--normal)' }}>
            {buzzerMuted ? <BellOffIcon /> : <BellIcon />}
          </div>
          <div className="control-card-info">
            <div className="control-card-label">Status Buzzer</div>
            <div className="control-card-status" style={{ color: buzzerMuted ? 'var(--warning)' : 'var(--ink-dim)' }}>
              {buzzerMuted ? 'SENYAP' : 'SIAGA'}
            </div>
          </div>
          <button
            id="btn-mute-buzzer"
            className={`control-btn ${buzzerMuted ? 'control-btn-warning' : 'control-btn-normal'}`}
            onClick={handleMuteToggle}
            disabled={disabled}
            title={disabled ? 'Modul offline' : buzzerMuted ? 'Aktifkan alarm buzzer' : 'Senyapkan alarm buzzer'}
          >
            {buzzerMuted ? 'AKTIFKAN' : 'SENYAPKAN'}
          </button>
        </div>

        {/* Card 2: Pengujian Indikator RGB LED */}
        <div className="control-card">
          <div className="control-card-icon" style={{ color: 'var(--blue)' }}>
            <ZapIcon />
          </div>
          <div className="control-card-info">
            <div className="control-card-label">Status RGB LED</div>
            <div className="control-card-status" style={{ color: 'var(--ink-dim)' }}>
              {ledBusy ? 'Testing...' : 'Ready'}
            </div>
          </div>
          <button
            id="btn-test-led"
            className="control-btn control-btn-blue"
            onClick={handleTestLed}
            disabled={disabled || ledBusy}
            title={disabled ? 'Modul offline' : 'Jalankan uji coba lampu LED'}
          >
            {ledBusy ? '...' : 'TEST LED'}
          </button>
        </div>

        {/* Card 3: Trigger Telegram Alert Manual */}
        <div className="control-card">
          <div className="control-card-icon" style={{ color: 'var(--ink-2)' }}>
            <SendIcon />
          </div>
          <div className="control-card-info">
            <div className="control-card-label">Telegram Alert</div>
            <div className="control-card-status" style={{ color: 'var(--ink-dim)' }}>
              {tgBusy ? 'Sending...' : 'Manual Trigger'}
            </div>
          </div>
          <button
            id="btn-telegram-alert"
            className="control-btn control-btn-dim"
            onClick={handleTelegramAlert}
            disabled={disabled || tgBusy}
            title={disabled ? 'Modul offline' : 'Kirim status ke Telegram sekarang'}
          >
            {tgBusy ? '...' : 'PING TELEGRAM'}
          </button>
        </div>
      </div>

      {/* Notifikasi jika perangkat terputus dari internet */}
      {disabled && (
        <div className="control-offline-notice">
          Sinyal Terputus — modul ESP32 sedang offline
        </div>
      )}
    </div>
  );
}

ControlPanel.propTypes = {
  publishCmd:   PropTypes.func.isRequired,
  deviceOnline: PropTypes.bool.isRequired,
  buzzerMuted:  PropTypes.bool,
};
