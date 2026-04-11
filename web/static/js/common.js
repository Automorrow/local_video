export const API_BASE = '/api';
export const PAGE_SIZE = 40;

export const state = {
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
    addingBlacklist: false,
    isRandomPlay: false
};

export const elements = {
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
    videoTitle: document.getElementById('videoModalTitle'),
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
    emptySettingsBtn: document.getElementById('emptySettingsBtn'),
    settingsModal: document.getElementById('settingsModal'),
    settingsPort: document.getElementById('settingsPort'),
    dirBreadcrumb: document.getElementById('dirBreadcrumb'),
    dirList: document.getElementById('dirList'),
    dirSelectedPath: document.getElementById('dirSelectedPath'),
    settingsSaveBtn: document.getElementById('settingsSaveBtn'),
    settingsCancelBtn: document.getElementById('settingsCancelBtn'),
    settingsStatus: document.getElementById('settingsStatus'),
    backToTop: document.getElementById('backToTop'),
    bottomNav: document.getElementById('bottomNav'),
    mobileSearchToggle: document.getElementById('mobileSearchToggle'),
    searchWrap: document.getElementById('searchWrap')
};

export function escapeHtml(str) {
    if (typeof str !== 'string') return String(str ?? '');
    return str.replace(/&/g, '&amp;')
              .replace(/</g, '&lt;')
              .replace(/>/g, '&gt;')
              .replace(/"/g, '&quot;')
              .replace(/'/g, '&#39;');
}

export function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    elements.toastContainer.appendChild(toast);
    setTimeout(() => {
        toast.style.animation = 'toastIn 0.3s ease-out reverse';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

export function formatSize(bytes) {
    if (!bytes || bytes === 0) return '0 B';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
}

export function formatPosition(seconds) {
    if (!seconds || seconds === 0) return '0:00';
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
}

export function showLoading() {
    elements.loading.style.display = 'block';
    elements.emptyMessage.style.display = 'none';
    elements.videoList.style.display = 'none';
}

export function hideLoading() {
    elements.loading.style.display = 'none';
}

export function showEmpty() {
    elements.emptyMessage.style.display = 'block';
    elements.videoList.style.display = 'none';
}

export function showVideos() {
    elements.loading.style.display = 'none';
    elements.emptyMessage.style.display = 'none';
    elements.videoList.style.display = 'grid';
}
