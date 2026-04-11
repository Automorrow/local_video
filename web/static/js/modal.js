import { state, elements } from './common.js';

export function closeModal(modal) {
    modal.classList.remove('active');
    modal.classList.remove('drawer-open');
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

export function closeTopModal() {
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

export function initModalGestures() {
    const drawers = [elements.historyModal, elements.favoritesModal, elements.blacklistModal];
    drawers.forEach(modal => {
        const content = modal.querySelector('.modal-content');
        if (!content) return;
        let startY = 0;
        let currentY = 0;
        let isDragging = false;

        content.addEventListener('touchstart', (e) => {
            startY = e.touches[0].clientY;
            isDragging = true;
            content.style.transition = 'none';
        }, { passive: true });

        content.addEventListener('touchmove', (e) => {
            if (!isDragging) return;
            currentY = e.touches[0].clientY;
            const delta = currentY - startY;
            if (delta > 0) {
                content.style.transform = `translateY(${delta}px)`;
            }
        }, { passive: true });

        content.addEventListener('touchend', () => {
            if (!isDragging) return;
            isDragging = false;
            const delta = currentY - startY;
            content.style.transition = 'transform 0.2s ease-out';
            if (delta > 80) {
                content.style.transform = 'translateY(100%)';
                setTimeout(() => {
                    closeModal(modal);
                    content.style.transform = '';
                }, 200);
            } else {
                content.style.transform = '';
            }
        });
    });
}
