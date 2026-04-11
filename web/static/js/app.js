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
    currentSearch: ''
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
        if (!response.ok) throw new Error('Network error');
        return await response.json();
    } catch (error) {
        console.error('Fetch error:', error);
        return null;
    }
}

async function loadVideos(searchQuery = '', append = false) {
    if (!append) {
        showLoading();
        state.currentPage = 0;
        state.allLoaded = false;
        state.currentSearch = searchQuery;
        state.videos = [];
    }

    const params = [];
    params.push(`limit=${PAGE_SIZE}`);
    params.push(`offset=${state.currentPage * PAGE_SIZE}`);
    if (searchQuery) params.push(`search=${encodeURIComponent(searchQuery)}`);

    const url = API_BASE + '/videos?' + params.join('&');
    const data = await fetchJSON(url);

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
    }
}

async function getFavorites() {
    const data = await fetchJSON(API_BASE + '/favorites');
    if (data) {
        renderFavorites(data);
        elements.favoritesModal.classList.add('active');
    }
}

async function getBlacklist() {
    const data = await fetchJSON(API_BASE + '/blacklist');
    if (data) {
        renderBlacklist(data);
        elements.blacklistModal.classList.add('active');
    }
}

async function addBlacklist(path) {
    if (!path) return;
    const result = await fetchJSON(API_BASE + '/blacklist', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path: path })
    });
    if (result && result.success) {
        getBlacklist();
        elements.blacklistInput.value = '';
        loadVideos();
        showToast('Directory added to blacklist', 'success');
    } else {
        showToast(result?.error || 'Failed to add to blacklist', 'error');
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
                <h4>${item.path}</h4>
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

async function toggleFavorite(videoId) {
    if (state.favorites.has(videoId)) {
        await fetchJSON(API_BASE + '/favorites/' + videoId, {
            method: 'DELETE'
        });
        state.favorites.delete(videoId);
        showToast('Removed from favorites', 'info');
    } else {
        await fetchJSON(API_BASE + '/favorites', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ video_id: videoId })
        });
        state.favorites.add(videoId);
        showToast('Added to favorites', 'success');
    }
    updateFavoriteButton();
    renderVideos(state.videos);
}

async function clearHistory() {
    await fetchJSON(API_BASE + '/history', { method: 'DELETE' });
    getHistory();
    showToast('History cleared', 'success');
}

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
                    <div class="video-title" title="${video.title}">${video.title}</div>
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
        const observer = new IntersectionObserver((entries) => {
            entries.forEach(entry => {
                if (entry.isIntersecting) {
                    const img = entry.target;
                    img.src = img.dataset.src;
                    img.removeAttribute('data-src');
                    observer.unobserve(img);
                }
            });
        }, { rootMargin: '200px' });
        lazyImages.forEach(img => observer.observe(img));
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

    /* Load more button */
    let loadMoreBtn = document.getElementById('loadMoreBtn');
    if (!state.allLoaded) {
        if (!loadMoreBtn) {
            loadMoreBtn = document.createElement('button');
            loadMoreBtn.id = 'loadMoreBtn';
            loadMoreBtn.textContent = 'Load More';
            loadMoreBtn.className = 'load-more-btn';
            loadMoreBtn.addEventListener('click', () => {
                loadVideos(state.currentSearch, true);
            });
            elements.videoList.parentNode.appendChild(loadMoreBtn);
        }
        loadMoreBtn.style.display = 'block';
    } else if (loadMoreBtn) {
        loadMoreBtn.style.display = 'none';
    }
}

