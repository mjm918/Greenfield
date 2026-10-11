// Restore the existing save file before main can load or overwrite it.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
    addRunDependency('greenfield-save');
    Module['greenfieldStorageReady'] = false;
    const restored = function (error) {
        Module['greenfieldStorageReady'] = !error;
        if (error) {
            console.warn('Greenfield browser storage is unavailable:', error);
            document.getElementById('storage-status').textContent =
                'Saving unavailable. Allow site storage and reload to keep your village.';
        }
        removeRunDependency('greenfield-save');
    };
    try {
        FS.mkdir('/greenfield-save');
        FS.mount(IDBFS, {}, '/greenfield-save');
        FS.syncfs(true, restored);
    } catch (error) {
        restored(error);
    }
});

// A pending IndexedDB write cannot be completed synchronously on tab close.
window.addEventListener('beforeunload', function (event) {
    if (Module['greenfieldSaving']) {
        event.preventDefault();
        event.returnValue = '';
    }
});
