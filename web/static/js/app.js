const API_BASE = '/api';

const PAGE_SIZE = 40;

const state = {
    videos: [],
    categories: [],
    currentVideo: null,
    currentFilter: 'all',
    favorites: new Set(),
    blacklist: [],
    searchTimeout: null,
    pendingVideo: null,
    currentPage: 0,
    allLoaded: false,
    currentSearch: '',
    loadSerial: 0,
    togglingFavorite: new Set(),
    savingSettings: false,
    addingBlacklist: false
};

const elements = {
    videoList: document.getElementById('videoList'),
    loading: document.getElementById('loading'),
    emptyMessage: document.getElementById('emptyMessage'),
    searchInput: document.getElementById('searchInput'),
    searchBtn: document.getElementById('searchBtn'),
    randomBtn: document.getElementById('randomBtn'),
    historyBtn: document.getElementById('historyBtn'),
    favoritesBtn: document.getElementById('favoritesBtn'),
    blacklistBtn: document.getElementById('blacklistBtn'),
    allVideosBtn: document.getElementById('allVideosBtn'),
    videoModal: document.getElementById('videoModal'),
    videoPlayer: document.getElementById('videoPlayer'),
    videoTitle: document.getElementById('videoTitle'),
    videoPath: document.getElementById('videoPath'),
    toggleFavoriteBtn: document.getElementById('toggleFavoriteBtn'),
    playRandomBtn: document.getElementById('playRandomBtn'),
    historyModal: document.getElementById('historyModal'),
    historyList: document.getElementById('historyList'),
    clearHistoryBtn: document.getElementById('clearHistoryBtn'),
    favoritesModal: document.getElementById('favoritesModal'),
    favoritesList: document.getElementById('favoritesList'),
    blacklistModal: document.getElementById('blacklistModal'),
    blacklistList: document.getElementById('blacklistList'),
    blacklistInput: document.getElementById('blacklistInput'),
    addBlacklistBtn: document.getElementById('addBlacklistBtn'),
    toastContainer: document.getElementById('toastContainer'),
    resumeModal: document.getElementById('resumeModal'),
    resumeVideoTitle: document.getElementById('resumeVideoTitle'),
    resumePosition: document.getElementById('resumePosition'),
    resumeFromBeginning: document.getElementById('resumeFromBeginning'),
    resumeFromPosition: document.getElementById('resumeFromPosition'),
    settingsBtn: document.getElementById('settingsBtn'),
    settingsModal: document.getElementById('settingsModal'),
    settingsPort: document.getElementById('settingsPort'),
    dirBreadcrumb: document.getElementById('dirBreadcrumb'),
    dirList: document.getElementById('dirList'),
    dirSelectedPath: document.getElementById('dirSelectedPath'),
    settingsSaveBtn: document.getElementById('settingsSaveBtn'),
    settingsStatus: document.getElementById('settingsStatus')
};

