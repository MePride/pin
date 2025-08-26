/**
 * Pin Configuration PWA Service Worker
 */

const CACHE_NAME = 'pin-config-v1.0.0';
const STATIC_ASSETS = [
    '/',
    '/index.html',
    '/app.js',
    '/manifest.json'
];

// 安装事件 - 缓存静态资源
self.addEventListener('install', (event) => {
    console.log('Service Worker installing...');
    
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then((cache) => {
                console.log('Caching static assets...');
                return cache.addAll(STATIC_ASSETS);
            })
            .then(() => {
                console.log('Static assets cached successfully');
                return self.skipWaiting();
            })
            .catch((error) => {
                console.error('Failed to cache static assets:', error);
            })
    );
});

// 激活事件 - 清理旧缓存
self.addEventListener('activate', (event) => {
    console.log('Service Worker activating...');
    
    event.waitUntil(
        caches.keys()
            .then((cacheNames) => {
                return Promise.all(
                    cacheNames.map((cacheName) => {
                        if (cacheName !== CACHE_NAME) {
                            console.log('Deleting old cache:', cacheName);
                            return caches.delete(cacheName);
                        }
                    })
                );
            })
            .then(() => {
                console.log('Service Worker activated');
                return self.clients.claim();
            })
    );
});

// 拦截请求 - 缓存策略
self.addEventListener('fetch', (event) => {
    const request = event.request;
    
    // 只处理GET请求
    if (request.method !== 'GET') {
        return;
    }
    
    // API请求 - 网络优先策略
    if (request.url.includes('/api/')) {
        event.respondWith(
            fetch(request)
                .then((response) => {
                    // 缓存成功的API响应
                    if (response.ok && response.status === 200) {
                        const responseClone = response.clone();
                        caches.open(CACHE_NAME)
                            .then((cache) => {
                                cache.put(request, responseClone);
                            });
                    }
                    return response;
                })
                .catch(() => {
                    // 网络失败时尝试从缓存获取
                    return caches.match(request)
                        .then((cachedResponse) => {
                            if (cachedResponse) {
                                return cachedResponse;
                            }
                            // 返回离线响应
                            return new Response(
                                JSON.stringify({ error: '网络连接失败，请检查设备连接', offline: true }),
                                { 
                                    status: 503,
                                    headers: { 'Content-Type': 'application/json' }
                                }
                            );
                        });
                })
        );
        return;
    }
    
    // 静态资源 - 缓存优先策略
    event.respondWith(
        caches.match(request)
            .then((cachedResponse) => {
                if (cachedResponse) {
                    return cachedResponse;
                }
                
                return fetch(request)
                    .then((response) => {
                        // 只缓存成功的响应
                        if (response.ok && response.status === 200) {
                            const responseClone = response.clone();
                            caches.open(CACHE_NAME)
                                .then((cache) => {
                                    cache.put(request, responseClone);
                                });
                        }
                        return response;
                    })
                    .catch(() => {
                        // 返回离线提示页面
                        if (request.url.endsWith('.html') || 
                            request.headers.get('accept')?.includes('text/html')) {
                            return new Response(`
                                <!DOCTYPE html>
                                <html>
                                <head>
                                    <title>离线模式</title>
                                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                                    <style>
                                        body { 
                                            font-family: sans-serif; 
                                            text-align: center; 
                                            padding: 50px;
                                            background: #f5f5f5;
                                        }
                                        .offline { 
                                            background: white;
                                            padding: 40px;
                                            border-radius: 12px;
                                            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
                                            max-width: 400px;
                                            margin: 0 auto;
                                        }
                                        .btn {
                                            background: #2196F3;
                                            color: white;
                                            border: none;
                                            padding: 12px 24px;
                                            border-radius: 8px;
                                            cursor: pointer;
                                            font-size: 16px;
                                            margin-top: 20px;
                                        }
                                    </style>
                                </head>
                                <body>
                                    <div class="offline">
                                        <h1>😔 离线模式</h1>
                                        <p>无法连接到Pin设备，请检查网络连接。</p>
                                        <button class="btn" onclick="location.reload()">重试</button>
                                    </div>
                                </body>
                                </html>
                            `, {
                                headers: { 'Content-Type': 'text/html' }
                            });
                        }
                        
                        return new Response('资源不可用', { status: 404 });
                    });
            })
    );
});