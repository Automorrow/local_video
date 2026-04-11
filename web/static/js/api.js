import { API_BASE, showToast } from './common.js';

export async function fetchJSON(url, options = {}) {
    try {
        const response = await fetch(url, options);
        if (!response.ok) {
            const text = await response.text().catch(() => '');
            return { error: true, status: response.status, message: text || 'HTTP error' };
        }
        return await response.json();
    } catch (error) {
        console.error('Fetch error:', error);
        return { error: true, status: 0, message: error.message || 'Network error' };
    }
}

export const api = {
    getVideos: (limit, offset, search) => {
        const params = [`limit=${limit}`, `offset=${offset}`];
        if (search) params.push(`search=${encodeURIComponent(search)}`);
        return fetchJSON(`${API_BASE}/videos?${params.join('&')}`);
    },
    getFavorites: () => fetchJSON(`${API_BASE}/favorites`),
    getRandom: () => fetchJSON(`${API_BASE}/random`),
    getHistory: () => fetchJSON(`${API_BASE}/history`),
    getBlacklist: () => fetchJSON(`${API_BASE}/blacklist`),
    postBlacklist: (path) => fetchJSON(`${API_BASE}/blacklist`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path })
    }),
    deleteBlacklist: (id) => fetchJSON(`${API_BASE}/blacklist/${id}`, { method: 'DELETE' }),
    postHistory: (videoId, position) => fetchJSON(`${API_BASE}/history`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ video_id: videoId, position })
    }),
    deleteHistory: (id) => fetchJSON(`${API_BASE}/history/${id}`, { method: 'DELETE' }),
    postFavorite: (videoId) => fetchJSON(`${API_BASE}/favorites`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ video_id: videoId })
    }),
    deleteFavorite: (videoId) => fetchJSON(`${API_BASE}/favorites/${videoId}`, { method: 'DELETE' }),
    deleteAllHistory: () => fetchJSON(`${API_BASE}/history`, { method: 'DELETE' }),
    getConfig: () => fetchJSON(`${API_BASE}/config`),
    postConfig: (port, scanDir) => fetchJSON(`${API_BASE}/config`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ port, scan_directory: scanDir })
    }),
    getScanStatus: () => fetchJSON(`${API_BASE}/scan-status`),
    browse: (path) => fetchJSON(`${API_BASE}/browse?path=${encodeURIComponent(path || '')}`)
};