function escapeHtml(str) {
    if (typeof str !== 'string') return String(str ?? '');
    return str.replace(/&/g, '&amp;')
              .replace(/</g, '&lt;')
              .replace(/>/g, '&gt;')
              .replace(/"/g, '&quot;')
              .replace(/'/g, '&#39;');
}

function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    elements.toastContainer.appendChild(toast);
    
    setTimeout(() => {
        toast.style.animation = 'toastIn 0.3s ease-out reverse';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

function formatSize(bytes) {
    if (!bytes || bytes === 0) return '0 B';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
}

function formatPosition(seconds) {
    if (!seconds || seconds === 0) return '0:00';
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
}

function showLoading() {
    elements.loading.style.display = 'block';
    elements.emptyMessage.style.display = 'none';
    elements.videoList.style.display = 'none';
}

function hideLoading() {
    elements.loading.style.display = 'none';
}

function showEmpty() {
    elements.emptyMessage.style.display = 'block';
    elements.videoList.style.display = 'none';
}

function showVideos() {
    elements.loading.style.display = 'none';
    elements.emptyMessage.style.display = 'none';
    elements.videoList.style.display = 'grid';
}

async function fetchJSON(url, options = {}) {
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

async function loadVideos(searchQuery = '', append = false) {
    if (!append) {
        showLoading();
        state.currentPage = 0;
        state.allLoaded = false;
        state.currentSearch = searchQuery;
        state.videos = [];
        state.loadSerial++;
        const sentinel = document.getElementById('scrollSentinel');
        if (sentinel && state._sentinelObserver) {
            state._sentinelObserver.unobserve(sentinel);
        }
    }

    const mySerial = state.loadSerial;
    const params = [];
    params.push(`limit=${PAGE_SIZE}`);
    params.push(`offset=${state.currentPage * PAGE_SIZE}`);
    if (searchQuery) params.push(`search=${encodeURIComponent(searchQuery)}`);

    const url = API_BASE + '/videos?' + params.join('&');
    const data = await fetchJSON(url);

    if (mySerial !== state.loadSerial) return;

    if (data && Array.isArray(data)) {
        if (data.length < PAGE_SIZE) state.allLoaded = true;
        state.videos = append ? state.videos.concat(data) : data;
        state.currentPage++;
        renderVideos(state.videos, append);
    } else if (!append) {
        showEmpty();
    }
}

function debouncedSearch(query) {
    if (state.searchTimeout) {
        clearTimeout(state.searchTimeout);
    }
    state.searchTimeout = setTimeout(() => {
        loadVideos(query);
    }, 300);
}

async function loadFavorites() {
    const data = await fetchJSON(API_BASE + '/favorites');
    if (data && Array.isArray(data)) {
        state.favorites = new Set(data.map(f => f.video_id));
    }
}

async function getRandomVideo() {
    const data = await fetchJSON(API_BASE + '/random');
    if (data && data.length > 0) {
        playVideo(data[0]);
    }
}

async function getHistory() {
    const data = await fetchJSON(API_BASE + '/history');
    if (data) {
        renderHistory(data);
        elements.historyModal.classList.add('active');
        if (!window.location.hash) history.pushState(null, '', '#history');
    }
}

async function getFavorites() {
    const data = await fetchJSON(API_BASE + '/favorites');
    if (data) {
        renderFavorites(data);
        elements.favoritesModal.classList.add('active');
        if (!window.location.hash) history.pushState(null, '', '#favorites');
    }
}

async function getBlacklist() {
    const data = await fetchJSON(API_BASE + '/blacklist');
    if (data) {
        renderBlacklist(data);
        elements.blacklistModal.classList.add('active');
        if (!window.location.hash) history.pushState(null, '', '#blacklist');
    }
}

async function addBlacklist(path) {
    if (!path) return;
    state.addingBlacklist = true;
    const result = await fetchJSON(API_BASE + '/blacklist', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path: path })
    });
    state.addingBlacklist = false;
    if (result && result.success) {
        getBlacklist();
        elements.blacklistInput.value = '';
        loadVideos();
        showToast('Directory added to blacklist', 'success');
    } else {
        showToast(result?.error || result?.message || 'Failed to add to blacklist', 'error');
    }
}

async function removeBlacklist(id) {
    await fetchJSON(API_BASE + '/blacklist/' + id, {
        method: 'DELETE'
    });
    getBlacklist();
    loadVideos();
    showToast('Directory removed from blacklist', 'success');
}

function renderBlacklist(blacklist) {
    if (!blacklist || blacklist.length === 0) {
        elements.blacklistList.innerHTML = '<p>No directories in blacklist</p>';
        return;
    }
    elements.blacklistList.innerHTML = blacklist.map(item => `
        <div class="blacklist-item" data-id="${item.id}">
            <div class="blacklist-info">
                <h4>${escapeHtml(item.path)}</h4>
                <p>Added: ${new Date(item.created_at * 1000).toLocaleString()}</p>
            </div>
            <button class="remove-btn" data-id="${item.id}">Remove</button>
        </div>
    `).join('');
    
    elements.blacklistList.querySelectorAll('.blacklist-item').forEach(item => {
        item.querySelector('.remove-btn').addEventListener('click', (e) => {
            e.stopPropagation();
            removeBlacklist(parseInt(item.dataset.id));
        });
    });
}

async function addToHistory(videoId, position = 0) {
    await fetchJSON(API_BASE + '/history', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ video_id: videoId, position: position })
    });
}

