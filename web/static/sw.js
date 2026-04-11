const CACHE_NAME = 'local-video-v3';
const PRECACHE_URLS = [
    '/',
    '/index.html',
    '/css/style.css',
    '/js/app.js',
    '/manifest.json'
];

self.addEventListener('install', (event) => {
    event.waitUntil(
        caches.open(CACHE_NAME).then((cache) => {
            return cache.addAll(PRECACHE_URLS);
        })
    );
    self.skipWaiting();
});

self.addEventListener('activate', (event) => {
    event.waitUntil(
        caches.keys().then((cacheNames) => {
            return Promise.all(
                cacheNames
                    .filter((name) => name !== CACHE_NAME)
                    .map((name) => caches.delete(name))
            );
        })
    );
    self.clients.claim();
});

self.addEventListener('fetch', (event) => {
    if (event.request.method !== 'GET') return;

    const pathname = new URL(event.request.url).pathname;
    if (pathname.startsWith('/api/') || pathname.startsWith('/video/') ||
        pathname.startsWith('/thumbnail/')) {
        return;
    }

    /* HTML: network-first to always get latest version */
    if (event.request.mode === 'navigate' ||
        event.request.url.endsWith('/index.html') ||
        event.request.url.endsWith('/')) {
        event.respondWith(
            fetch(event.request).then((response) => {
                if (response && response.status === 200) {
                    const responseToCache = response.clone();
                    caches.open(CACHE_NAME).then((cache) => {
                        cache.put(event.request, responseToCache);
                    });
                }
                return response;
            }).catch(() => {
                return caches.match('/index.html');
            })
        );
        return;
    }

    /* Static assets (JS/CSS): cache-first with fallback */
    event.respondWith(
        caches.match(event.request).then((cachedResponse) => {
            if (cachedResponse) {
                return cachedResponse;
            }
            return fetch(event.request).then((response) => {
                if (!response || response.status !== 200 || response.type !== 'basic') {
                    return response;
                }
                const responseToCache = response.clone();
                caches.open(CACHE_NAME).then((cache) => {
                    cache.put(event.request, responseToCache);
                });
                return response;
            });
        }).catch(() => {
            if (event.request.destination === 'document') {
                return caches.match('/index.html');
            }
        })
    );
});
