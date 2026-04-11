import { state, elements, formatPosition, showToast } from './common.js';
import { api } from './api.js';
import { closeModal, closeTopModal } from './modal.js';

export function playVideo(video, resume = true, isRandom = false) {
    state.currentVideo = video;
    state.isRandomPlay = isRandom;
    elements.videoTitle.textContent = video.title || 'Unknown';
    elements.videoPath.textContent = video.path || '';

    updateFavoriteButton();
    api.postHistory(video.id, 0);

    elements.videoPlayer.onerror = () => {
        showToast('视频无法播放，可能已被删除或加入黑名单', 'error');
        closeModal(elements.videoModal);
        window.dispatchEvent(new CustomEvent('lv:reloadVideos'));
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

export function playNextVideo() {
    if (!state.currentVideo || state.videos.length === 0) return;
    const currentIndex = state.videos.findIndex(v => v.id === state.currentVideo.id);
    if (currentIndex >= 0 && currentIndex < state.videos.length - 1) {
        playVideo(state.videos[currentIndex + 1]);
    } else if (state.videos.length > 0) {
        playVideo(state.videos[0]);
    }
}

export function showResumeDialog(video) {
    state.pendingVideo = video;
    elements.resumeVideoTitle.textContent = video.title || 'Unknown';
    elements.resumePosition.textContent = `Last played at: ${formatPosition(video.position)}`;
    elements.resumeModal.classList.add('active');
    if (!window.location.hash) history.pushState(null, '', '#resume');
}

export function updateFavoriteButton() {
    if (state.currentVideo && state.favorites.has(state.currentVideo.id)) {
        elements.toggleFavoriteBtn.textContent = 'Remove from Favorites';
        elements.toggleFavoriteBtn.classList.add('active');
    } else {
        elements.toggleFavoriteBtn.textContent = 'Add to Favorites';
        elements.toggleFavoriteBtn.classList.remove('active');
    }
}

export function bindPlayerEvents() {
    elements.toggleFavoriteBtn.addEventListener('click', () => {
        if (state.currentVideo) {
            window.dispatchEvent(new CustomEvent('lv:toggleFavorite', { detail: { videoId: state.currentVideo.id } }));
        }
    });
    elements.playRandomBtn.addEventListener('click', async () => {
        const data = await api.getRandom();
        if (data && data.length > 0) playVideo(data[0], true, true);
    });
    elements.videoPlayer.addEventListener('ended', () => {
        if (state.currentVideo) {
            api.postHistory(state.currentVideo.id, Math.floor(elements.videoPlayer.currentTime));
        }
        if (state.isRandomPlay) {
            api.getRandom().then(data => {
                if (data && data.length > 0) playVideo(data[0], true, true);
            });
        }
    });
    let pauseDebounceTimer = null;
    elements.videoPlayer.addEventListener('pause', () => {
        if (state.currentVideo) {
            const position = Math.floor(elements.videoPlayer.currentTime);
            if (pauseDebounceTimer) clearTimeout(pauseDebounceTimer);
            pauseDebounceTimer = setTimeout(() => {
                api.postHistory(state.currentVideo.id, position);
            }, 500);
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
}