async function removeFromHistory(historyId) {
    await fetchJSON(API_BASE + '/history/' + historyId, {
        method: 'DELETE'
    });
    getHistory();
    showToast('History item removed', 'success');
}

function updateVideoCardFavorite(videoId, isFavorite) {
    const card = document.querySelector(`.video-card[data-id="${videoId}"] .video-thumbnail`);
    if (!card) return;
    let badge = card.querySelector('.favorite-badge');
    if (isFavorite) {
        if (!badge) {
            badge = document.createElement('span');
            badge.className = 'favorite-badge';
            badge.textContent = '❤️';
            card.appendChild(badge);
        }
    } else if (badge) {
        badge.remove();
    }
}

async function toggleFavorite(videoId) {
    if (state.togglingFavorite.has(videoId)) return;
    state.togglingFavorite.add(videoId);
    const isFavorite = state.favorites.has(videoId);
    let result;
    if (isFavorite) {
        result = await fetchJSON(API_BASE + '/favorites/' + videoId, { method: 'DELETE' });
        if (result && !result.error) {
            state.favorites.delete(videoId);
            showToast('Removed from favorites', 'info');
        }
    } else {
        result = await fetchJSON(API_BASE + '/favorites', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ video_id: videoId })
        });
        if (result && !result.error) {
            state.favorites.add(videoId);
            showToast('Added to favorites', 'success');
        }
    }
    state.togglingFavorite.delete(videoId);
    if (result && result.error) {
        showToast(result.message || 'Failed to update favorites', 'error');
    } else {
        updateFavoriteButton();
        updateVideoCardFavorite(videoId, !isFavorite);
    }
}

async function clearHistory() {
    await fetchJSON(API_BASE + '/history', { method: 'DELETE' });
    getHistory();
    showToast('History cleared', 'success');
}

let _thumbnailObserver = null;

function renderVideos(videos, append) {
    hideLoading();
    if (!videos || videos.length === 0) {
        showEmpty();
        return;
    }
    showVideos();

    /* Build HTML only for new items */
    const startIdx = append ? (state.currentPage - 1) * PAGE_SIZE : 0;
    const newVideos = videos.slice(startIdx);
    const html = newVideos.map(video => `
        <div class="video-card" data-id="${video.id}">
            <div class="video-thumbnail">
                <img data-src="/thumbnail/${video.id}" alt="" onerror="this.style.display='none'; this.nextElementSibling.style.display='block';">
                <span style="display: none;">▶</span>
                ${state.favorites.has(video.id) ? '<span class="favorite-badge">❤️</span>' : ''}
            </div>
            <div class="video-info-card">
                    <div class="video-title" title="${escapeHtml(video.title)}">${escapeHtml(video.title)}</div>
                    <div class="video-category">${escapeHtml(video.category || '')}</div>
                    <div class="video-size">${formatSize(video.size)}</div>
                </div>
        </div>
    `).join('');

    if (append) {
        elements.videoList.insertAdjacentHTML('beforeend', html);
    } else {
        elements.videoList.innerHTML = html;
    }

    /* Lazy load new thumbnails */
    const lazyImages = elements.videoList.querySelectorAll('img[data-src]');
    if ('IntersectionObserver' in window) {
        if (!_thumbnailObserver) {
            _thumbnailObserver = new IntersectionObserver((entries) => {
                entries.forEach(entry => {
                    if (entry.isIntersecting) {
                        const img = entry.target;
                        img.src = img.dataset.src;
                        img.removeAttribute('data-src');
                        _thumbnailObserver.unobserve(img);
                    }
                });
            }, { rootMargin: '200px' });
        }
        lazyImages.forEach(img => _thumbnailObserver.observe(img));
    } else {
        lazyImages.forEach(img => {
            img.src = img.dataset.src;
            img.removeAttribute('data-src');
        });
    }

    /* Bind click events for new cards */
    elements.videoList.querySelectorAll('.video-card').forEach(card => {
        if (card._bound) return;
        card._bound = true;
        card.addEventListener('click', () => {
            const video = state.videos.find(v => v.id == card.dataset.id);
            if (video) playVideo(video);
        });
    });

    /* Infinite scroll sentinel */
    let sentinel = document.getElementById('scrollSentinel');
    if (!sentinel) {
        sentinel = document.createElement('div');
        sentinel.id = 'scrollSentinel';
        sentinel.style.height = '1px';
        elements.videoList.parentNode.appendChild(sentinel);
    }
    if (!state.allLoaded) {
        sentinel.style.display = 'block';
        if (!state._sentinelObserver) {
            state._sentinelObserver = new IntersectionObserver((entries) => {
                if (entries[0].isIntersecting && !state.allLoaded) {
                    loadVideos(state.currentSearch, true);
                }
            }, { rootMargin: '400px' });
        }
        state._sentinelObserver.observe(sentinel);
    } else if (sentinel) {
        sentinel.style.display = 'none';
        if (state._sentinelObserver) state._sentinelObserver.unobserve(sentinel);
    }
}

