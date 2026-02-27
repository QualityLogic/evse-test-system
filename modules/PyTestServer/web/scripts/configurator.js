
const CONFIGURATOR_STATE_KEY = 'configurator-state';

const Configuration = {
    energy: [],
    identity: [],
    protocols: [],
    scenarios: [],
};

const configuratorPopoverList = [];

/**
 * Load the configurator state from local storage.
 * @returns {{energy?: string[], identity?: string[], protocols?: string[], scenarios?: string[]}} The configurator state.
 */
function loadConfiguratorState() {
    const savedState = localStorage.getItem(CONFIGURATOR_STATE_KEY);
    if (savedState) {
        try {
            return JSON.parse(savedState);
        } catch (error) {
            console.error(error);
        }
    }
    return {};
}

/**
 * Save the configurator state to local storage for persistence.
 * @param {object} state The configurator state to save to local storage.
 * @param {string[]} [state.energy] An array of selected energy transfer modes.
 * @param {string[]} [state.identity] An array of selected identification modes.
 * @param {string[]} [state.protocols] An array of selected protocols.
 * @param {string[]} [state.scenarios] An array of selected scenarios.
 */
function saveConfiguratorState(state) {
    localStorage.setItem(CONFIGURATOR_STATE_KEY, JSON.stringify(state));
}

/**
 * Set the `enabled` status of the configurator 'Upload' button.
 * @param {boolean} enable Whether the button should be enabled.
 */
function enableUploadButton(enable) {
    const uploadButton = document.getElementById('configurator_upload__button');
    if (uploadButton && uploadButton.disabled === enable) {
        uploadButton.disabled = !enable;
    }
}

/**
 * Remove all configuration entries from the cache and DOM.
 */
function resetConfigurator() {
    // Disable upload button to prevent corrupted uploads
    enableUploadButton(false);

    // Remove all popover bindings
    configuratorPopoverList.length = 0;

    // Remove all configurator options from the DOM
    for (const item of Configuration.energy)
        item.container.remove();
    for (const item of Configuration.identity)
        item.container.remove();
    for (const item of Configuration.protocols)
        item.container.remove();
    for (const item of Configuration.scenarios)
        item.container.remove();

    // Reset all configuration entries from cache
    Configuration.energy.length = 0;
    Configuration.identity.length = 0;
    Configuration.protocols.length = 0;
    Configuration.scenarios.length = 0;

    // Ensure upload button is still disabled
    enableUploadButton(false);
}

/**
 * Toggle the enabled status of the 'Upload' button based on every
 * category containing at least one option selected.
 */
function refreshUploadButton() {
    const predicate = (item) => item.checkbox.checked;
    const enabled =
        Configuration.energy.some(predicate) &&
        Configuration.identity.some(predicate) &&
        Configuration.protocols.some(predicate) &&
        Configuration.scenarios.some(predicate);
    enableUploadButton(enabled);
}

/**
 *
 * @param {object} data
 * @param {string} data.id
 * @param {string} data.name
 * @param {string} [data.description]
 * @param {boolean} [data.default=false]
 * @param {boolean} [data.disabled=false]
 * @param {boolean} cached
 * @returns {{id: string, container: HTMLDivElement, checkbox: HTMLInputElement}}
 */
function createConfiguratorEntry(data, cached) {
    const containerElem = document.createElement('div');
    const inputElem = document.createElement('input');
    const labelElem = document.createElement('label');

    containerElem.classList.add('form-check');

    inputElem.classList.add('form-check-input');
    inputElem.id = `configurator_${data.id}__checkbox`;
    inputElem.type = 'checkbox';
    inputElem.onclick = refreshUploadButton;
    inputElem.disabled = data.disabled || false;
    inputElem.checked = data.default || (cached && !inputElem.disabled);

    labelElem.classList.add('form-check-label');
    labelElem.textContent = data.name;
    labelElem.setAttribute('for', inputElem.id);

    // Attach a popover if a description was provided
    if (data.description) {
        labelElem.setAttribute('data-bs-toggle', 'popover');
        labelElem.setAttribute('data-bs-trigger', 'hover focus');
        labelElem.setAttribute('data-bs-html', 'true');
        labelElem.setAttribute('data-bs-content', data.description);

        // Create popover binding
        const popover = new bootstrap.Popover(labelElem);
        configuratorPopoverList.push(popover);
    }

    containerElem.appendChild(inputElem);
    containerElem.appendChild(labelElem);

    return {
        id: data.id,
        container: containerElem,
        checkbox: inputElem,
    }
}

async function reloadConfigurator() {
    try {
        resetConfigurator();

        const url = location.href + 'api/v1/configuration';
        const response = await fetch(url);

        if (!response.ok) {
            console.error(`Error reloading configurator: HTTP ${response.status} (${response.statusText})`);
            return;
        }

        const fetchedData = await response.json();
        const sessionData = loadConfiguratorState();

        // Locate configurator categories
        const energyContainer = document.getElementById('configurator_etm__checkboxes');
        const identityContainer = document.getElementById('configurator_ident__checkboxes');
        const protocolContainer = document.getElementById('configurator_proto__checkboxes');
        const scenarioContainer = document.getElementById('configurator_scenario__checkboxes');

        // Energy Transfer Mode
        if (energyContainer) {
            for (const item of fetchedData['charge_modes']) {
                const cached = sessionData.energy?.includes(item.id) || false;
                const entry = createConfiguratorEntry(item, cached);
                Configuration.energy.push(entry);
                energyContainer.appendChild(entry.container);
            }
        }

        // Identification Mode
        if (identityContainer) {
            for (const item of fetchedData['identity']) {
                const cached = sessionData.identity?.includes(item.id) || false;
                const entry = createConfiguratorEntry(item, cached);
                Configuration.identity.push(entry);
                identityContainer.appendChild(entry.container);
            }
        }

        // Application Protocol
        if (protocolContainer) {
            for (const item of fetchedData['protocols']) {
                const cached = sessionData.protocols?.includes(item.id) || false;
                const entry = createConfiguratorEntry(item, cached);
                Configuration.protocols.push(entry);
                protocolContainer.appendChild(entry.container);
            }
        }

        // Scenarios
        if (scenarioContainer) {
            for (const item of fetchedData['scenarios']) {
                const cached = sessionData.scenarios?.includes(item.id) || false;
                const entry = createConfiguratorEntry(item, cached);
                Configuration.scenarios.push(entry);
                scenarioContainer.appendChild(entry.container);
            }
        }

        refreshUploadButton();

    } catch (error) {
        console.error(error);
    }
}

async function uploadConfiguration() {
    const state = {
        energy: Configuration.energy.filter(entry => entry.checkbox.checked).map(entry => entry.id),
        identity: Configuration.identity.filter(entry => entry.checkbox.checked).map(entry => entry.id),
        protocols: Configuration.protocols.filter(entry => entry.checkbox.checked).map(entry => entry.id),
        scenarios: Configuration.scenarios.filter(entry => entry.checkbox.checked).map(entry => entry.id),
    };
    saveConfiguratorState(state);

    const url = location.href + 'api/v1/configuration';
    const response = await fetch(url, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(state),
    });

    if (!response.ok) {
        console.error(`Error uploading configuration: HTTP ${response.status} (${response.statusText})`);
    }
}

(async () => {
    await reloadConfigurator();
    // TODO: Refresh popover logic
})();
