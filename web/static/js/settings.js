import { state, elements, escapeHtml, showToast } from './common.js';
import { api } from './api.js';
import { closeModal } from './modal.js';

export let settingsSelectedDir = '';

export function closeSettings() {
    closeModal(elements.settingsModal);
}

export async function openSettings() {
    elements.settingsModal.classList.add('active');
    elements.settingsModal.classList.add('drawer-open');
    elements.settingsStatus.className = 'settings-status';
    elements.settingsStatus.textContent = '';
    document.title = 'Settings - LocalVideoServer';
    if (window.location.hash !== '#settings') history.pushState(null, '', '#settings');

    const config = await api.getConfig();
    if (config) {
        elements.settingsPort.value = config.port || '';
        settingsSelectedDir = config.scan_directory || '';
        elements.dirSelectedPath.textContent = settingsSelectedDir || 'None';
        const startPath = settingsSelectedDir ? settingsSelectedDir.substring(0, 3) : '';
        browseDir(startPath);
    } else {
        browseDir('');
    }
}

export async function browseDir(path) {
    if (!path || path === 'undefined') path = '';
    const dirs = await api.browse(path);
    if (!dirs || !Array.isArray(dirs)) {
        elements.dirList.innerHTML = '<div class="dir-empty">Unable to read directory. Please check the path or permissions.</div>';
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

    let dirHtml = '';
    if (path) {
        const parentSel = path === settingsSelectedDir ? ' selected' : '';
        dirHtml += '<div class="dir-item' + parentSel + '" data-path="' + escapeHtml(path) + '"><span class="dir-icon">📂</span><span class="dir-name">Use this folder</span></div>';
    }

    if (dirs.length === 0) {
        if (!path) {
            elements.dirList.innerHTML = '<div class="dir-empty">No subdirectories</div>';
            return;
        }
    } else {
        dirHtml += dirs.map(d => {
            const icon = d.type === 'drive' ? '💾' : '📁';
            const sel = d.path === settingsSelectedDir ? ' selected' : '';
            return '<div class="dir-item' + sel + '" data-path="' + escapeHtml(d.path) + '"><span class="dir-icon">' + icon + '</span><span class="dir-name">' + escapeHtml(d.name) + '</span></div>';
        }).join('');
    }

    elements.dirList.innerHTML = dirHtml;

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

export async function saveSettings() {
    state.savingSettings = true;
    const port = parseInt(elements.settingsPort.value) || 0;
    const dir = settingsSelectedDir;

    if (!dir) {
        showSettingsStatus('Please select a video directory', 'error');
        state.savingSettings = false;
        return;
    }

    const result = await api.postConfig(port, dir);

    if (result && result.success) {
        showSettingsStatus('Settings saved! Rescanning videos...', 'success');
        setTimeout(() => {
            closeSettings();
            let attempts = 0;
            const poll = async () => {
                const status = await api.getScanStatus();
                attempts++;
                if (status && !status.scanning) {
                    window.dispatchEvent(new CustomEvent('lv:reloadVideos'));
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
                    window.dispatchEvent(new CustomEvent('lv:reloadVideos'));
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