function renderHistory(history) {
    if (!history || history.length === 0) {
        elements.historyList.innerHTML = '<p>No history yet</p>';
        return;
    }
    elements.historyList.innerHTML = history.map(item => `
        <div class="history-item" data-id="${item.id}" data-video-id="${item.video_id}" data-path="${escapeHtml(item.path)}" data-position="${item.position || 0}">
            <div class="history-info">
                <h4>${escapeHtml(item.title || 'Unknown')}</h4>
                <p>${escapeHtml(item.path)}</p>
                <span class="position">Position: ${formatPosition(item.position)}</span>
            </div>
            <button class="remove-btn" data-id="${item.id}">Remove</button>
        </div>
    `).join('');
    
    elements.historyList.querySelectorAll('.history-item').forEach(item => {
        item.addEventListener('click', (e) => {
            if (e.target.classList.contains('remove-btn')) {
                e.stopPropagation();
                removeFromHistory(item.dataset.id);
            } else {
                const video = { 
                    id: item.dataset.videoId, 
                    path: item.dataset.path, 
                    title: item.querySelector('h4').textContent,
                    position: parseInt(item.dataset.position) || 0
                };
                elements.historyModal.classList.remove('active');
                
                if (video.position > 5) {
                    showResumeDialog(video);
                } else {
                    playVideo(video);
                }
            }
        });
    });
}

function renderFavorites(favorites) {
    if (!favorites || favorites.length === 0) {
        elements.favoritesList.innerHTML = '<p>No favorites yet</p>';
        return;
    }
    elements.favoritesList.innerHTML = favorites.map(item => `
        <div class="favorite-item" data-id="${item.video_id}" data-path="${escapeHtml(item.path)}">
            <div class="favorite-info">
                <h4>${escapeHtml(item.title || 'Unknown')}</h4>
                <p>${escapeHtml(item.path)}</p>
            </div>
            <button class="remove-btn" data-id="${item.video_id}">Remove</button>
        </div>
    `).join('');
    
    elements.favoritesList.querySelectorAll('.favorite-item').forEach(item => {
        item.addEventListener('click', (e) => {
            if (e.target.classList.contains('remove-btn')) {
                e.stopPropagation();
                const videoId = parseInt(e.target.dataset.id);
                toggleFavorite(videoId).then(() => getFavorites());
            } else {
                const video = { 
                    id: item.dataset.id, 
                    path: item.dataset.path, 
                    title: item.querySelector('h4').textContent 
                };
                elements.favoritesModal.classList.remove('active');
                playVideo(video);
            }
        });
    });
}

function showResumeDialog(video) {
    state.pendingVideo = video;
    elements.resumeVideoTitle.textContent = video.title || 'Unknown';
    elements.resumePosition.textContent = `Last played at: ${formatPosition(video.position)}`;
    elements.resumeModal.classList.add('active');
    if (!window.location.hash) history.pushState(null, '', '#resume');
}