function renderHistory(history) {
    if (!history || history.length === 0) {
        elements.historyList.innerHTML = '<p>No history yet</p>';
        return;
    }
    elements.historyList.innerHTML = history.map(item => `
        <div class="history-item" data-id="${item.id}" data-video-id="${item.video_id}" data-path="${item.path}" data-position="${item.position || 0}">
            <div class="history-info">
                <h4>${item.title || 'Unknown'}</h4>
                <p>${item.path}</p>
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
        <div class="favorite-item" data-id="${item.video_id}" data-path="${item.path}">
            <div class="favorite-info">
                <h4>${item.title || 'Unknown'}</h4>
                <p>${item.path}</p>
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
}

function playVideo(video, resume = true) {
    state.currentVideo = video;
    elements.videoTitle.textContent = video.title || 'Unknown';
    elements.videoPath.textContent = video.path || '';
    
    elements.videoPlayer.src = '/video/' + video.id;
    elements.videoPlayer.load();
    elements.videoModal.classList.add('active');
    
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
        elements.blacklistModal
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
}

function initEventListeners() {
    elements.searchBtn.addEventListener('click', () => {
        loadVideos(elements.searchInput.value);
    });
    
    elements.searchInput.addEventListener('input', (e) => {
        debouncedSearch(e.target.value);
    });
    
    elements.searchInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            if (state.searchTimeout) clearTimeout(state.searchTimeout);
            loadVideos(elements.searchInput.value);
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
        addBlacklist(elements.blacklistInput.value);
    });
    
    elements.blacklistInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            addBlacklist(elements.blacklistInput.value);
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
        if (e.key === 'Escape') {
            closeTopModal();
        }
        if (e.key === ' ' && elements.videoModal.classList.contains('active')) {
            e.preventDefault();
            if (elements.videoPlayer.paused) {
                elements.videoPlayer.play();
            } else {
                elements.videoPlayer.pause();
            }
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
    
    elements.videoPlayer.addEventListener('pause', () => {
        if (state.currentVideo) {
            const position = Math.floor(elements.videoPlayer.currentTime);
            addToHistory(state.currentVideo.id, position);
        }
    });

    /* Settings */
    elements.settingsBtn.addEventListener('click', openSettings);
    elements.settingsModal.querySelector('.close').addEventListener('click', closeSettings);
    elements.settingsSaveBtn.addEventListener('click', saveSettings);
}

async function init() {
    initEventListeners();
    await loadFavorites();
    await loadVideos();

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

    const config = await fetchJSON(API_BASE + '/config');
    if (config) {
        elements.settingsPort.value = config.port || '';
        settingsSelectedDir = config.scan_directory || '';
        elements.dirSelectedPath.textContent = settingsSelectedDir || 'None';
        const startPath = settingsSelectedDir ? settingsSelectedDir.substring(0, 3) : '';
        browseDir(startPath);
    }
}

function closeSettings() {
    elements.settingsModal.classList.remove('active');
}

async function browseDir(path) {
    const url = API_BASE + '/browse?path=' + encodeURIComponent(path || '');
    const dirs = await fetchJSON(url);
    if (!dirs || !Array.isArray(dirs)) {
        elements.dirList.innerHTML = '<div class="dir-empty">Unable to read directory</div>';
        return;
    }

    if (path) {
        const parts = path.split(/[/\\]/).filter(Boolean);
        let breadcrumb = '<span class="dir-crumb" data-path="">Root</span>';
        let accum = '';
        for (const part of parts) {
            accum += (accum ? '\\' : '') + part;
            breadcrumb += ' <span class="dir-crumb-sep">›</span> <span class="dir-crumb" data-path="' + accum + '">' + part + '</span>';
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
        return '<div class="dir-item' + sel + '" data-path="' + d.path + '"><span class="dir-icon">' + icon + '</span><span class="dir-name">' + d.name + '</span></div>';
    }).join('');

    elements.dirList.querySelectorAll('.dir-item').forEach(item => {
        item.addEventListener('click', () => {
            settingsSelectedDir = item.dataset.path;
            elements.dirSelectedPath.textContent = settingsSelectedDir;
            elements.dirList.querySelectorAll('.dir-item').forEach(i => i.classList.remove('selected'));
            item.classList.add('selected');
            browseDir(settingsSelectedDir);
        });
    });
}

async function saveSettings() {
    const port = parseInt(elements.settingsPort.value) || 0;
    const dir = settingsSelectedDir;

    if (!dir) {
        showSettingsStatus('Please select a video directory', 'error');
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
        setTimeout(() => loadVideos(), 500);
    } else {
        showSettingsStatus('Failed to save settings', 'error');
    }
}

function showSettingsStatus(msg, type) {
    elements.settingsStatus.textContent = msg;
    elements.settingsStatus.className = 'settings-status ' + type;
}

init();