import { state, elements, PAGE_SIZE, escapeHtml, formatSize, formatPosition, showToast, showLoading, hideLoading, showEmpty, showVideos } from './common.js';
import { api } from './api.js';
import { playVideo, showResumeDialog, bindPlayerEvents, updateFavoriteButton } from './player.js';
import { openSettings, browseDir, saveSettings, settingsSelectedDir } from './settings.js';
import { closeModal, closeTopModal, initModalGestures } from './modal.js';

let _thumbnailObserver = null;

export function loadVideos(searchQuery = '', append = false) {
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
    api.getVideos(PAGE_SIZE, state.currentPage * PAGE_SIZE, searchQuery).then(data => {
        if (mySerial !== state.loadSerial) return;
        if (data && Array.isArray(data)) {
            if (data.length < PAGE_SIZE) state.allLoaded = true;
            state.videos = append ? state.videos.concat(data) : data;
            state.currentPage++;
            renderVideos(state.videos, append);
        } else if (!append) {
            showEmpty();
        }
    });
}

export function debouncedSearch(query) {
    if (state.searchTimeout) clearTimeout(state.searchTimeout);
    state.searchTimeout = setTimeout(() => loadVideos(query), 300);
}

export async function loadFavorites() {
    const data = await api.getFavorites();
    if (data && Array.isArray(data)) {
        state.favorites = new Set(data.map(f => f.video_id));
    }
}

async function getRandomVideo() {
    const data = await api.getRandom();
    if (data && data.length > 0) playVideo(data[0], true, true);
}

async function getHistory() {
    const data = await api.getHistory();
    if (data) {
        renderHistory(data);
        elements.historyModal.classList.add('active');
        elements.historyModal.classList.add('drawer-open');
        if (!window.location.hash) history.pushState(null, '', '#history');
    }
}

async function getFavorites() {
    const data = await api.getFavorites();
    if (data) {
        renderFavorites(data);
        elements.favoritesModal.classList.add('active');
        elements.favoritesModal.classList.add('drawer-open');
        if (!window.location.hash) history.pushState(null, '', '#favorites');
    }
}

async function getBlacklist() {
    const data = await api.getBlacklist();
    if (data) {
        renderBlacklist(data);
        elements.blacklistModal.classList.add('active');
        elements.blacklistModal.classList.add('drawer-open');
        if (!window.location.hash) history.pushState(null, '', '#blacklist');
    }
}

async function addBlacklist(path) {
    if (!path) return;
    state.addingBlacklist = true;
    const result = await api.postBlacklist(path);
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
    await api.deleteBlacklist(id);
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

async function removeFromHistory(historyId) {
    await api.deleteHistory(historyId);
    getHistory();
    showToast('History item removed', 'success');
}

export function updateVideoCardFavorite(videoId, isFavorite) {
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

export async function toggleFavorite(videoId) {
    if (state.togglingFavorite.has(videoId)) return;
    state.togglingFavorite.add(videoId);
    const isFavorite = state.favorites.has(videoId);
    let result;
    if (isFavorite) {
        result = await api.deleteFavorite(videoId);
        if (result && !result.error) {
            state.favorites.delete(videoId);
            showToast('Removed from favorites', 'info');
        }
    } else {
        result = await api.postFavorite(videoId);
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
    await api.deleteAllHistory();
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

    elements.videoList.querySelectorAll('.video-card').forEach(card => {
        if (card._bound) return;
        card._bound = true;
        card.addEventListener('click', () => {
            const video = state.videos.find(v => v.id == card.dataset.id);
            if (video) playVideo(video);
        });
    });

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
                elements.historyModal.classList.remove('drawer-open');
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
                elements.favoritesModal.classList.remove('drawer-open');
                playVideo(video);
            }
        });
    });
}

function initEventListeners() {
    elements.searchBtn.addEventListener('click', () => loadVideos(elements.searchInput.value));
    elements.searchInput.addEventListener('input', (e) => debouncedSearch(e.target.value));
    elements.searchInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            if (state.searchTimeout) clearTimeout(state.searchTimeout);
            loadVideos(elements.searchInput.value.trim());
        }
    });
    elements.randomBtn.addEventListener('click', getRandomVideo);
    elements.historyBtn.addEventListener('click', () => { closeTopModal(); getHistory(); });
    elements.favoritesBtn.addEventListener('click', () => { closeTopModal(); getFavorites(); });
    elements.blacklistBtn.addEventListener('click', () => { closeTopModal(); getBlacklist(); });
    elements.allVideosBtn.addEventListener('click', () => {
        closeTopModal();
        elements.searchInput.value = '';
        loadVideos();
    });
    elements.clearHistoryBtn.addEventListener('click', clearHistory);
    elements.addBlacklistBtn.addEventListener('click', () => {
        if (state.addingBlacklist) return;
        addBlacklist(elements.blacklistInput.value.trim());
    });
    elements.blacklistInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') addBlacklist(elements.blacklistInput.value.trim());
    });
    document.querySelectorAll('.close').forEach(btn => {
        btn.addEventListener('click', closeTopModal);
    });
    window.addEventListener('click', (e) => {
        if (e.target.classList.contains('modal')) closeTopModal();
    });
    elements.settingsBtn.addEventListener('click', () => { closeTopModal(); openSettings(); });
    if (elements.emptySettingsBtn) {
        elements.emptySettingsBtn.addEventListener('click', openSettings);
    }
    elements.settingsSaveBtn.addEventListener('click', () => {
        if (state.savingSettings) return;
        saveSettings();
    });
    if (elements.settingsCancelBtn) {
        elements.settingsCancelBtn.addEventListener('click', closeTopModal);
    }
    document.querySelectorAll('.mobile-back').forEach(btn => {
        btn.addEventListener('click', closeTopModal);
    });

    if (elements.mobileSearchToggle && elements.searchWrap) {
        elements.mobileSearchToggle.addEventListener('click', () => {
            elements.searchWrap.classList.toggle('open');
            if (elements.searchWrap.classList.contains('open')) {
                elements.searchInput.focus();
            }
        });
    }

    if (elements.bottomNav) {
        elements.bottomNav.querySelectorAll('button').forEach(btn => {
            btn.addEventListener('click', () => {
                const view = btn.dataset.view;
                if (view === 'home') {
                    closeTopModal();
                    elements.searchInput.value = '';
                    loadVideos();
                    window.scrollTo({ top: 0, behavior: 'smooth' });
                } else if (view === 'favorites') {
                    closeTopModal();
                    getFavorites();
                } else if (view === 'history') {
                    closeTopModal();
                    getHistory();
                } else if (view === 'settings') {
                    closeTopModal();
                    openSettings();
                }
            });
        });
    }

    window.addEventListener('lv:reloadVideos', () => loadVideos());
    window.addEventListener('lv:toggleFavorite', (e) => toggleFavorite(e.detail.videoId));

    if (elements.backToTop) {
        window.addEventListener('scroll', () => {
            if (window.scrollY > 300) {
                elements.backToTop.classList.add('visible');
            } else {
                elements.backToTop.classList.remove('visible');
            }
        }, { passive: true });
        elements.backToTop.addEventListener('click', () => {
            window.scrollTo({ top: 0, behavior: 'smooth' });
        });
    }
}

async function init() {
    initEventListeners();
    bindPlayerEvents();
    initModalGestures();
    await loadFavorites();
    await loadVideos();

    window.addEventListener('hashchange', () => {
        if (!window.location.hash) {
            closeTopModal();
        } else if (window.location.hash === '#settings') {
            openSettings();
        }
    });

    if (window.location.hash === '#settings') {
        openSettings();
    }
}

init();