function playVideo(video, resume = true) {
    state.currentVideo = video;
    elements.videoTitle.textContent = video.title || 'Unknown';
    elements.videoPath.textContent = video.path || '';

    updateFavoriteButton();
    addToHistory(video.id, 0);

    // Add error handling
    elements.videoPlayer.onerror = () => {
        showToast('视频无法播放，可能已被删除或加入黑名单', 'error');
        closeModal(elements.videoModal);
    };

    if (resume && video.position && video.position > 0) {
        elements.videoPlayer.onloadedmetadata = () => {
            elements.videoPlayer.currentTime = video.position;
            elements.videoPlayer.play().catch(err => {
                console.log('Auto-play blocked:', err);
            });
        };
    } else {
        elements.videoPlayer.onloadedmetadata = () => {
            elements.videoPlayer.play().catch(err => {
                console.log('Auto-play blocked:', err);
            });
        };
    }

    elements.videoPlayer.src = '/video/' + video.id;
    elements.videoPlayer.load();
    elements.videoModal.classList.add('active');
    if (!window.location.hash) history.pushState(null, '', '#video');
}

function updateFavoriteButton() {
    if (state.currentVideo && state.favorites.has(state.currentVideo.id)) {
        elements.toggleFavoriteBtn.textContent = 'Remove from Favorites';
        elements.toggleFavoriteBtn.classList.add('active');
    } else {
        elements.toggleFavoriteBtn.textContent = 'Add to Favorites';
        elements.toggleFavoriteBtn.classList.remove('active');
    }
}

function playNextVideo() {
    if (!state.currentVideo || state.videos.length === 0) return;
    
    const currentIndex = state.videos.findIndex(v => v.id === state.currentVideo.id);
    if (currentIndex >= 0 && currentIndex < state.videos.length - 1) {
        playVideo(state.videos[currentIndex + 1]);
    } else if (state.videos.length > 0) {
        playVideo(state.videos[0]);
    }
}

function closeTopModal() {
    const modals = [
        elements.resumeModal,
        elements.videoModal,
        elements.historyModal,
        elements.favoritesModal,
        elements.blacklistModal,
        elements.settingsModal
    ];
    
    for (const modal of modals) {
        if (modal.classList.contains('active')) {
            closeModal(modal);
            return true;
        }
    }
    return false;
}

function closeModal(modal) {
    modal.classList.remove('active');
    if (modal === elements.videoModal) {
        elements.videoPlayer.pause();
    }
    if (modal === elements.resumeModal) {
        state.pendingVideo = null;
    }
    if (modal === elements.settingsModal) {
        document.title = 'LocalVideoServer';
    }
    if (window.location.hash) {
        history.pushState(null, '', window.location.pathname + window.location.search);
    }
}

