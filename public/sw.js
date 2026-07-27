// Service Worker — cache app shell untuk offline support
const CACHE_NAME = 'room-safety-v1';
const SHELL = ['/', '/index.html'];

self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(SHELL))
  );
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

// Network-first strategy: coba network, fallback ke cache
self.addEventListener('fetch', event => {
  // Hanya handle GET requests yang bukan MQTT WebSocket
  if (event.request.method !== 'GET') return;
  if (!event.request.url.startsWith('http')) return; // Abaikan ekstensi browser, chrome-extension, dll.
  if (event.request.url.includes('mqtt') || event.request.url.includes('wss://')) return;

  event.respondWith(
    fetch(event.request)
      .then(response => {
        const clone = response.clone();
        caches.open(CACHE_NAME).then(cache => cache.put(event.request, clone));
        return response;
      })
      .catch(() => caches.match(event.request))
  );
});