function initEventListeners() {
    elements.searchBtn.addEventListener('click', () => {
        loadVideos(elements.searchInput.value);
    });

    elements.searchInput.addEventListener('input', (e) => {
        debouncedSearch(e.target.value);
    });

    elements.searchInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            if (state.searchTimeout) clearTimeout(state.searchTimeout);
            loadVideos(elements.searchInput.value.trim());
        }
    });
    
    elements.randomBtn.addEventListener('click', getRandomVideo);
    elements.historyBtn.addEventListener('click', getHistory);
    elements.favoritesBtn.addEventListener('click', getFavorites);
    elements.blacklistBtn.addEventListener('click', getBlacklist);
    
    elements.allVideosBtn.addEventListener('click', () => {
        elements.searchInput.value = '';
        loadVideos();
    });
    
    elements.toggleFavoriteBtn.addEventListener('click', () => {
        if (state.currentVideo) {
            toggleFavorite(state.currentVideo.id);
        }
    });
    
    elements.playRandomBtn.addEventListener('click', getRandomVideo);
    elements.clearHistoryBtn.addEventListener('click', clearHistory);
    
    elements.addBlacklistBtn.addEventListener('click', () => {
        if (state.addingBlacklist) return;
        addBlacklist(elements.blacklistInput.value.trim());
    });

    elements.blacklistInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            addBlacklist(elements.blacklistInput.value.trim());
        }
    });
    
    document.querySelectorAll('.close').forEach(btn => {
        btn.addEventListener('click', () => {
            closeTopModal();
        });
    });
    
    window.addEventListener('click', (e) => {
        if (e.target.classList.contains('modal')) {
            closeTopModal();
        }
    });
    
    document.addEventListener('keydown', (e) => {
        const tag = e.target.tagName;
        const isTyping = tag === 'INPUT' || tag === 'TEXTAREA' || e.target.isContentEditable;
        if (e.key === 'Escape') {
            closeTopModal();
        }
        if (!elements.videoModal.classList.contains('active')) return;
        if (isTyping) return;
        if (e.key === 'ArrowLeft') {
            e.preventDefault();
            elements.videoPlayer.currentTime = Math.max(0, elements.videoPlayer.currentTime - 10);
        } else if (e.key === 'ArrowRight') {
            e.preventDefault();
            elements.videoPlayer.currentTime = Math.min(elements.videoPlayer.duration || Infinity, elements.videoPlayer.currentTime + 10);
        } else if (e.key === 'f' || e.key === 'F') {
            e.preventDefault();
            if (elements.videoPlayer.requestFullscreen) elements.videoPlayer.requestFullscreen();
            else if (elements.videoPlayer.webkitRequestFullscreen) elements.videoPlayer.webkitRequestFullscreen();
        } else if (e.key === 'm' || e.key === 'M') {
            e.preventDefault();
            elements.videoPlayer.muted = !elements.videoPlayer.muted;
        } else if (e.key === 'n' || e.key === 'N') {
            e.preventDefault();
            playNextVideo();
        }
    });
    
    elements.resumeFromBeginning.addEventListener('click', () => {
        if (state.pendingVideo) {
            elements.resumeModal.classList.remove('active');
            playVideo(state.pendingVideo, false);
            state.pendingVideo = null;
        }
    });
    
    elements.resumeFromPosition.addEventListener('click', () => {
        if (state.pendingVideo) {
            elements.resumeModal.classList.remove('active');
            playVideo(state.pendingVideo, true);
            state.pendingVideo = null;
        }
    });
    
    elements.videoPlayer.addEventListener('ended', () => {
        if (state.currentVideo) {
            const position = Math.floor(elements.videoPlayer.currentTime);
            addToHistory(state.currentVideo.id, position);
        }
        playNextVideo();
    });
    
    let pauseDebounceTimer = null;
    elements.videoPlayer.addEventListener('pause', () => {
        if (state.currentVideo) {
            const position = Math.floor(elements.videoPlayer.currentTime);
            if (pauseDebounceTimer) clearTimeout(pauseDebounceTimer);
            pauseDebounceTimer = setTimeout(() => {
                addToHistory(state.currentVideo.id, position);
            }, 500);
        }
    });

    /* Settings */
    elements.settingsBtn.addEventListener('click', openSettings);
    elements.settingsSaveBtn.addEventListener('click', () => {
        if (state.savingSettings) return;
        saveSettings();
    });
}

async function init() {
    initEventListeners();
    await loadFavorites();
    await loadVideos();

    /* Browser back button closes modals */
    window.addEventListener('hashchange', () => {
        if (!window.location.hash) {
            closeTopModal();
        } else if (window.location.hash === '#settings') {
            openSettings();
        }
    });

    /* Auto-open settings if URL has #settings */
    if (window.location.hash === '#settings') {
        openSettings();
    }
}

/* ===== Settings ===== */
let settingsSelectedDir = '';

async function openSettings() {
    elements.settingsModal.classList.add('active');
    elements.settingsStatus.className = 'settings-status';
    elements.settingsStatus.textContent = '';
    document.title = 'Settings - LocalVideoServer';
    if (window.location.hash !== '#settings') history.pushState(null, '', '#settings');

    const config = await fetchJSON(API_BASE + '/config');
    if (config) {
        elements.settingsPort.value = config.port || '';
        settingsSelectedDir = config.scan_directory || '';
        elements.dirSelectedPath.textContent = settingsSelectedDir || 'None';
        /* Start browsing from drive root or current path's drive */
        const startPath = settingsSelectedDir ? settingsSelectedDir.substring(0, 3) : '';
        browseDir(startPath);
    } else {
        browseDir('');
    }
}

function closeSettings() {
    elements.settingsModal.classList.remove('active');
    document.title = 'LocalVideoServer';
    if (window.location.hash) {
        history.pushState(null, '', window.location.pathname + window.location.search);
    }
}

async function browseDir(path) {
    if (!path || path === 'undefined') path = '';
    const url = API_BASE + '/browse?path=' + encodeURIComponent(path);
    const dirs = await fetchJSON(url);
    if (!dirs || !Array.isArray(dirs)) {
        elements.dirList.innerHTML = '<div class="dir-empty">Unable to read directory</div>';
        return;
    }

    const sep = (navigator.platform && navigator.platform.toLowerCase().includes('win')) ? '\\' : '/';
    if (path) {
        const parts = path.split(/[/\\]/).filter(Boolean);
        let breadcrumb = '<span class="dir-crumb" data-path="">Root</span>';
        let accum = '';
        for (const part of parts) {
            accum += (accum ? sep : '') + part;
            breadcrumb += ' <span class="dir-crumb-sep">›</span> <span class="dir-crumb" data-path="' + escapeHtml(accum) + '">' + escapeHtml(part) + '</span>';
        }
        elements.dirBreadcrumb.innerHTML = breadcrumb;
        elements.dirBreadcrumb.querySelectorAll('.dir-crumb').forEach(crumb => {
            crumb.addEventListener('click', () => browseDir(crumb.dataset.path));
        });
    } else {
        elements.dirBreadcrumb.innerHTML = '<span class="dir-crumb" data-path="">Root</span>';
    }

    if (dirs.length === 0) {
        elements.dirList.innerHTML = '<div class="dir-empty">No subdirectories</div>';
        return;
    }

    elements.dirList.innerHTML = dirs.map(d => {
        const icon = d.type === 'drive' ? '💾' : '📁';
        const sel = d.path === settingsSelectedDir ? ' selected' : '';
        return '<div class="dir-item' + sel + '" data-path="' + escapeHtml(d.path) + '"><span class="dir-icon">' + icon + '</span><span class="dir-name">' + escapeHtml(d.name) + '</span></div>';
    }).join('');

    elements.dirList.querySelectorAll('.dir-item').forEach(item => {
        item.addEventListener('click', () => {
            settingsSelectedDir = item.dataset.path;
            elements.dirSelectedPath.textContent = settingsSelectedDir;
            elements.dirList.querySelectorAll('.dir-item').forEach(i => i.classList.remove('selected'));
            item.classList.add('selected');
        });
        item.addEventListener('dblclick', () => {
            browseDir(item.dataset.path);
        });
    });
}

async function saveSettings() {
    state.savingSettings = true;
    const port = parseInt(elements.settingsPort.value) || 0;
    const dir = settingsSelectedDir;

    if (!dir) {
        showSettingsStatus('Please select a video directory', 'error');
        state.savingSettings = false;
        return;
    }

    const body = JSON.stringify({ port: port, scan_directory: dir });
    const result = await fetchJSON(API_BASE + '/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: body
    });

    if (result && result.success) {
        showSettingsStatus('Settings saved! Rescanning videos...', 'success');
        setTimeout(() => {
            closeSettings();
            /* Poll scan status until scanning finishes, then load videos once */
            let attempts = 0;
            const poll = async () => {
                const status = await fetchJSON(API_BASE + '/scan-status');
                attempts++;
                if (status && !status.scanning) {
                    await loadVideos();
                    if (status.video_count > 0) {
                        showToast(`Scan complete: ${status.video_count} videos found`, 'success');
                    } else {
                        showToast('Scan complete. No videos found.', 'info');
                    }
                    return;
                }
                if (attempts < 60) {
                    setTimeout(poll, 1000);
                } else {
                    await loadVideos();
                    showToast('Scan timeout. Videos may still be loading.', 'info');
                }
            };
            poll();
        }, 500);
    } else {
        showSettingsStatus(result?.message || 'Failed to save settings', 'error');
    }
    state.savingSettings = false;
}

function showSettingsStatus(msg, type) {
    elements.settingsStatus.textContent = msg;
    elements.settingsStatus.className = 'settings-status ' + type;
}

init();